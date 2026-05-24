#!/usr/bin/env bash
# =============================================================================
# test.sh  —  RTP Gap State Machine Test Script
#
# What this does:
#   1. Builds rtp_server
#   2. Optionally enables kernel-level packet loss via `tc netem`
#   3. Starts rtp_server in background
#   4. Streams a video with ffmpeg (video only, -an)
#   5. Cleans up on exit
#
# Usage:
#   ./test.sh <input.mp4> [loss_percent] [port] [timeout_ms]
#
# Examples:
#   ./test.sh video.mp4            # no loss
#   ./test.sh video.mp4 5          # 5% random packet loss
#   ./test.sh video.mp4 10 5006    # 10% loss, custom port
#   ./test.sh video.mp4 5 5004 300 # 5% loss, 300ms timeout
#
# Requirements:
#   - ffmpeg
#   - gcc / make
#   - iproute2 (for tc netem loss simulation)  — optional
#   - sudo (only needed for tc netem)           — optional
# =============================================================================

set -e

# ---- Arguments ---------------------------------------------------------------
INPUT="${1:-}"
LOSS="${2:-0}"
PORT="${3:-5004}"
TIMEOUT_MS="${4:-200}"

if [[ -z "$INPUT" ]]; then
    echo "Usage: $0 <input.mp4> [loss_percent] [port] [timeout_ms]"
    exit 1
fi

if [[ ! -f "$INPUT" ]]; then
    echo "Error: file not found: $INPUT"
    exit 1
fi

# ---- Dependency check --------------------------------------------------------
for cmd in ffmpeg gcc make; do
    if ! command -v "$cmd" &>/dev/null; then
        echo "Error: '$cmd' not found. Please install it first."
        exit 1
    fi
done

# ---- Build -------------------------------------------------------------------
echo "=== Building rtp_server ==="
cd "$(dirname "$0")"
make -s
echo "Build OK."
echo

# ---- Network loss simulation via tc netem ------------------------------------
NETEM_ACTIVE=0
NETEM_IFACE="lo"

setup_netem() {
    if [[ "$LOSS" == "0" || "$LOSS" == "0.0" ]]; then
        return
    fi
    if ! command -v tc &>/dev/null; then
        echo "[warn] 'tc' not found, skipping loss simulation"
        return
    fi
    echo "=== Setting up ${LOSS}% packet loss on ${NETEM_IFACE} (netem) ==="
    # Remove any existing qdisc first (ignore error if none exists)
    sudo tc qdisc del dev "$NETEM_IFACE" root 2>/dev/null || true
    sudo tc qdisc add dev "$NETEM_IFACE" root netem loss "${LOSS}%"
    NETEM_ACTIVE=1
    echo "Netem loss=${LOSS}% active on ${NETEM_IFACE}"
    echo
}

teardown_netem() {
    if [[ "$NETEM_ACTIVE" -eq 1 ]]; then
        echo
        echo "=== Removing netem qdisc ==="
        sudo tc qdisc del dev "$NETEM_IFACE" root 2>/dev/null || true
        NETEM_ACTIVE=0
    fi
}

# ---- Cleanup handler ---------------------------------------------------------
SERVER_PID=""

cleanup() {
    echo
    echo "=== Cleaning up ==="

    if [[ -n "$SERVER_PID" ]] && kill -0 "$SERVER_PID" 2>/dev/null; then
        echo "Stopping rtp_server (pid=$SERVER_PID)"
        kill "$SERVER_PID" 2>/dev/null || true
        wait "$SERVER_PID" 2>/dev/null || true
    fi

    teardown_netem
    echo "Done."
}
trap cleanup EXIT INT TERM

# ---- Start server ------------------------------------------------------------
echo "=== Starting rtp_server on port $PORT (timeout=${TIMEOUT_MS}ms) ==="
./rtp_server "$PORT" "$TIMEOUT_MS" &
SERVER_PID=$!
echo "Server PID: $SERVER_PID"
sleep 0.5   # give server time to bind

# ---- Setup loss --------------------------------------------------------------
setup_netem

# ---- Stream with ffmpeg ------------------------------------------------------
echo
echo "=== Streaming '$INPUT' via ffmpeg (video only, real-time) ==="
echo "    ffmpeg -re -i $INPUT -an -c:v copy -f rtp rtp://127.0.0.1:$PORT"
echo "    Press Ctrl-C to stop early."
echo

# -re          : read at native frame rate (simulate live capture)
# -an          : no audio
# -c:v copy    : pass-through, no re-encode
# -f rtp       : RTP muxer
# -loglevel warning: suppress ffmpeg noise, show only warnings
ffmpeg -re \
       -i "$INPUT" \
       -an \
       -c:v copy \
       -f rtp \
       -loglevel warning \
       "rtp://127.0.0.1:${PORT}"

echo
echo "=== ffmpeg finished ==="

# Give the server a moment to flush its last output
sleep 0.3
