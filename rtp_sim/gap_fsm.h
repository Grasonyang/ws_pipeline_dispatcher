#pragma once
#include <stdint.h>
#include <time.h>

/* ------------------------------------------------------------------ *
 * States
 * ------------------------------------------------------------------ */
typedef enum {
    GAP_STATE_NORMAL,      /* seq continuous, all good            */
    GAP_STATE_WAITING,     /* gap detected, waiting for late pkts */
    GAP_STATE_RECOVERING,  /* timeout expired, skip & resume      */
} gap_state_t;

/* ------------------------------------------------------------------ *
 * Per-gap record (kept for logging / stats)
 * ------------------------------------------------------------------ */
typedef struct {
    uint16_t start;        /* first missing seq                   */
    uint16_t end;          /* last  missing seq                   */
    int      recovered;    /* 1 = late pkt filled it, 0 = dropped */
} gap_record_t;

#define MAX_GAP_HISTORY 64

/* ------------------------------------------------------------------ *
 * State machine context
 * ------------------------------------------------------------------ */
typedef struct {
    gap_state_t      state;
    int              initialized;    /* 0 until first packet seen   */
    uint16_t         expected_seq;

    /* current gap (valid in WAITING / RECOVERING) */
    uint16_t         gap_start;
    uint16_t         gap_end;
    struct timespec  gap_detected_at;
    int              timeout_ms;

    /* statistics */
    uint32_t         pkts_received;
    uint32_t         pkts_out_of_order;
    uint32_t         total_gaps;
    uint32_t         recovered_gaps;  /* late pkt arrived in time   */
    uint32_t         dropped_gaps;    /* timeout → skip             */

    /* history ring */
    gap_record_t     history[MAX_GAP_HISTORY];
    int              history_idx;
} gap_fsm_t;

/* ------------------------------------------------------------------ *
 * API
 * ------------------------------------------------------------------ */
void        gap_fsm_init   (gap_fsm_t *fsm, int timeout_ms);
void        gap_fsm_feed   (gap_fsm_t *fsm, uint16_t seq);
void        gap_fsm_tick   (gap_fsm_t *fsm);   /* call periodically  */
void        gap_fsm_stats  (const gap_fsm_t *fsm);
const char *gap_state_str  (gap_state_t s);
