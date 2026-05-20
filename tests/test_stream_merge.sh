#!/bin/sh
set -eu

STREAM_MERGE=${STREAM_MERGE:-./build/stream_merge}
TMP_DIR=$(mktemp -d)
trap 'rm -rf "$TMP_DIR"' EXIT
SESSION=sess_stream
BIN="$TMP_DIR/$SESSION.bin"
META="$TMP_DIR/$SESSION.meta.jsonl"

check_eq() {
    name=$1
    expected=$2
    actual=$3
    if [ "$expected" != "$actual" ]; then
        printf 'FAIL %s\nexpected: %s\nactual:   %s\n' "$name" "$expected" "$actual" >&2
        exit 1
    fi
}

check_contains() {
    name=$1
    needle=$2
    haystack=$3
    case "$haystack" in
        *"$needle"*) ;;
        *) printf 'FAIL %s\nneedle:   %s\nhaystack: %s\n' "$name" "$needle" "$haystack" >&2
           exit 1 ;;
    esac
}

wait_ready() {
    errfile=$1
    i=0
    while [ $i -lt 100 ]; do
        grep -q "inotify ready" "$errfile" 2>/dev/null && return 0
        sleep 0.01
        i=$((i + 1))
    done
    printf 'FAIL: stream_merge did not signal ready within 1s\n' >&2
    exit 1
}

# ── Test 1: normal 5-second window → one complete clip ─────────────────

: >"$BIN"
: >"$META"

# Use --clip-secs 5 --idle-secs 10 (long idle so the 5s window fires first).
"$STREAM_MERGE" --src "$TMP_DIR" --session "$SESSION" \
    --clip-secs 5 --idle-secs 10 \
    >"$TMP_DIR/out1" 2>"$TMP_DIR/err1" &
pid=$!
wait_ready "$TMP_DIR/err1"

# Three chunks spanning 6 seconds: seq 1 (ts=0ms), seq 2 (ts=3000ms), seq 3 (ts=6000ms).
# seq 3 pushes span to 6000ms >= 5000ms → emit complete clip.
printf '\x00\x01\x02\x03'     >>"$BIN"
printf '{"kind":"data","seq":1,"offset":0,"length":4,"ts_ms":0}\n'       >>"$META"
sleep 0.05
printf '\x04\x05\x06\x07'     >>"$BIN"
printf '{"kind":"data","seq":2,"offset":4,"length":4,"ts_ms":3000}\n'    >>"$META"
sleep 0.05
printf '\x08\x09\x0a\x0b'     >>"$BIN"
printf '{"kind":"data","seq":3,"offset":8,"length":4,"ts_ms":6000}\n'    >>"$META"
sleep 0.05
touch "$TMP_DIR/.pipeline_end"

for _ in 1 2 3 4 5 6 7 8 9 10; do
    sleep 0.1
    kill -0 "$pid" 2>/dev/null || break
done
wait "$pid" || true

line_count=$(grep -c '^{' "$TMP_DIR/out1" || true)
check_eq  "t1 clip count"      "2" "$line_count"
check_eq  "t1 stdout only json" "" "$(grep -v '^{' "$TMP_DIR/out1" || true)"
check_contains "t1 first complete"  '"complete":true'  "$(head -1 "$TMP_DIR/out1")"
check_contains "t1 session_id"      '"session_id":"sess_stream"' "$(head -1 "$TMP_DIR/out1")"
check_contains "t1 type clip"       '"type":"clip"' "$(head -1 "$TMP_DIR/out1")"
check_contains "t1 clip1 offset"    '"offset":0'  "$(head -1 "$TMP_DIR/out1")"
check_contains "t1 clip1 length"    '"length":8'  "$(head -1 "$TMP_DIR/out1")"
check_contains "t1 clip2 offset"    '"offset":8'  "$(sed -n '2p' "$TMP_DIR/out1")"
check_contains "t1 clip2 length"    '"length":4'  "$(sed -n '2p' "$TMP_DIR/out1")"

# ── Test 2: continuity break → partial clip then new clip ───────────────

rm -f "$TMP_DIR/.pipeline_end"
SESSION2=sess_gap
BIN2="$TMP_DIR/$SESSION2.bin"
META2="$TMP_DIR/$SESSION2.meta.jsonl"
: >"$BIN2"
: >"$META2"

"$STREAM_MERGE" --src "$TMP_DIR" --session "$SESSION2" \
    --clip-secs 30 --idle-secs 10 \
    >"$TMP_DIR/out2" 2>"$TMP_DIR/err2" &
pid2=$!
wait_ready "$TMP_DIR/err2"

# seq 1 then seq 5 (gap: expected 2) → partial clip emitted for seq 1 alone.
printf '\x00\x01\x02\x03' >>"$BIN2"
printf '{"kind":"data","seq":1,"offset":0,"length":4,"ts_ms":0}\n'     >>"$META2"
sleep 0.05
printf '\x10\x11\x12\x13' >>"$BIN2"
printf '{"kind":"data","seq":5,"offset":16,"length":4,"ts_ms":1000}\n' >>"$META2"
sleep 0.05
touch "$TMP_DIR/.pipeline_end"

for _ in 1 2 3 4 5 6 7 8 9 10; do
    sleep 0.1
    kill -0 "$pid2" 2>/dev/null || break
done
wait "$pid2" || true

line_count2=$(grep -c '^{' "$TMP_DIR/out2" || true)
check_eq "t2 clip count" "2" "$line_count2"
check_contains "t2 first partial" '"complete":false' "$(head -1 "$TMP_DIR/out2")"
check_eq "t2 stdout only json" "" "$(grep -v '^{' "$TMP_DIR/out2" || true)"

# ── Test 3: idle timeout → partial clip with "complete":false ──────────

rm -f "$TMP_DIR/.pipeline_end"
SESSION3=sess_idle
BIN3="$TMP_DIR/$SESSION3.bin"
META3="$TMP_DIR/$SESSION3.meta.jsonl"
: >"$BIN3"
: >"$META3"

"$STREAM_MERGE" --src "$TMP_DIR" --session "$SESSION3" \
    --clip-secs 30 --idle-secs 0.2 \
    >"$TMP_DIR/out3" 2>"$TMP_DIR/err3" &
pid3=$!
wait_ready "$TMP_DIR/err3"

# Write one chunk then wait longer than idle-secs (0.2s → 200ms).
printf '\x00\x01\x02\x03' >>"$BIN3"
printf '{"kind":"data","seq":1,"offset":0,"length":4,"ts_ms":0}\n' >>"$META3"
sleep 0.4

touch "$TMP_DIR/.pipeline_end"

for _ in 1 2 3 4 5 6 7 8 9 10; do
    sleep 0.1
    kill -0 "$pid3" 2>/dev/null || break
done
wait "$pid3" || true

line_count3=$(grep -c '^{' "$TMP_DIR/out3" || true)
check_eq "t3 clip count"       "1" "$line_count3"
check_contains "t3 idle partial" '"complete":false' "$(head -1 "$TMP_DIR/out3")"
check_eq "t3 stdout only json"  "" "$(grep -v '^{' "$TMP_DIR/out3" || true)"

printf 'OK: all stream_merge tests passed\n'
