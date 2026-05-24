#pragma once
#include <stdint.h>
#include <arpa/inet.h>

#define RTP_VERSION      2
#define RTP_HEADER_SIZE  12
#define MAX_RTP_PACKET   2048
#define DEFAULT_TIMEOUT_MS  200

/*
 * RFC 3550 RTP fixed header
 * Fields are stored in network byte order on the wire.
 * We parse them manually to avoid bitfield endian issues.
 */
typedef struct {
    uint8_t  octet0;   /* version(2) | p(1) | x(1) | cc(4) */
    uint8_t  octet1;   /* marker(1)  | pt(7)                */
    uint16_t seq;      /* sequence number (network order)   */
    uint32_t timestamp;
    uint32_t ssrc;
} __attribute__((packed)) rtp_header_t;

/* Accessors - always parse from raw wire bytes */
static inline int      rtp_version(const rtp_header_t *h) { return (h->octet0 >> 6) & 0x3; }
static inline int      rtp_marker (const rtp_header_t *h) { return (h->octet1 >> 7) & 0x1; }
static inline int      rtp_pt     (const rtp_header_t *h) { return  h->octet1       & 0x7f; }
static inline uint16_t rtp_seq    (const rtp_header_t *h) { return ntohs(h->seq); }
static inline uint32_t rtp_ts     (const rtp_header_t *h) { return ntohl(h->timestamp); }
static inline uint32_t rtp_ssrc   (const rtp_header_t *h) { return ntohl(h->ssrc); }
