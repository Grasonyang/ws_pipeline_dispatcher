#include "gap_fsm.h"
#include <stdio.h>
#include <string.h>

/* ------------------------------------------------------------------ *
 * Helpers
 * ------------------------------------------------------------------ */

/* Elapsed milliseconds since a recorded timespec */
static long elapsed_ms(const struct timespec *since)
{
    struct timespec now;
    clock_gettime(CLOCK_MONOTONIC, &now);
    long ms = (now.tv_sec  - since->tv_sec)  * 1000L
            + (now.tv_nsec - since->tv_nsec) / 1000000L;
    return ms;
}

/*
 * RFC 3550 sequence-number comparison using uint16 arithmetic.
 * Returns 1 if `a` is strictly after `b` (within a half-window).
 * This correctly handles the 0xFFFF → 0x0000 wrap-around.
 */
static inline int seq_gt(uint16_t a, uint16_t b)
{
    return (uint16_t)(a - b) < 0x8000u && a != b;
}

static inline int seq_lt(uint16_t a, uint16_t b)
{
    return seq_gt(b, a);
}

/* Record a gap in the history ring */
static void record_gap(gap_fsm_t *fsm, uint16_t start, uint16_t end, int recovered)
{
    gap_record_t *r = &fsm->history[fsm->history_idx % MAX_GAP_HISTORY];
    r->start     = start;
    r->end       = end;
    r->recovered = recovered;
    fsm->history_idx++;
}

/* Colour helpers for terminal output */
#define ANSI_RED    "\033[31m"
#define ANSI_YEL    "\033[33m"
#define ANSI_GRN    "\033[32m"
#define ANSI_CYN    "\033[36m"
#define ANSI_RST    "\033[0m"

/* ------------------------------------------------------------------ *
 * Public API
 * ------------------------------------------------------------------ */

void gap_fsm_init(gap_fsm_t *fsm, int timeout_ms)
{
    memset(fsm, 0, sizeof(*fsm));
    fsm->state       = GAP_STATE_NORMAL;
    fsm->timeout_ms  = timeout_ms;
    fsm->initialized = 0;
    printf(ANSI_CYN "[FSM] initialized, timeout=%d ms\n" ANSI_RST, timeout_ms);
}

const char *gap_state_str(gap_state_t s)
{
    switch (s) {
    case GAP_STATE_NORMAL:     return "NORMAL";
    case GAP_STATE_WAITING:    return "WAITING";
    case GAP_STATE_RECOVERING: return "RECOVERING";
    default:                   return "UNKNOWN";
    }
}

/*
 * gap_fsm_feed() - main entry point, called for every received RTP packet.
 *
 * NORMAL state logic:
 *   - First packet ever     → bootstrap expected_seq, stay NORMAL
 *   - seq == expected       → continuous stream, bump expected, stay NORMAL
 *   - seq >  expected       → gap detected, transition to WAITING
 *   - seq <  expected       → stale / duplicate, discard, stay NORMAL
 *
 * WAITING state logic:
 *   - seq falls inside the gap range   → late packet filled the hole,
 *                                        go back to NORMAL
 *   - seq is the next expected after gap_end → gap fully passed,
 *                                        no recovery needed, NORMAL
 *   - anything else (future seq)       → store, stay WAITING
 *   (timeout is handled by gap_fsm_tick())
 *
 * RECOVERING state logic:
 *   - arrived here because timeout fired in gap_fsm_tick()
 *   - accept whatever seq comes next as the new baseline, → NORMAL
 */
