#!/usr/bin/env bash
set -euo pipefail
cd "$(dirname "$0")"
DEMO_PORT="${1:-8000}"
if [[ ! "$DEMO_PORT" =~ ^[0-9]+$ ]] || (( DEMO_PORT < 1 || DEMO_PORT > 65535 )); then
    echo 'Usage: ./run-demo.sh [port between 1 and 65535]' >&2
    exit 1
fi
command -v cmake >/dev/null || { echo 'Install CMake first.' >&2; exit 1; }
command -v python3 >/dev/null || { echo 'Install Python 3 first.' >&2; exit 1; }
cmake -S . -B build
cmake --build build
DEMO_SERVER_LOG=$(mktemp)
DEMO_SERVER_PID=''
DEMO_ENGINE_PID=''
cleanup() {
    if [[ -n "$DEMO_ENGINE_PID" ]]; then
        kill -TERM "$DEMO_ENGINE_PID" 2>/dev/null || true
        wait "$DEMO_ENGINE_PID" 2>/dev/null || true
    fi
    if [[ -n "$DEMO_SERVER_PID" ]]; then
        kill "$DEMO_SERVER_PID" 2>/dev/null || true
        wait "$DEMO_SERVER_PID" 2>/dev/null || true
    fi
    rm -f "$DEMO_SERVER_LOG"
}
trap cleanup EXIT
trap 'exit 130' INT
trap 'exit 143' TERM
python3 -m http.server "$DEMO_PORT" --bind 127.0.0.1 --directory frontend >"$DEMO_SERVER_LOG" 2>&1 &
DEMO_SERVER_PID=$!
sleep 0.3
if ! kill -0 "$DEMO_SERVER_PID" 2>/dev/null; then
    cat "$DEMO_SERVER_LOG" >&2
    echo 'Could not start dashboard server. Try a different port.' >&2
    exit 1
fi
echo "Dashboard: http://127.0.0.1:$DEMO_PORT"
echo 'Continuous synthetic feed. Press Ctrl+C to stop both processes.'
./build/order_book_replay --demo --snapshot frontend/state.json > /dev/null &
DEMO_ENGINE_PID=$!
wait "$DEMO_ENGINE_PID"
