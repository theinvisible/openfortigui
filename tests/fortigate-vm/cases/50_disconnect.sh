#!/usr/bin/env bash
# lab-requires: vm
#
# Clean disconnect and restoration of the host state.
#
# SIGTERM is the correct way: io_loop() installs a handler for it
# (openfortivpn/src/io.c:666) which ends the IO loop via sem_stop_io.
# vpnWorker::process() then walks the teardown chain pppd_terminate ->
# auth_log_out -> ipv4_restore_routes.

set -uo pipefail
source "$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)/lib/case.sh"
case_setup

PROFILE="lab-disconnect"
LOG="$(case_log client)"

ROUTES_BEFORE="$(route_snapshot)"
DEFROUTE_BEFORE="$(default_route_snapshot)"
ADDRS_BEFORE="$(ip -4 -o addr show | awk '{print $2, $4}' | sort)"

client_write_profile "$PROFILE" "trusted_cert=$LAB_GW_DIGEST" >/dev/null
client_start "$PROFILE" "$LOG" || fail "VPN process did not start"

if ! client_wait_log "$LOG" 'Tunnel is up and running' "$TIMEOUT_CONNECT"; then
    fail "tunnel did not come up" "$(tail -n 25 "$LOG")"
    case_finish
fi
wait_ppp_up 20 || { fail "no ppp interface"; case_finish; }

IFACE="$(ppp_iface)"
ok "tunnel is up: $IFACE"
assert_eq "split route active before disconnecting" "$IFACE" "$(route_to_dev "$LAB_INSIDE_IP")"

part "Disconnect via SIGTERM"

if client_stop TERM 45; then
    ok "process exits after SIGTERM"
else
    fail "process did not react to SIGTERM (had to be killed)" \
        "$(tail -n 20 "$LOG")"
fi

# Regression check: the teardown must not end in a crash.
# vpnProcess::closeProcess() runs as a queued slot of vpnWorker::finished() on
# the main thread. When it called vpnWorker::end() there, that dereferenced
# ptr_tunnel -- a pointer to the stack-local struct tunnel from
# vpnWorker::process(), which no longer existed at that point. Since the fix,
# process() clears the pointer before emit finished() and the stop goes through
# SIGTERM so that the teardown happens on the owning thread.
RC="$(client_exit_code)"
case "$RC" in
    0)   ok "process exited with code 0" ;;
    139) fail "process died with SIGSEGV (exit code 139)" \
            "Dangling ptr_tunnel in vpnWorker::end() --
called from vpnProcess::closeProcess()." ;;
    "")  skip "exit code" "could not be determined" ;;
    *)   fail "process exited with code $RC" "expected 0" ;;
esac

assert_contains "connection closed" "$LOG" 'Closed connection to gateway'
assert_contains "pppd terminated" "$LOG" 'Terminated pppd'
assert_contains "logged out" "$LOG" 'Logged out'

part "Host state after disconnecting"

# The ppp interface may linger briefly
for _ in 1 2 3 4 5; do
    [[ -z "$(ppp_iface)" ]] && break
    sleep 1
done
assert_true "ppp interface removed" test -z "$(ppp_iface)"

assert_eq "default route unchanged" "$DEFROUTE_BEFORE" "$(default_route_snapshot)"
assert_eq "remaining routes unchanged" "$ROUTES_BEFORE" "$(route_snapshot)"
assert_eq "IPv4 addresses unchanged" "$ADDRS_BEFORE" \
    "$(ip -4 -o addr show | awk '{print $2, $4}' | sort)"

# The gateway route specifically: ipv4_protect_tunnel_route() installs a /32 to
# the gateway (in this lab wrongly via the default next hop instead of on-link,
# an upstream finding in ipv4.c). ipv4_restore_routes() removes it on a clean
# teardown -- if it stays behind, the FortiGate becomes unreachable.
GW_DEV="$(route_to_dev "$FGT_WAN_IP")"
if [[ "$GW_DEV" == "$LAB_BR_OUT" ]]; then
    ok "no orphaned /32 route to the gateway ($FGT_WAN_IP via $GW_DEV)"
else
    fail "gateway route not rolled back: $FGT_WAN_IP goes via ${GW_DEV:-no route}" \
        "$(ip -4 route show "$FGT_WAN_IP")"
fi

# Counter-check for the split route: the inside network no longer goes via ppp
DEV_AFTER="$(route_to_dev "$LAB_INSIDE_IP")"
if [[ "$DEV_AFTER" != ppp* ]]; then
    ok "split route removed ($LAB_INSIDE_IP now goes via ${DEV_AFTER:-no route})"
else
    fail "the split route still points at $DEV_AFTER" "$(ip -4 route show | grep ppp)"
fi

if pgrep -f 'openfortigui --start-vpn' >/dev/null 2>&1; then
    fail "the VPN process is still running" "$(pgrep -af 'openfortigui --start-vpn')"
else
    ok "no VPN process left behind"
fi

if pgrep -f 'pppd .*169\.254\.2\.1' >/dev/null 2>&1; then
    fail "pppd is still running" "$(pgrep -af 'pppd .*169\.254\.2\.1')"
else
    ok "no pppd left behind"
fi

case_finish
