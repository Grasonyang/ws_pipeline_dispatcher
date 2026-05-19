#!/bin/sh
set -eu

CLIP_STORE=${CLIP_STORE:-./build/clip_store}
TMP_DIR=$(mktemp -d)
trap 'rm -rf "$TMP_DIR"' EXIT
DB="$TMP_DIR/clips.db"

check_eq() {
    name=$1
    expected=$2
    actual=$3
    if [ "$expected" != "$actual" ]; then
        printf 'FAIL %s\nexpected: %s\nactual:   %s\n' "$name" "$expected" "$actual" >&2
        exit 1
    fi
}

printf '%s\n' '{"type":"clip","session_id":"sess_001","ts":1747065600,"path":"/tmp/one.mp4"}' |
    "$CLIP_STORE" --db "$DB" --ttl 300 >"$TMP_DIR/write.out" 2>"$TMP_DIR/write.err"
check_eq "pipe write stdout" "" "$(cat "$TMP_DIR/write.out")"
check_eq "pipe write get" "/tmp/one.mp4" "$($CLIP_STORE --db "$DB" --get sess_001:1747065600)"

printf '%s\n' '{"type":"clip","session_id":"sess_001","ts":1747065600,"path":"/tmp/two.mp4"}' |
    "$CLIP_STORE" --db "$DB" --ttl 300
check_eq "upsert latest value" "/tmp/two.mp4" "$($CLIP_STORE --db "$DB" --get sess_001:1747065600)"

printf '%s\n' '{"type":"clip","session_id":"sess_001","ts":1747065601,"path":"/tmp/expired.mp4"}' |
    "$CLIP_STORE" --db "$DB" --ttl 0
check_eq "expired get" "" "$($CLIP_STORE --db "$DB" --get sess_001:1747065601)"

"$CLIP_STORE" --db "$DB" --gc
check_eq "gc removes duplicates and expired" "1" "$(wc -l <"$DB" | tr -d ' ')"
check_eq "gc keeps latest" "sess_001:1747065600	/tmp/two.mp4" "$(cut -f1,2 "$DB")"

for i in 1 2 3 4 5; do
    (printf '{"type":"clip","session_id":"sess_c","ts":%s,"path":"/tmp/%s.mp4"}\n' "$i" "$i" |
        "$CLIP_STORE" --db "$DB" --ttl 300) &
done
wait
"$CLIP_STORE" --db "$DB" --gc
check_eq "concurrent writes line count" "6" "$(wc -l <"$DB" | tr -d ' ')"

printf 'OK: all clip_store tests passed\n'
