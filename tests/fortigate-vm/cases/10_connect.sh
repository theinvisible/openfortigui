#!/usr/bin/env bash
# lab-requires: vm
#
# Happy path: connect with a pinned gateway certificate.
#
# Exercises the full chain in vpnWorker::process(): get_gateway_host_ip,
# ssl_connect, auth_log_in, auth_request_vpn_allocation, auth_get_config,
# pppd_run, io_loop -- and that the result is a ppp interface with an address
# from the FortiGate's SSL-VPN pool.

set -uo pipefail
source "$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)/lib/case.sh"
case_setup

PROFILE="lab-connect"
LOG="$(case_log client)"

client_write_profile "$PROFILE" "trusted_cert=$LAB_GW_DIGEST" >/dev/null
info "profile $PROFILE (trusted_cert=${LAB_GW_DIGEST:0:16}...)"

client_start "$PROFILE" "$LOG" || fail "VPN process did not start"

if client_wait_log "$LOG" 'Tunnel is up and running' "$TIMEOUT_CONNECT"; then
    ok "tunnel is up ('Tunnel is up and running.')"
else
    fail "tunnel did not come up" "$(tail -n 25 "$LOG" 2>/dev/null)"
    case_finish
fi

assert_contains "gateway reached" "$LOG" 'Connected to gateway'
assert_contains "authenticated" "$LOG" 'Authenticated'
assert_contains "VPN allocated" "$LOG" 'Remote gateway has allocated a VPN'
assert_not_contains "no certificate warning" "$LOG" 'certificate digest is not in the local whitelist'

if wait_ppp_up 20; then
    IFACE="$(ppp_iface)"
    PPPIP="$(ppp_ip "$IFACE")"
    ok "ppp interface $IFACE with pool address $PPPIP"
else
    IFACE="$(ppp_iface)"
    fail "no ppp interface with an address from ${LAB_POOL_FIRST}-${LAB_POOL_LAST}" \
        "$(ip -4 addr show 2>/dev/null | grep -A2 ppp || echo 'no ppp interface present')"
fi

assert_true "VPN process is running" client_running

APPLOG="$(case_app_log)"
assert_true "application log is being written" test -s "$APPLOG"
assert_not_contains "no Critical/Fatal messages" "$APPLOG" 'openfortiGUI::(Critical|Fatal)'

# Teardown is not part of this case's scope (see 50_disconnect), but the next
# case should start from a clean state.
client_stop TERM 30 >/dev/null 2>&1 || true

case_finish
