#!/usr/bin/env bash
# lab-requires: vm
#
# Routes and data path through the tunnel.
#
# Key point: the target 10.99.10.50 sits in its own netns behind the FortiGate
# and the host has no address on that bridge. A successful curl therefore proves
# tunnel + installed split routes + FortiGate policy.
#
# Second key point: the host's default route must NOT change. Without split
# routes openfortivpn deletes it (ipv4_set_default_routes,
# openfortivpn/src/ipv4.c:922) -- which is exactly what would cut the test host
# off the network.

set -uo pipefail
source "$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)/lib/case.sh"
case_setup

PROFILE="lab-routing"
LOG="$(case_log client)"

DEFROUTE_BEFORE="$(default_route_snapshot)"

client_write_profile "$PROFILE" "trusted_cert=$LAB_GW_DIGEST" "set_routes=true" >/dev/null
client_start "$PROFILE" "$LOG" || fail "VPN process did not start"

if ! client_wait_log "$LOG" 'Tunnel is up and running' "$TIMEOUT_CONNECT"; then
    fail "tunnel did not come up" "$(tail -n 25 "$LOG" 2>/dev/null)"
    case_finish
fi
wait_ppp_up 20 || { fail "no ppp interface"; case_finish; }

IFACE="$(ppp_iface)"
PPPIP="$(ppp_ip "$IFACE")"
ok "tunnel is up: $IFACE / $PPPIP"

part "Routes"

assert_eq "split route: $LAB_INSIDE_IP goes via $IFACE" \
    "$IFACE" "$(route_to_dev "$LAB_INSIDE_IP")"

assert_eq "default route unchanged (split tunneling)" \
    "$DEFROUTE_BEFORE" "$(default_route_snapshot)"

if ip -4 route show default 2>/dev/null | grep -q "dev $IFACE"; then
    fail "the default route points at the tunnel" \
        "The portal apparently pushes no split routes -- the test host would
have lost its internet connection. Check the portal's 'split-tunneling'
setting: testlab show"
else
    ok "no default route through the tunnel"
fi

part "Data path behind the FortiGate"

# Opt-in: a FortiGate VM without a valid license brings the tunnel up but
# forwards nothing. "get vpn ssl monitor" then shows the session with
# I/O Bytes 0/0, the routing table has no host route to the tunnel IP, and a
# flow trace on the FortiGate sees no packet at all. Against a fully licensed
# FortiGate this can be checked as well with OFGUI_TEST_DATAPATH=1.
if [[ "${OFGUI_TEST_DATAPATH:-0}" == "1" ]]; then
    INSIDE_LOG_BEFORE=0
    [[ -f "$LAB_INSIDE_LOG" ]] && INSIDE_LOG_BEFORE="$(wc -l <"$LAB_INSIDE_LOG")"

    assert_true "target behind the FortiGate reachable (http://$LAB_INSIDE_IP:$LAB_INSIDE_PORT)" \
        inside_reachable 8

    # The HTTP server in the netns logs the source IP. Without NAT in the policy
    # the tunnel pool address has to show up there.
    sleep 1
    NEWLINES="$(tail -n "+$(( INSIDE_LOG_BEFORE + 1 ))" "$LAB_INSIDE_LOG" 2>/dev/null)"
    if [[ "$FGT_POLICY_NAT" == "0" ]]; then
        if printf '%s' "$NEWLINES" | grep -q "$PPPIP"; then
            ok "source IP at the target is the tunnel address $PPPIP"
        else
            fail "source IP at the target is not $PPPIP" "$NEWLINES"
        fi
    else
        if printf '%s' "$NEWLINES" | grep -q "$FGT_LAN_IP"; then
            ok "source IP at the target is the NAT address $FGT_LAN_IP"
        else
            fail "source IP at the target is not $FGT_LAN_IP" "$NEWLINES"
        fi
    fi
else
    skip "reachability behind the FortiGate" \
        "set OFGUI_TEST_DATAPATH=1; needs a licensed FortiGate"
fi

part "Counters"

# The same source vpnProcess::updateStats() reads its statistics from
read -r RX TX <<<"$(iface_bytes "$IFACE")"
if [[ -n "${RX:-}" && -n "${TX:-}" ]] && (( RX > 0 && TX > 0 )); then
    ok "/proc/net/dev for $IFACE: RX=$RX TX=$TX"
else
    fail "no byte counters for $IFACE in /proc/net/dev" \
        "$(grep -E "^ *$IFACE:" /proc/net/dev 2>/dev/null)"
fi

part "DNS"

if [[ "$OFGUI_TEST_DNS" == "1" ]]; then
    # openfortivpn is compiled here with an empty RESOLVCONF_PATH and therefore
    # writes to /etc/resolv.conf directly (ipv4.c:1126). On this host that is a
    # symlink into the systemd-resolved stub, so back it up first and write it
    # back byte for byte afterwards.
    RESOLV_TARGET="$(readlink -f /etc/resolv.conf)"
    RESOLV_BACKUP="$CASE_OUT_DIR/resolv.conf.bak"
    as_root cp -a "$RESOLV_TARGET" "$RESOLV_BACKUP"
    info "backed up: $RESOLV_TARGET -> $RESOLV_BACKUP"

    client_stop TERM 30 >/dev/null 2>&1 || true
    DNSLOG="$(case_log client-dns)"
    client_write_profile "$PROFILE-dns" "trusted_cert=$LAB_GW_DIGEST" "set_dns=true" >/dev/null
    client_start "$PROFILE-dns" "$DNSLOG" || fail "DNS test process did not start"

    if client_wait_log "$DNSLOG" 'Tunnel is up and running' "$TIMEOUT_CONNECT"; then
        sleep 2
        if grep -q "nameserver $FGT_LAN_IP" "$RESOLV_TARGET" 2>/dev/null; then
            ok "DNS server $FGT_LAN_IP written to $RESOLV_TARGET"
        else
            fail "DNS server not written" "$(cat "$RESOLV_TARGET" 2>/dev/null)"
        fi
    else
        fail "tunnel in the DNS test did not come up" "$(tail -n 20 "$DNSLOG")"
    fi

    client_stop TERM 30 >/dev/null 2>&1 || true
    sleep 1
    as_root cp -a --no-preserve=timestamps "$RESOLV_BACKUP" "$RESOLV_TARGET"
    if diff -q "$RESOLV_BACKUP" "$RESOLV_TARGET" >/dev/null 2>&1; then
        ok "$RESOLV_TARGET restored"
    else
        fail "$RESOLV_TARGET differs from the backup" \
            "$(diff "$RESOLV_BACKUP" "$RESOLV_TARGET" 2>&1 | head -n 10)"
    fi
else
    skip "DNS push" "set OFGUI_TEST_DNS=1 (writes to /etc/resolv.conf)"
fi

client_stop TERM 30 >/dev/null 2>&1 || true
case_finish
