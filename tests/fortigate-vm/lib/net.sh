# shellcheck shell=bash
#
# Network topology of the test lab:
#
#   host 10.99.99.1/24 --[ofgt-out]-- tap0 == FortiGate port1 10.99.99.10
#        (ppp0)  ==== SSL-VPN tunnel ====> ssl.root
#                                          FortiGate port2 10.99.10.1/24
#                                                 == tap1 --[ofgt-in]-- veth
#                                                        netns ofgt-inside
#                                                        10.99.10.50:8080
#
# The host deliberately has NO address on ofgt-in: 10.99.10.50 is therefore
# reachable through the VPN tunnel only, so a successful curl proves tunnel +
# routes + FortiGate policy at once.

LAB_INSIDE_PIDFILE="$LAB_RUN_DIR/inside-http.pid"
LAB_INSIDE_LOG="$LAB_OUT_DIR/inside-http.log"

_link_exists() { ip link show dev "$1" >/dev/null 2>&1; }
_netns_exists() { ip netns list 2>/dev/null | awk '{print $1}' | grep -qx "$1"; }

# --------------------------------------------------------------------------
# iptables: Docker sets the FORWARD policy to DROP. Once br_netfilter is
# loaded, bridged packets go through FORWARD as well -- tap1 <-> veth on
# ofgt-in would then never meet. Two targeted ACCEPT rules instead of global
# sysctl changes.
# --------------------------------------------------------------------------

_fw_rules() {
    printf '%s\n' \
        "-i $LAB_BR_IN -o $LAB_BR_IN" \
        "-i $LAB_BR_OUT -o $LAB_BR_OUT"
}

net_fw_up() {
    have iptables || { warn "iptables not found -- skipping FORWARD rules"; return 0; }
    local rule
    while read -r rule; do
        # shellcheck disable=SC2086
        as_root iptables -C FORWARD $rule -j ACCEPT 2>/dev/null && continue
        # shellcheck disable=SC2086
        as_root iptables -I FORWARD 1 $rule -j ACCEPT 2>/dev/null \
            || warn "could not install FORWARD rule: $rule"
    done < <(_fw_rules)
}

net_fw_down() {
    have iptables || return 0
    local rule
    while read -r rule; do
        # shellcheck disable=SC2086
        while as_root iptables -C FORWARD $rule -j ACCEPT 2>/dev/null; do
            # shellcheck disable=SC2086
            as_root iptables -D FORWARD $rule -j ACCEPT 2>/dev/null || break
        done
    done < <(_fw_rules)
}

# --------------------------------------------------------------------------
# Setup / teardown
# --------------------------------------------------------------------------

net_is_up() {
    _link_exists "$LAB_BR_OUT" && _link_exists "$LAB_BR_IN" \
        && _link_exists "$LAB_TAP_OUT" && _link_exists "$LAB_TAP_IN" \
        && _netns_exists "$LAB_NETNS"
}

net_require_up() {
    net_is_up || die "lab network is missing -- run 'testlab net-up' first"
}

