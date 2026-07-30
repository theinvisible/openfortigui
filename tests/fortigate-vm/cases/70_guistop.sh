#!/usr/bin/env bash
# lab-requires: vm
#
# Stop initiated by the GUI while the tunnel is up.
#
# Two paths lead into vpnProcess::closeProcess() while the tunnel still stands:
# ACTION_STOP over the local socket, and the GUI going away
# (onServerDisconnected). Headless, both are only testable with a stand-in that
# provides the "openfortiGUI" QLocalServer -- mock_gui.py.
#
# closeProcess() used to tear the tunnel down from the main thread
# (vpnWorker::end() -> ipv4_restore_routes) and then end the worker with
# QThread::terminate(), which orphaned pppd. Today it raises a SIGTERM instead,
# so that process() cleans up on the owning thread.

set -uo pipefail
source "$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)/lib/case.sh"
case_setup

MOCK="$LAB_SRC_DIR/mock_gui.py"
[[ -f "$MOCK" ]] || { skip "GUI stop" "mock_gui.py is missing"; case_finish; }

# mode -> description
run_case() {
    local mode="$1" label="$2"
    local profile="lab-guistop-$mode"
    local log; log="$(case_log "client-$mode")"
    local mockout="$CASE_OUT_DIR/mockgui-$mode.log"

    part "$label"

    local routes_before defroute_before
    routes_before="$(route_snapshot)"
    defroute_before="$(default_route_snapshot)"

    client_write_profile "$profile" "trusted_cert=$LAB_GW_DIGEST" >/dev/null

    python3 "$MOCK" "$mode" "$profile" "$log" 90 >"$mockout" 2>&1 &
    local mock_pid=$!
    sleep 1

    if ! client_start "$profile" "$log"; then
        fail "VPN process did not start ($mode)"
        kill "$mock_pid" 2>/dev/null
        return
    fi

    if ! client_wait_log "$log" 'Tunnel is up and running' "$TIMEOUT_CONNECT"; then
        fail "tunnel did not come up ($mode)" "$(tail -n 20 "$log")"
        client_cleanup_all; kill "$mock_pid" 2>/dev/null
        return
    fi
    ok "tunnel is up ($mode): $(ppp_iface)"

    if grep -q 'HELLO' "$mockout" 2>/dev/null; then
        ok "the child process registered with the GUI stand-in"
    else
        skip "GUI registration" "no HELLO in $mockout"
    fi

    # The stand-in triggers the stop -- wait for the process to end
    if client_wait_exit 60; then
        ok "process exits after the GUI stop"
    else
        fail "process keeps running after the GUI stop" "$(tail -n 20 "$log")"
        client_cleanup_all; kill "$mock_pid" 2>/dev/null
        return
    fi

    local rc; rc="$(client_exit_code)"
    case "$rc" in
        0)   ok "process exited with code 0" ;;
        134) fail "process died with SIGABRT (exit code 134)" "heap corruption" ;;
        139) fail "process died with SIGSEGV (exit code 139)" \
                "Dangling ptr_tunnel in vpnWorker::end()." ;;
        "")  skip "exit code" "could not be determined" ;;
        *)   fail "process exited with code $rc" "expected 0" ;;
    esac

    # The teardown has to have run through completely, not just half way.
    assert_contains "routes restored ($mode)" "$log" 'Restoring routes'
    assert_contains "pppd terminated ($mode)" "$log" 'Terminated pppd'
    assert_contains "logged out ($mode)" "$log" 'Logged out'

    for _ in 1 2 3 4 5; do
        [[ -z "$(ppp_iface)" ]] && break
        sleep 1
    done
    assert_true "ppp interface removed ($mode)" test -z "$(ppp_iface)"

    if pgrep -f 'pppd .*169\.254\.2\.1' >/dev/null 2>&1; then
        fail "pppd orphaned ($mode)" "$(pgrep -af 'pppd .*169\.254\.2\.1')"
    else
        ok "no orphaned pppd ($mode)"
    fi

    assert_eq "default route unchanged ($mode)" \
        "$defroute_before" "$(default_route_snapshot)"
    assert_eq "remaining routes unchanged ($mode)" \
        "$routes_before" "$(route_snapshot)"

    kill "$mock_pid" 2>/dev/null
    wait "$mock_pid" 2>/dev/null
    client_cleanup_all
}

run_case stop       "a) ACTION_STOP over the local socket"
run_case disconnect "b) the GUI exits (socket disconnect)"

case_finish
