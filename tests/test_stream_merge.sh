#!/bin/sh
set -eu

STREAM_MERGE=${STREAM_MERGE:-./build/stream_merge}
TMP_DIR=$(mktemp -d)
trap 'rm -rf "$TMP_DIR"' EXIT
SESSION=sess_stream
BIN="$TMP_DIR/$SESSION.bin"
: >"$BIN"

check_eq() {
    name=$1
    expected=$2
    actual=$3
    if [ "$expected" != "$actual" ]; then
        printf 'FAIL %s\nexpected: %s\nactual:   %s\n' "$name" "$expected" "$actual" >&2
        exit 1
    fi
}

"$STREAM_MERGE" --src "$TMP_DIR" --session "$SESSION" >"$TMP_DIR/out" 2>"$TMP_DIR/err" &
pid=$!

printf '{"type":"clip","session_id":"sess_stream","ts":1,' >>"$BIN"
sleep 0.1
printf '"path":"/tmp/one.mp4"}' >>"$BIN"
printf '{"type":"heartbeat","session_id":"sess_stream","ts":2}' >>"$BIN"
printf '{"type":"clip","session_id":"sess_stream","ts":3,"path":"/tmp/two.mp4"}' >>"$BIN"
touch "$TMP_DIR/.pipeline_end"

for _ in 1 2 3 4 5 6 7 8 9 10; do
    if wait "$pid"; then
        break
    fi
    sleep 0.1
done

expected='{"type":"clip","session_id":"sess_stream","ts":1,"path":"/tmp/one.mp4"}
{"type":"clip","session_id":"sess_stream","ts":3,"path":"/tmp/two.mp4"}'
check_eq "stream output" "$expected" "$(cat "$TMP_DIR/out")"
check_eq "stream stdout only json" "" "$(grep -v '^{' "$TMP_DIR/out" || true)"

printf 'OK: all stream_merge tests passed\n'