net_up() {
    ensure_sudo
    lab_mkdirs

    if net_is_up; then
        info "lab network is already up"
        inside_http_start
        net_fw_up
        return 0
    fi

    step "Setting up the lab network"

    # --- outside: bridge with a host IP -----------------------------------
    if ! _link_exists "$LAB_BR_OUT"; then
        as_root ip link add name "$LAB_BR_OUT" type bridge || die "bridge $LAB_BR_OUT"
    fi
    as_root ip addr replace "$LAB_HOST_IP/${LAB_OUT_NET##*/}" dev "$LAB_BR_OUT"
    as_root ip link set "$LAB_BR_OUT" up

    # --- inside: bridge without a host IP ---------------------------------
    if ! _link_exists "$LAB_BR_IN"; then
        as_root ip link add name "$LAB_BR_IN" type bridge || die "bridge $LAB_BR_IN"
    fi
    as_root ip link set "$LAB_BR_IN" up

    # --- taps for QEMU (owned by the user, the VM runs unprivileged) ------
    local tap
    for tap in "$LAB_TAP_OUT:$LAB_BR_OUT" "$LAB_TAP_IN:$LAB_BR_IN"; do
        local name="${tap%%:*}" br="${tap##*:}"
        if ! _link_exists "$name"; then
            as_root ip tuntap add dev "$name" mode tap user "${SUDO_USER:-$USER}" \
                || die "tap $name"
        fi
        as_root ip link set "$name" master "$br"
        as_root ip link set "$name" up
    done

    # --- netns holding the test target behind the FortiGate ---------------
    if ! _netns_exists "$LAB_NETNS"; then
        as_root ip netns add "$LAB_NETNS" || die "netns $LAB_NETNS"
    fi
    if ! _link_exists "$LAB_VETH_HOST"; then
        as_root ip link add "$LAB_VETH_HOST" type veth peer name "$LAB_VETH_NS" \
            || die "veth pair"
    fi
    as_root ip link set "$LAB_VETH_HOST" master "$LAB_BR_IN"
    as_root ip link set "$LAB_VETH_HOST" up
    if _link_exists "$LAB_VETH_NS"; then
        as_root ip link set "$LAB_VETH_NS" netns "$LAB_NETNS"
    fi
    as_root ip -n "$LAB_NETNS" link set lo up
    as_root ip -n "$LAB_NETNS" addr replace "$LAB_INSIDE_IP/${LAB_INSIDE_NET##*/}" \
        dev "$LAB_VETH_NS"
    as_root ip -n "$LAB_NETNS" link set "$LAB_VETH_NS" up
    as_root ip -n "$LAB_NETNS" route replace default via "$FGT_LAN_IP"

    net_fw_up
    inside_http_start

    info "outside: $LAB_HOST_IP <-> $FGT_WAN_IP  |  inside: $FGT_LAN_IP <-> $LAB_INSIDE_IP:$LAB_INSIDE_PORT"
}

net_down() {
    ensure_sudo
    step "Tearing down the lab network"
    inside_http_stop
    net_fw_down

    local l
    for l in "$LAB_TAP_OUT" "$LAB_TAP_IN" "$LAB_VETH_HOST"; do
        _link_exists "$l" && as_root ip link del "$l" 2>/dev/null || true
    done
    _netns_exists "$LAB_NETNS" && as_root ip netns del "$LAB_NETNS" 2>/dev/null || true
    for l in "$LAB_BR_OUT" "$LAB_BR_IN"; do
        _link_exists "$l" && as_root ip link del "$l" 2>/dev/null || true
    done
    info "network removed"
}

# --------------------------------------------------------------------------
# Test target inside the netns
# --------------------------------------------------------------------------

inside_http_running() {
    [[ -f "$LAB_INSIDE_PIDFILE" ]] || return 1
    local pid; pid="$(cat "$LAB_INSIDE_PIDFILE" 2>/dev/null)"
    pid_alive "$pid" && pid_cmdline_has "$pid" "http.server"
}

inside_http_start() {
    inside_http_running && return 0
    _netns_exists "$LAB_NETNS" || return 1

    : >"$LAB_INSIDE_LOG"
    # The access log lines carry the source IP -- the routing test uses that to
    # verify the FortiGate passes the tunnel pool IP through.
    as_root sh -c "echo \$\$ >'$LAB_INSIDE_PIDFILE'; exec ip netns exec '$LAB_NETNS' \
        python3 -u -m http.server '$LAB_INSIDE_PORT' --bind '$LAB_INSIDE_IP'" \
        >>"$LAB_INSIDE_LOG" 2>&1 &
    disown 2>/dev/null || true

    local waited=0
    while ! as_root ip netns exec "$LAB_NETNS" \
            timeout 1 bash -c "exec 3<>/dev/tcp/$LAB_INSIDE_IP/$LAB_INSIDE_PORT" 2>/dev/null; do
        sleep 0.3
        waited=$(( waited + 1 ))
        (( waited > 30 )) && { warn "inside HTTP server does not start"; return 1; }
    done
    info "test target running: http://$LAB_INSIDE_IP:$LAB_INSIDE_PORT (netns $LAB_NETNS)"
}

inside_http_stop() {
    if [[ -f "$LAB_INSIDE_PIDFILE" ]]; then
        local pid; pid="$(cat "$LAB_INSIDE_PIDFILE" 2>/dev/null)"
        if pid_alive "$pid"; then
            as_root kill "$pid" 2>/dev/null || true
        fi
        rm -f "$LAB_INSIDE_PIDFILE" 2>/dev/null || true
    fi
    # Safety net in case the PID file went missing
    as_root pkill -f "http.server $LAB_INSIDE_PORT --bind $LAB_INSIDE_IP" 2>/dev/null || true
}

