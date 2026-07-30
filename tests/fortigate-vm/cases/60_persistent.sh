#!/usr/bin/env bash
# lab-requires: vm
#
# Reconnect with persistent=true.
#
# The tunnel is cut server-side; vpnWorker::process() then has to rebuild it
# through the goto-start_tunnel branch.
#
# That branch also checks vpnWorker::stopRequested(): otherwise it would answer
# a SIGTERM with a reconnect instead of shutting down, and the process could
# only be stopped headless with SIGKILL. Both are verified here.

set -uo pipefail
source "$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)/lib/case.sh"
case_setup

PROFILE="lab-persistent"
LOG="$(case_log client)"

client_write_profile "$PROFILE" "trusted_cert=$LAB_GW_DIGEST" "persistent=true" >/dev/null
client_start "$PROFILE" "$LOG" || fail "VPN process did not start"

if ! client_wait_log "$LOG" 'Tunnel is up and running' "$TIMEOUT_CONNECT"; then
    fail "tunnel did not come up" "$(tail -n 25 "$LOG")"
    case_finish
fi
wait_ppp_up 20 || { fail "no ppp interface"; case_finish; }
IFACE1="$(ppp_iface)"
IP1="$(ppp_ip "$IFACE1")"
ok "first tunnel is up: $IFACE1 / $IP1"
assert_eq "split route active" "$IFACE1" "$(route_to_dev "$LAB_INSIDE_IP")"

part "Cutting the tunnel server-side"

# "execute vpn sslvpn del-tunnel" expects the index from the session list, not
# the user name ("Please enter number only for index."). The index is in the
# "SSL-VPN sessions:" table of "get vpn ssl monitor".
FGT_OUT="$CASE_OUT_DIR/fgt-del-tunnel.txt"
fgt_cli "get vpn ssl monitor" >"$FGT_OUT" 2>&1 || true
IDX="$(awk '/SSL-VPN sessions:/{f=1; next} f && /^\|[0-9]+\|/ {gsub(/\|/," "); print $1; exit}' "$FGT_OUT")"

if [[ -n "$IDX" ]]; then
    fgt_cli "execute vpn sslvpn del-tunnel $IDX" >>"$FGT_OUT" 2>&1 || true
    if grep -qiE 'Please enter|Unknown action|command parse error' "$FGT_OUT"; then
        fail "del-tunnel was rejected" "$(cat "$FGT_OUT")"
        client_kill
        case_finish
    fi
    ok "the FortiGate closed tunnel session $IDX ($VPN_USER)"
else
    fail "no SSL-VPN session found on the FortiGate" "$(cat "$FGT_OUT")"
    client_kill
    case_finish
fi

part "Rebuild"

if client_wait_log "$LOG" 'Persistent mode enabled, trying to reconnect' 60; then
    ok "the persistent branch kicks in ('trying to reconnect')"
else
    fail "no reconnect attempt in the log" "$(tail -n 30 "$LOG")"
fi

if client_wait_log_count "$LOG" 'Tunnel is up and running' 2 "$TIMEOUT_RECONNECT"; then
    ok "second tunnel is up"
else
    fail "the tunnel was not rebuilt" "$(tail -n 30 "$LOG")"
    client_kill
    case_finish
fi

if wait_ppp_up 30; then
    IFACE2="$(ppp_iface)"
    ok "ppp interface is back: $IFACE2 / $(ppp_ip "$IFACE2")"
    assert_eq "split route reinstalled after the reconnect" \
        "$IFACE2" "$(route_to_dev "$LAB_INSIDE_IP")"
else
    fail "no ppp interface after the reconnect"
fi

assert_true "process is still running" client_running

part "Stop behaviour with persistent=true"

# SIGTERM has to stop the process in the persistent branch as well, instead of
# triggering yet another reconnect.
UPS_BEFORE="$(grep -Ec 'Tunnel is up and running' "$LOG" 2>/dev/null)"

if client_stop TERM 45; then
    ok "SIGTERM stops the process even with persistent=true"
else
    UPS_AFTER="$(grep -Ec 'Tunnel is up and running' "$LOG" 2>/dev/null)"
    if (( ${UPS_AFTER:-0} > ${UPS_BEFORE:-0} )); then
        fail "SIGTERM triggers another reconnect instead of stopping" \
            "Does the persistent branch check vpnWorker::stopRequested()?"
    else
        fail "process did not react to SIGTERM (had to be killed)" \
            "$(tail -n 20 "$LOG")"
    fi
fi

RC="$(client_exit_code)"
case "$RC" in
    0)   ok "process exited with code 0" ;;
    134) fail "process died with SIGABRT (exit code 134)" \
            "Heap corruption -- is split_routes reset next to the free() of
split_rt?" ;;
    139) fail "process died with SIGSEGV (exit code 139)" \
            "Dangling ptr_tunnel in vpnWorker::end()." ;;
    "")  skip "exit code" "could not be determined" ;;
    *)   fail "process exited with code $RC" "expected 0" ;;
esac

assert_not_contains "no heap corruption in the log" "$LOG" 'invalid pointer'
assert_contains "clean teardown" "$LOG" 'Closed connection to gateway'

# Check the routes BEFORE client_cleanup_all runs -- force_cleanup removes an
# orphaned gateway route itself and would mask the finding.
for _ in 1 2 3 4 5; do
    [[ -z "$(ppp_iface)" ]] && break
    sleep 1
done
assert_true "no ppp interface left" test -z "$(ppp_iface)"

# Interface-bound routes are removed by the kernel together with the interface.
# With split tunneling nothing is left behind.
PPP_ROUTES="$(ip -4 route show 2>/dev/null | grep ' dev ppp' || true)"
if [[ -z "$PPP_ROUTES" ]]; then
    ok "no orphaned ppp routes"
else
    fail "routes via a ppp interface that no longer exists" "$PPP_ROUTES"
fi

# The teardown must have taken the /32 to the gateway with it, otherwise the
# FortiGate is unreachable afterwards (see 50_disconnect).
GW_DEV="$(route_to_dev "$FGT_WAN_IP")"
if [[ "$GW_DEV" == "$LAB_BR_OUT" ]]; then
    ok "no orphaned /32 route to the gateway ($FGT_WAN_IP via $GW_DEV)"
else
    fail "gateway route not rolled back: $FGT_WAN_IP goes via ${GW_DEV:-no route}" \
        "$(ip -4 route show "$FGT_WAN_IP")"
fi

client_cleanup_all

case_finish