void gap_fsm_feed(gap_fsm_t *fsm, uint16_t seq)
{
    fsm->pkts_received++;

    switch (fsm->state) {

    /* ============================================================ */
    case GAP_STATE_NORMAL:
    /* ============================================================ */

        /* First packet: bootstrap expected, no comparison possible */
        if (!fsm->initialized) {
            fsm->expected_seq = (uint16_t)(seq + 1);
            fsm->initialized  = 1;
            printf(ANSI_GRN "[NORMAL] first pkt seq=%-5u  expected_next=%u\n"
                   ANSI_RST, seq, fsm->expected_seq);
            break;
        }

        if (seq == fsm->expected_seq) {
            /* ---- Happy path: perfectly in order ---- */
            fsm->expected_seq = (uint16_t)(seq + 1);
            printf("[NORMAL] ok  seq=%-5u\n", seq);

        } else if (seq_gt(seq, fsm->expected_seq)) {
            /* ---- Gap detected: seq jumped forward ---- */
            fsm->gap_start = fsm->expected_seq;
            fsm->gap_end   = (uint16_t)(seq - 1);
            fsm->total_gaps++;
            clock_gettime(CLOCK_MONOTONIC, &fsm->gap_detected_at);

            /*
             * The packet that exposed the gap (seq) is itself valid.
             * Advance expected past it, then wait for the missing range.
             */
            fsm->expected_seq = (uint16_t)(seq + 1);
            fsm->state = GAP_STATE_WAITING;

            printf(ANSI_YEL
                   "[NORMAL→WAITING] gap detected! missing seq %u..%u "
                   "(exposed by seq=%u)\n"
                   ANSI_RST,
                   fsm->gap_start, fsm->gap_end, seq);

        } else {
            /* ---- Stale / duplicate ---- */
            fsm->pkts_out_of_order++;
            printf(ANSI_YEL
                   "[NORMAL] stale/dup seq=%-5u expected=%u  (discarded)\n"
                   ANSI_RST, seq, fsm->expected_seq);
        }
        break;

    /* ============================================================ */
    case GAP_STATE_WAITING:
    /* ============================================================ */

        /*
         * Check if this packet falls inside the missing range.
         * We consider the gap "filled" if we see any packet in
         * [gap_start .. gap_end].  For a strict full-recovery you
         * would track a bitmap; for live video a single late packet
         * restoring continuity is usually sufficient.
         */
        if (!seq_lt(seq, fsm->gap_start) && !seq_gt(seq, fsm->gap_end)) {
            /* Late packet arrived and fills (part of) the gap */
            fsm->recovered_gaps++;
            long latency = elapsed_ms(&fsm->gap_detected_at);
            record_gap(fsm, fsm->gap_start, fsm->gap_end, 1);
            printf(ANSI_GRN
                   "[WAITING→NORMAL] late pkt seq=%-5u filled gap %u..%u "
                   "(latency=%ld ms)\n"
                   ANSI_RST,
                   seq, fsm->gap_start, fsm->gap_end, latency);
            /*
             * Re-anchor expected to just after the gap_end so we don't
             * re-trigger on the already-received seq past the gap.
             */
            fsm->expected_seq = (uint16_t)(fsm->gap_end + 1);
            fsm->state = GAP_STATE_NORMAL;

        } else if (seq_gt(seq, fsm->gap_end)) {
            /*
             * A future packet arrived before the gap was filled AND
             * before timeout.  This means a second gap opened up.
             * For simplicity: treat first gap as lost, start fresh.
             */
            fsm->dropped_gaps++;
            record_gap(fsm, fsm->gap_start, fsm->gap_end, 0);
            printf(ANSI_RED
                   "[WAITING] new seq=%u arrived, gap %u..%u declared lost "
                   "(nested gap)\n"
                   ANSI_RST,
                   seq, fsm->gap_start, fsm->gap_end);

            /* Start a new gap if needed */
            uint16_t new_expected = (uint16_t)(fsm->gap_end + 1);
            if (seq_gt(seq, new_expected)) {
                fsm->gap_start = new_expected;
                fsm->gap_end   = (uint16_t)(seq - 1);
                fsm->total_gaps++;
                clock_gettime(CLOCK_MONOTONIC, &fsm->gap_detected_at);
                fsm->expected_seq = (uint16_t)(seq + 1);
                printf(ANSI_YEL
                       "[WAITING→WAITING] new gap %u..%u\n"
                       ANSI_RST, fsm->gap_start, fsm->gap_end);
            } else {
                fsm->expected_seq = (uint16_t)(seq + 1);
                fsm->state = GAP_STATE_NORMAL;
            }
        } else {
            /* seq is older than gap_start: stale/dup, ignore */
            fsm->pkts_out_of_order++;
            printf(ANSI_YEL
                   "[WAITING] stale seq=%-5u (gap %u..%u), discarded\n"
                   ANSI_RST, seq, fsm->gap_start, fsm->gap_end);
        }
        break;

    /* ============================================================ */
    case GAP_STATE_RECOVERING:
    /* ============================================================ */

        /*
         * We arrive here from gap_fsm_tick() after timeout.
         * Accept the next packet as the new baseline.
         */
        printf(ANSI_GRN
               "[RECOVERING→NORMAL] resuming at seq=%u\n"
               ANSI_RST, seq);
        fsm->expected_seq = (uint16_t)(seq + 1);
        fsm->state = GAP_STATE_NORMAL;
        break;
    }
}

/*
 * gap_fsm_tick() - call this periodically (e.g. every 10 ms or on each
 * packet receive) to detect timeouts while in WAITING state.
 */
void gap_fsm_tick(gap_fsm_t *fsm)
{
    if (fsm->state != GAP_STATE_WAITING)
        return;

    if (elapsed_ms(&fsm->gap_detected_at) >= fsm->timeout_ms) {
        fsm->dropped_gaps++;
        record_gap(fsm, fsm->gap_start, fsm->gap_end, 0);
        printf(ANSI_RED
               "[WAITING→RECOVERING] TIMEOUT %d ms, gap %u..%u declared lost\n"
               ANSI_RST,
               fsm->timeout_ms, fsm->gap_start, fsm->gap_end);
        fsm->state = GAP_STATE_RECOVERING;
    }
}

void gap_fsm_stats(const gap_fsm_t *fsm)
{
    printf("\n=== Gap FSM Statistics ===\n");
    printf("  State          : %s\n", gap_state_str(fsm->state));
    printf("  Pkts received  : %u\n", fsm->pkts_received);
    printf("  Out-of-order   : %u\n", fsm->pkts_out_of_order);
    printf("  Total gaps     : %u\n", fsm->total_gaps);
    printf("  Recovered      : %u\n", fsm->recovered_gaps);
    printf("  Dropped        : %u\n", fsm->dropped_gaps);
    if (fsm->total_gaps > 0) {
        printf("  Recovery rate  : %.1f%%\n",
               100.0 * fsm->recovered_gaps / fsm->total_gaps);
    }

    int n = fsm->history_idx < MAX_GAP_HISTORY
          ? fsm->history_idx : MAX_GAP_HISTORY;
    if (n > 0) {
        printf("\n  Last %d gap(s):\n", n);
        int start = fsm->history_idx > MAX_GAP_HISTORY
                  ? fsm->history_idx % MAX_GAP_HISTORY : 0;
        for (int i = 0; i < n; i++) {
            const gap_record_t *r =
                &fsm->history[(start + i) % MAX_GAP_HISTORY];
            printf("    seq %5u..%-5u  %s\n",
                   r->start, r->end,
                   r->recovered ? "[RECOVERED]" : "[DROPPED]");
        }
    }
    printf("==========================\n\n");
}