# --------------------------------------------------------------------------
# Optional internet uplink
#
# The lab network is deliberately isolated. To activate a VM license the
# FortiGate has to reach FortiCare/FortiGuard, though
# (/api/v2/monitor/license/status otherwise shows fortiguard.connected=false).
# Hence NAT through the host, plus a default route and DNS on the FortiGate.
# --------------------------------------------------------------------------

LAB_IPFWD_MARKER="$LAB_RUN_DIR/ipforward-was-off"

net_uplink_up() {
    ensure_sudo
    net_require_up
    step "Enabling the internet uplink for the FortiGate"

    if [[ "$(cat /proc/sys/net/ipv4/ip_forward 2>/dev/null)" != "1" ]]; then
        : >"$LAB_IPFWD_MARKER"
        as_root sysctl -q -w net.ipv4.ip_forward=1
        info "net.ipv4.ip_forward enabled (reset again on 'uplink off')"
    fi

    if ! as_root iptables -t nat -C POSTROUTING -s "$LAB_OUT_NET" \
            ! -o "$LAB_BR_OUT" -j MASQUERADE 2>/dev/null; then
        as_root iptables -t nat -A POSTROUTING -s "$LAB_OUT_NET" \
            ! -o "$LAB_BR_OUT" -j MASQUERADE \
            || die "could not install the MASQUERADE rule"
    fi
    as_root iptables -C FORWARD -i "$LAB_BR_OUT" -j ACCEPT 2>/dev/null \
        || as_root iptables -I FORWARD 1 -i "$LAB_BR_OUT" -j ACCEPT
    as_root iptables -C FORWARD -o "$LAB_BR_OUT" -j ACCEPT 2>/dev/null \
        || as_root iptables -I FORWARD 1 -o "$LAB_BR_OUT" -j ACCEPT

    if vm_running; then
        info "setting the default route and DNS on the FortiGate"
        fgt_cli \
            "config router static" "edit 99" "set gateway $LAB_HOST_IP" \
            "set device port1" "next" "end" \
            "config system dns" "set primary ${LAB_DNS_SERVER:-1.1.1.1}" "end" \
            >/dev/null 2>&1 || warn "route/DNS not set on the FortiGate"
    fi
    info "uplink active: $LAB_OUT_NET is masqueraded through the host"
}

net_uplink_down() {
    ensure_sudo
    have iptables || return 0
    while as_root iptables -t nat -C POSTROUTING -s "$LAB_OUT_NET" \
            ! -o "$LAB_BR_OUT" -j MASQUERADE 2>/dev/null; do
        as_root iptables -t nat -D POSTROUTING -s "$LAB_OUT_NET" \
            ! -o "$LAB_BR_OUT" -j MASQUERADE 2>/dev/null || break
    done
    local dir
    for dir in -i -o; do
        while as_root iptables -C FORWARD "$dir" "$LAB_BR_OUT" -j ACCEPT 2>/dev/null; do
            as_root iptables -D FORWARD "$dir" "$LAB_BR_OUT" -j ACCEPT 2>/dev/null || break
        done
    done
    if [[ -f "$LAB_IPFWD_MARKER" ]]; then
        as_root sysctl -q -w net.ipv4.ip_forward=0
        rm -f "$LAB_IPFWD_MARKER"
        info "net.ipv4.ip_forward reset"
    fi
    info "uplink removed"
}

net_status() {
    local l
    printf 'Network:\n'
    for l in "$LAB_BR_OUT" "$LAB_BR_IN" "$LAB_TAP_OUT" "$LAB_TAP_IN" "$LAB_VETH_HOST"; do
        if _link_exists "$l"; then
            printf '  %-12s %spresent%s\n' "$l" "$C_GREEN" "$C_OFF"
        else
            printf '  %-12s %smissing%s\n' "$l" "$C_RED" "$C_OFF"
        fi
    done
    if _netns_exists "$LAB_NETNS"; then
        printf '  %-12s %spresent%s\n' "netns" "$C_GREEN" "$C_OFF"
    else
        printf '  %-12s %smissing%s\n' "netns" "$C_RED" "$C_OFF"
    fi
    if inside_http_running; then
        printf '  %-12s %srunning%s (%s:%s)\n' "target" "$C_GREEN" "$C_OFF" \
            "$LAB_INSIDE_IP" "$LAB_INSIDE_PORT"
    else
        printf '  %-12s %sdown%s\n' "target" "$C_RED" "$C_OFF"
    fi
}
