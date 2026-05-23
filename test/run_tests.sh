#!/usr/bin/env bash
set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
ROOT_DIR="$(cd "$SCRIPT_DIR/.." && pwd)"
SERVER="$ROOT_DIR/build/example_server"
PORT=8080
BASE="http://127.0.0.1:$PORT"
PASS=0
FAIL=0

# ── helpers ──────────────────────────────────────────────────────────────────

die() { echo "ERROR: $*" >&2; exit 1; }

start_server() {
    # ensure no leftover instance is holding the port
    pkill -f "${SERVER##*/}" 2>/dev/null || true
    sleep 0.1

    "$SERVER" &
    SERVER_PID=$!
    # wait until the port is accepting connections (up to 3 s)
    for i in $(seq 1 30); do
        if curl -sf --max-time 0.2 "$BASE/ping" >/dev/null 2>&1; then
            return 0
        fi
        sleep 0.1
    done
    die "server did not start within 3 seconds"
}

stop_server() {
    if [[ -n "${SERVER_PID:-}" ]]; then
        kill "$SERVER_PID" 2>/dev/null || true
        # give the server up to 2 s to exit cleanly, then force-kill
        local i
        for i in $(seq 1 20); do
            kill -0 "$SERVER_PID" 2>/dev/null || break
            sleep 0.1
        done
        kill -9 "$SERVER_PID" 2>/dev/null || true
        wait "$SERVER_PID" 2>/dev/null || true
    fi
}

assert_eq() {
    local label="$1" expected="$2" actual="$3"
    if [[ "$actual" == "$expected" ]]; then
        echo "  PASS  $label"
        (( ++PASS ))
    else
        echo "  FAIL  $label"
        echo "        expected: $expected"
        echo "        actual:   $actual"
        (( ++FAIL ))
    fi
}

assert_contains() {
    local label="$1" needle="$2" haystack="$3"
    if [[ "$haystack" == *"$needle"* ]]; then
        echo "  PASS  $label"
        (( ++PASS ))
    else
        echo "  FAIL  $label"
        echo "        expected to contain: $needle"
        echo "        actual:              $haystack"
        (( ++FAIL ))
    fi
}

# ── pre-flight ────────────────────────────────────────────────────────────────

[[ -x "$SERVER" ]] || die "binary not found: $SERVER (run cmake --build build first)"
command -v curl >/dev/null || die "curl is required"

trap stop_server EXIT

# ── start ─────────────────────────────────────────────────────────────────────

echo "Starting $SERVER on port $PORT …"
start_server
echo "Server running (PID $SERVER_PID)"
echo

# ── tests ─────────────────────────────────────────────────────────────────────

CURL="curl -s --max-time 5"

echo "── GET /ping ────────────────────────────────────────────────────────────"
body=$($CURL -f "$BASE/ping")
assert_eq       "body is {\"pong\":true}"  '{"pong":true}'  "$body"

http_code=$($CURL -o /dev/null -w "%{http_code}" "$BASE/ping")
assert_eq       "status code 200"         "200"            "$http_code"

ct=$($CURL -D - -o /dev/null "$BASE/ping" | grep -i "^content-type:" | tr -d '\r' | cut -d' ' -f2)
assert_eq       "content-type json"       "application/json" "$ct"

echo
echo "── GET /hello ───────────────────────────────────────────────────────────"
body=$($CURL -f "$BASE/hello")
assert_contains "body contains greeting"  "Hello from chttp" "$body"
assert_contains "body echoes path"        "/hello"            "$body"

http_code=$($CURL -o /dev/null -w "%{http_code}" "$BASE/hello")
assert_eq       "status code 200"         "200"              "$http_code"

echo
echo "── POST /echo ───────────────────────────────────────────────────────────"
payload="hello world"
body=$($CURL -f -X POST -d "$payload" "$BASE/echo")
assert_eq       "body mirrors payload"    "$payload"         "$body"

http_code=$($CURL -o /dev/null -w "%{http_code}" -X POST -d "$payload" "$BASE/echo")
assert_eq       "status code 200"         "200"              "$http_code"

body=$($CURL -f -X POST "$BASE/echo")
assert_eq       "empty body fallback"     "(no body)"        "$body"

echo
echo "── 404 on unknown route ─────────────────────────────────────────────────"
http_code=$($CURL -o /dev/null -w "%{http_code}" "$BASE/does-not-exist")
assert_eq       "status code 404"         "404"              "$http_code"

# ── summary ───────────────────────────────────────────────────────────────────

echo
echo "Results: $PASS passed, $FAIL failed"
[[ $FAIL -eq 0 ]]
