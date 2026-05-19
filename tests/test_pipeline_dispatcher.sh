#!/bin/sh
set -eu

PIPELINE_DISPATCHER=${PIPELINE_DISPATCHER:-./build/pipeline_dispatcher}
TMP_DIR=$(mktemp -d)
trap 'rm -rf "$TMP_DIR"' EXIT

check_eq() {
    name=$1
    expected=$2
    actual=$3
    if [ "$expected" != "$actual" ]; then
        printf 'FAIL %s\nexpected: %s\nactual:   %s\n' "$name" "$expected" "$actual" >&2
        exit 1
    fi
}

set +e
"$PIPELINE_DISPATCHER" >"$TMP_DIR/missing.out" 2>"$TMP_DIR/missing.err"
rc=$?
set -e
check_eq "missing args exit" "2" "$rc"
check_eq "missing args stdout" "" "$(cat "$TMP_DIR/missing.out")"

set +e
"$PIPELINE_DISPATCHER" sess_missing "$TMP_DIR/nope" "$TMP_DIR/missing.db" 300 >"$TMP_DIR/fail.out" 2>"$TMP_DIR/fail.err"
rc=$?
set -e
check_eq "child failure exit" "254" "$rc"

SESSION=sess_dispatch
: >"$TMP_DIR/$SESSION.bin"
(
    cd /tmp
    "$PIPELINE_DISPATCHER" "$SESSION" "$TMP_DIR" "$TMP_DIR/clips.db" 300 >"$TMP_DIR/ok.out" 2>"$TMP_DIR/ok.err"
) &
pid=$!
sleep 0.1
printf '%s' '{"type":"clip","session_id":"sess_dispatch","ts":7,"path":"/tmp/seven.mp4"}' >>"$TMP_DIR/$SESSION.bin"
touch "$TMP_DIR/.pipeline_end"
wait "$pid"

check_eq "dispatcher stdout" "" "$(cat "$TMP_DIR/ok.out")"
check_eq "dispatcher db" "sess_dispatch:7	/tmp/seven.mp4" "$(cut -f1,2 "$TMP_DIR/clips.db")"

printf 'OK: all pipeline_dispatcher tests passed\n'
