/*
 * rtp_server.c
 *
 * Receives RTP video packets over UDP, runs the gap state machine,
 * and prints per-packet diagnostics + final statistics.
 *
 * Usage:  ./rtp_server [port] [timeout_ms]
 *         defaults: port=5004  timeout=200ms
 *
 * Designed to receive from:
 *   ffmpeg -re -i input.mp4 -an -c:v copy -f rtp rtp://127.0.0.1:5004
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <signal.h>
#include <unistd.h>
#include <errno.h>
#include <arpa/inet.h>
#include <sys/socket.h>
#include <sys/select.h>

#include "rtp.h"
#include "gap_fsm.h"

/* ------------------------------------------------------------------ */
static volatile int g_running = 1;

static void on_signal(int sig)
{
    (void)sig;
    g_running = 0;
}

/* ------------------------------------------------------------------ */
static int create_udp_socket(int port)
{
    int fd = socket(AF_INET, SOCK_DGRAM, 0);
    if (fd < 0) { perror("socket"); return -1; }

    int reuse = 1;
    setsockopt(fd, SOL_SOCKET, SO_REUSEADDR, &reuse, sizeof(reuse));

    /* Increase receive buffer to 4 MB to avoid kernel drop */
    int rcvbuf = 4 * 1024 * 1024;
    setsockopt(fd, SOL_SOCKET, SO_RCVBUF, &rcvbuf, sizeof(rcvbuf));

    struct sockaddr_in addr;
    memset(&addr, 0, sizeof(addr));
    addr.sin_family      = AF_INET;
    addr.sin_port        = htons((uint16_t)port);
    addr.sin_addr.s_addr = INADDR_ANY;

    if (bind(fd, (struct sockaddr *)&addr, sizeof(addr)) < 0) {
        perror("bind");
        close(fd);
        return -1;
    }
    return fd;
}

/* ------------------------------------------------------------------ */
int main(int argc, char *argv[])
{
    int port       = (argc > 1) ? atoi(argv[1]) : 5004;
    int timeout_ms = (argc > 2) ? atoi(argv[2]) : DEFAULT_TIMEOUT_MS;

    signal(SIGINT,  on_signal);
    signal(SIGTERM, on_signal);

    int sock = create_udp_socket(port);
    if (sock < 0) return 1;

    printf("RTP Server listening on UDP port %d  (timeout=%d ms)\n",
           port, timeout_ms);
    printf("Send stream with:\n");
    printf("  ffmpeg -re -i input.mp4 -an -c:v copy -f rtp rtp://127.0.0.1:%d\n\n",
           port);

    gap_fsm_t fsm;
    gap_fsm_init(&fsm, timeout_ms);

    uint8_t  buf[MAX_RTP_PACKET + RTP_HEADER_SIZE];
    struct sockaddr_in sender;
    socklen_t sender_len = sizeof(sender);

    while (g_running) {
        /* Use select() with 50 ms timeout so gap_fsm_tick() fires
         * even when no packets arrive (important for timeout detection) */
        fd_set rfds;
        FD_ZERO(&rfds);
        FD_SET(sock, &rfds);
        struct timeval tv = { .tv_sec = 0, .tv_usec = 50000 };

        int ret = select(sock + 1, &rfds, NULL, NULL, &tv);
        if (ret < 0) {
            if (errno == EINTR) break;
            perror("select");
            break;
        }

        /* Always tick the FSM to catch timeouts */
        gap_fsm_tick(&fsm);

        if (ret == 0) continue; /* select timed out, no data */

        ssize_t n = recvfrom(sock, buf, sizeof(buf), 0,
                             (struct sockaddr *)&sender, &sender_len);
        if (n < 0) {
            if (errno == EINTR) break;
            perror("recvfrom");
            break;
        }

        /* Sanity-check minimum RTP header size */
        if (n < RTP_HEADER_SIZE) {
            fprintf(stderr, "[SERVER] packet too short (%zd bytes), skip\n", n);
            continue;
        }

        rtp_header_t *h = (rtp_header_t *)buf;

        /* Validate RTP version */
        if (rtp_version(h) != RTP_VERSION) {
            fprintf(stderr, "[SERVER] bad RTP version %d, skip\n",
                    rtp_version(h));
            continue;
        }

        uint16_t seq    = rtp_seq(h);
        uint32_t ts     = rtp_ts(h);
        uint8_t  pt     = (uint8_t)rtp_pt(h);
        int      marker = rtp_marker(h);
        int      payload_len = (int)(n - RTP_HEADER_SIZE);

        printf("[RX] seq=%-5u  ts=%-10u  pt=%-3u  len=%-5d  marker=%d  "
               "state=%s\n",
               seq, ts, pt, payload_len, marker,
               gap_state_str(fsm.state));

        /* Feed sequence number into the gap state machine */
        gap_fsm_feed(&fsm, seq);
    }

    printf("\n[SERVER] shutting down...\n");
    gap_fsm_stats(&fsm);
    close(sock);
    return 0;
}
