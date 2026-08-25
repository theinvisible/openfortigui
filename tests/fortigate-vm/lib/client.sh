# shellcheck shell=bash
#
# Client side: an isolated openfortiGUI test home, writing profiles, starting
# and stopping the headless VPN process, and inspecting network state.
#
# The trick for the isolation is in tiConfMain::formatPath()
# (ticonfmain.cpp:183): if --main-config is an absolute path, openfortiGUI
# derives the home directory from two levels above it. So
# <lab>/client/home/.openfortigui/main.conf yields home = <lab>/client/home and
# the user's real ~/.openfortigui stays untouched.

LAB_CLIENT_PIDFILE="$LAB_RUN_DIR/client.pid"
LAB_CLIENT_RCFILE="$LAB_RUN_DIR/client.rc"

# --------------------------------------------------------------------------
# Binary
# --------------------------------------------------------------------------

client_bin() {
    if [[ -n "$OFGUI_BIN" ]]; then
        printf '%s' "$OFGUI_BIN"; return 0
    fi
    local candidates=(
        "$REPO_ROOT/cmake-build-debug/bin/openfortigui"
        "$REPO_ROOT/openfortigui/openfortigui"
        "/usr/bin/openfortigui"
    )
    local c
    for c in "${candidates[@]}"; do
        [[ -x "$c" ]] && { printf '%s' "$c"; return 0; }
    done
    return 1
}

client_require_bin() {
    local bin
    bin="$(client_bin)" || die "no openfortigui binary found. Build it with:
    cmake -S '$REPO_ROOT' -B '$REPO_ROOT/cmake-build-debug' && cmake --build '$REPO_ROOT/cmake-build-debug'
or set OFGUI_BIN."
    printf '%s' "$bin"
}

# --------------------------------------------------------------------------
# Password encryption (identical to vpnHelper::Qaes128_encrypt)
# --------------------------------------------------------------------------

ofg_encrypt() {
    [[ -z "$1" ]] && { printf ''; return 0; }
    printf '%s' "$1" | openssl enc -aes-128-cbc -a -A \
        -K "$(hex_of "$OFGUI_AESKEY")" -iv "$(hex_of "$OFGUI_AESIV")"
}

ofg_decrypt() {
    [[ -z "$1" ]] && { printf ''; return 0; }
    printf '%s' "$1" | openssl enc -d -aes-128-cbc -a -A \
        -K "$(hex_of "$OFGUI_AESKEY")" -iv "$(hex_of "$OFGUI_AESIV")"
}

# --------------------------------------------------------------------------
# Test-Home
# --------------------------------------------------------------------------

client_init_home() {
    local conf_dir="$LAB_CLIENT_HOME/.openfortigui"
    mkdir -p "$conf_dir/vpnprofiles" "$conf_dir/vpngroups" "$conf_dir/logs/vpn"
    chmod 0700 "$conf_dir"

    cat >"$LAB_MAIN_CONF" <<EOF
[main]
debug=true
aeskey=$OFGUI_AESKEY
aesiv=$OFGUI_AESIV
use_system_password_store=false
start_minimized=false
# "setupwizard" is a done-flag, not a switch: MainWindow opens the wizard when it
# is false. It has to be true or the GUI test finds a wizard instead of the main
# window (mainwindow.cpp, end of the constructor).
setupwizard=true
changelogrev_read=99
disallow_unsecure_certificates=false
sudo_preserve_env=false

[paths]
globalvpnprofiles=/etc/openfortigui/vpnprofiles
localvpnprofiles=~/.openfortigui/vpnprofiles
localvpngroups=~/.openfortigui/vpngroups
logs=$conf_dir/logs
initd=/etc/init.d/openfortigui

[checks]
sudopresenv=false
# Opt out of the one-time migration that switches debug logging off (issue #212).
# Several cases assert on the GUI's qDebug output -- 'finished::', 'error
# occurred::', 'vpnprofile found' -- so the lab wants debug=true above to stick.
debug_default_migrated=true

[gui]
disable_notifications=true
connect_on_dblclick=false
EOF
    info "test home: $LAB_CLIENT_HOME (home is derived from --main-config)"
}

# client_make_client_cert <directory> <passphrase>
#
# Self-signed client certificate with an encrypted private key. openfortivpn asks
# for the passphrase through pem_passphrase_cb() (tunnel.c), which is what issues
# #166 is about -- used by 30_cert (prompt appears in the log) and by 90_gui (the
# dialog appears on screen).
client_make_client_cert() {
    local dir="$1" pass="$2"
    mkdir -p "$dir"
    openssl req -x509 -newkey rsa:2048 -keyout "$dir/key.pem" \
        -out "$dir/cert.pem" -days 2 -passout "pass:$pass" \
        -subj "/CN=openfortigui-lab" >/dev/null 2>&1
}

# client_write_profile <name> [key=value ...]
#
# Writes a profile in the format of tiConfVpnProfiles::saveVpnProfile()
# (ticonfmain.cpp:230). The special key "password_plain" gets encrypted.
client_write_profile() {
    local name="$1"; shift
    [[ ${#name} -ge 3 ]] || die "profile name too short (openfortigui_config::validatorName requires >= 3 characters)"

    declare -A p=(
        [gateway_host]="$FGT_WAN_IP"
        [gateway_port]="$FGT_SSLVPN_PORT"
        [username]="$VPN_USER"
        [password_plain]="$VPN_PASS"
        [persistent]=false
        [device_type]=0
        [ca_file]=""
        [user_cert]=""
        [user_key]=""
        [verify_cert]=false
        [trusted_cert]=""
        [trust_all_gw_certs]=false
        [set_routes]=true
        [set_dns]=false
        [pppd_no_peerdns]=false
        # Has to stay on: openfortiGUI passes ":169.254.2.1" to pppd as the
        # remote address (vpnworker.cpp, pppd_run), but the FortiGate proposes
        # its own. Without pppd_accept_remote, "ipcp-accept-remote" is missing
        # (vpnworker.cpp:276) and pppd fails with
        # "Peer refused to agree to his IP address".
        [pppd_accept_remote]=true
        [insecure_ssl]=false
        [debug]=true
        [realm]=""
        [autostart]=false
        [always_ask_otp]=false
        [otp_prompt]=""
        [otp_delay]=0
        [half_internet_routers]=false
        [pppd_log_file]=""
        [pppd_plugin_file]=""
        [pppd_ifname]=""
        [pppd_ipparam]=""
        [pppd_call]=""
        [seclevel1]=false
        [min_tls]=default
        # Encrypted like the password; "cookie_plain" is the readable input.
        [cookie_plain]=""
        [sni]=""
        [saml_login]=false
        [saml_port]=8020
    )

    local kv key
    for kv in "$@"; do
        key="${kv%%=*}"
        [[ -v p["$key"] ]] || die "unknown profile key: $key"
        p["$key"]="${kv#*=}"
    done

    mkdir -p "$LAB_PROFILE_DIR"
    local file="$LAB_PROFILE_DIR/$name.conf"
    cat >"$file" <<EOF
[vpn]
name=$name
gateway_host=${p[gateway_host]}
gateway_port=${p[gateway_port]}
username=${p[username]}
password=$(ofg_encrypt "${p[password_plain]}")
cookie=$(ofg_encrypt "${p[cookie_plain]}")
sni=${p[sni]}
persistent=${p[persistent]}
device_type=${p[device_type]}

[cert]
ca_file=${p[ca_file]}
user_cert=${p[user_cert]}
user_key=${p[user_key]}
verify_cert=${p[verify_cert]}
trusted_cert=${p[trusted_cert]}
trust_all_gw_certs=${p[trust_all_gw_certs]}

[options]
set_routes=${p[set_routes]}
set_dns=${p[set_dns]}
pppd_no_peerdns=${p[pppd_no_peerdns]}
pppd_accept_remote=${p[pppd_accept_remote]}
insecure_ssl=${p[insecure_ssl]}
debug=${p[debug]}
realm=${p[realm]}
autostart=${p[autostart]}
always_ask_otp=${p[always_ask_otp]}
otp_prompt=${p[otp_prompt]}
otp_delay=${p[otp_delay]}
half_internet_routers=${p[half_internet_routers]}
pppd_log_file=${p[pppd_log_file]}
pppd_plugin_file=${p[pppd_plugin_file]}
pppd_ifname=${p[pppd_ifname]}
pppd_ipparam=${p[pppd_ipparam]}
pppd_call=${p[pppd_call]}
seclevel1=${p[seclevel1]}
min_tls=${p[min_tls]}
saml_login=${p[saml_login]}
saml_port=${p[saml_port]}
EOF
    printf '%s' "$file"
}

# --------------------------------------------------------------------------
# Process control
# --------------------------------------------------------------------------

client_running() {
    [[ -f "$LAB_CLIENT_PIDFILE" ]] || return 1
    local pid; pid="$(cat "$LAB_CLIENT_PIDFILE" 2>/dev/null)"
    pid_alive "$pid" && pid_cmdline_has "$pid" "--start-vpn"
}

client_pid() { cat "$LAB_CLIENT_PIDFILE" 2>/dev/null; }

# Extra environment for the VPN process.
#
# Nothing here is required any more: applyEarlyArgs() (main.cpp) evaluates
# --main-config before the first qDebug and the first tiConfMain, setMainConfig()
# recomputes main_gw_cert_cache, and the api socket path arrives via
# --api-socket. HOME stays set anyway so that a regression falls back into the
# test home instead of the real one -- 80_env is the case that deliberately runs
# without it and proves the independence.
CLIENT_EXTRA_ENV=(HOME="$LAB_CLIENT_HOME")

# client_start <profile-name> <log-file>
client_start() {
    local name="$1" log="$2"
    local bin; bin="$(client_require_bin)"
    ensure_sudo
    : >"$log"
    rm -f "$LAB_CLIENT_PIDFILE" "$LAB_CLIENT_RCFILE"

    local envprefix=""
    if (( ${#CLIENT_EXTRA_ENV[@]} )); then
        local assign
        for assign in "${CLIENT_EXTRA_ENV[@]}"; do
            envprefix+="$(printf '%q ' "$assign")"
        done
        envprefix="env $envprefix"
    fi

    # The binary runs as a background job of an sh so that "$!" gives its real
    # PID (in a subshell $$ would be the parent shell's) and "wait" can then
    # capture the exit status. That also makes it possible to send SIGTERM to
    # exactly the PID in the PID file, with no sudo in between.
    as_root sh -c "$envprefix'$bin' --start-vpn --vpn-name '$name' \
            --main-config '$LAB_MAIN_CONF' --api-socket '$LAB_API_SOCK' & \
        echo \$! >'$LAB_CLIENT_PIDFILE'; wait \$!; echo \$? >'$LAB_CLIENT_RCFILE'" \
        >>"$log" 2>&1 &
    disown 2>/dev/null || true

    local waited=0
    while ! [[ -s "$LAB_CLIENT_PIDFILE" ]]; do
        sleep 0.2
        waited=$(( waited + 1 ))
        (( waited > 50 )) && { err "VPN process did not start"; return 1; }
    done
    return 0
}

# Exit status of the finished VPN process; call after the process has ended.
# 0 = clean, 139 = SIGSEGV, 137 = SIGKILL, 143 = unhandled SIGTERM.
client_exit_code() {
    local waited=0
    while [[ ! -s "$LAB_CLIENT_RCFILE" ]] && (( waited < 40 )); do
        sleep 0.25
        waited=$(( waited + 1 ))
    done
    cat "$LAB_CLIENT_RCFILE" 2>/dev/null
}

# client_stop [signal]
# SIGTERM is the clean way: io_loop() installs a handler for it
# (openfortivpn/src/io.c:666) which triggers pppd_terminate + auth_log_out and
# the route restore. vpnWorker::process() checks stopRequested() in its
# persistent branch as well, so this also stops profiles with persistent=true.
client_stop() {
    local sig="${1:-TERM}" timeout="${2:-45}"
    client_running || { rm -f "$LAB_CLIENT_PIDFILE"; return 0; }
    local pid; pid="$(client_pid)"
    as_root kill "-$sig" "$pid" 2>/dev/null || true

    local waited=0
    while pid_alive "$pid" && (( waited < timeout )); do
        sleep 1; waited=$(( waited + 1 ))
    done
    if pid_alive "$pid"; then
        warn "VPN process does not react to SIG$sig, sending SIGKILL"
        as_root kill -9 "$pid" 2>/dev/null || true
        sleep 2
        client_force_cleanup
        rm -f "$LAB_CLIENT_PIDFILE"
        return 1
    fi
    rm -f "$LAB_CLIENT_PIDFILE"
    return 0
}

client_kill() {
    client_running || { rm -f "$LAB_CLIENT_PIDFILE"; return 0; }
    as_root kill -9 "$(client_pid)" 2>/dev/null || true
    sleep 1
    client_force_cleanup
    rm -f "$LAB_CLIENT_PIDFILE"
}

# Clean up leftovers after a hard kill. openfortiGUI's pppd is recognizable by
# its local address :169.254.2.1 (argument list in vpnworker.cpp, pppd_run).
client_force_cleanup() {
    as_root pkill -f 'pppd .*169\.254\.2\.1' 2>/dev/null || true
    local iface
    iface="$(ppp_iface)"
    if [[ -n "$iface" ]]; then
        sleep 1
        iface="$(ppp_iface)"
        [[ -n "$iface" ]] && as_root ip link del "$iface" 2>/dev/null || true
    fi

    # openfortivpn installs a host route to the gateway
    # (ipv4_protect_tunnel_route, openfortivpn/src/ipv4.c:729) and gets the next
    # hop wrong when the gateway is directly attached to a secondary interface:
    # it picks the default gateway instead of the direct connection. If the
    # process dies, that route stays behind and makes the gateway unreachable --
    # every following case then runs into "connect: connection timed out".
    local stale
    stale="$(ip -4 route show "$FGT_WAN_IP" 2>/dev/null | grep -v " dev $LAB_BR_OUT" | head -n1)"
    if [[ -n "$stale" ]]; then
        warn "removing orphaned gateway route: $stale"
        as_root ip route del "$FGT_WAN_IP" 2>/dev/null || true
    fi
}

client_cleanup_all() {
    client_kill
    as_root pkill -f 'openfortigui --start-vpn' 2>/dev/null || true
    client_force_cleanup
}

# client_wait_log <log-file> <regex> <timeout>
client_wait_log() {
    local log="$1" pattern="$2" timeout="${3:-$TIMEOUT_CONNECT}"
    local waited=0
    while ! grep -Eq -- "$pattern" "$log" 2>/dev/null; do
        if (( waited >= timeout )); then return 1; fi
        # Bail out early if the process is already gone and the pattern is absent
        if ! client_running && (( waited > 2 )); then
            grep -Eq -- "$pattern" "$log" 2>/dev/null && return 0
            return 1
        fi
        sleep 1
        waited=$(( waited + 1 ))
    done
    return 0
}

# client_wait_log_count <log-file> <regex> <count> <timeout>
client_wait_log_count() {
    local log="$1" pattern="$2" want="$3" timeout="${4:-$TIMEOUT_RECONNECT}"
    local waited=0 have
    while :; do
        # grep -c prints "0" even without a match; only the exit code is 1
        have="$(grep -Ec -- "$pattern" "$log" 2>/dev/null)"
        (( ${have:-0} >= want )) && return 0
        (( waited >= timeout )) && return 1
        sleep 1
        waited=$(( waited + 1 ))
    done
}

client_wait_exit() {
    local timeout="${1:-30}" waited=0
    while client_running; do
        (( waited >= timeout )) && return 1
        sleep 1
        waited=$(( waited + 1 ))
    done
    return 0
}

# --------------------------------------------------------------------------
# Network state
# --------------------------------------------------------------------------

ppp_iface() {
    ip -o link show 2>/dev/null | awk -F': ' '$2 ~ /^ppp[0-9]+$/ {print $2; exit}'
}

ppp_ip() {
    local iface="${1:-$(ppp_iface)}"
    [[ -n "$iface" ]] || return 1
    ip -4 -o addr show dev "$iface" 2>/dev/null | awk '{print $4}' | cut -d/ -f1 | head -n1
}

_ip2int() {
    local IFS=. ; read -ra o <<<"$1"
    printf '%s' "$(( (o[0] << 24) + (o[1] << 16) + (o[2] << 8) + o[3] ))"
}

ip_in_pool() {
    local ip="$1"
    [[ "$ip" =~ ^[0-9]+\.[0-9]+\.[0-9]+\.[0-9]+$ ]] || return 1
    local v f l
    v="$(_ip2int "$ip")"; f="$(_ip2int "$LAB_POOL_FIRST")"; l="$(_ip2int "$LAB_POOL_LAST")"
    (( v >= f && v <= l ))
}

# Waits until a ppp interface with a pool address exists.
wait_ppp_up() {
    local timeout="${1:-20}" waited=0 iface ip
    while (( waited < timeout )); do
        iface="$(ppp_iface)"
        if [[ -n "$iface" ]]; then
            ip="$(ppp_ip "$iface")"
            [[ -n "$ip" ]] && ip_in_pool "$ip" && return 0
        fi
        sleep 1
        waited=$(( waited + 1 ))
    done
    return 1
}

route_to_dev() {
    ip -o route get "$1" 2>/dev/null | sed -nE 's/.* dev ([^ ]+).*/\1/p' | head -n1
}

default_route_snapshot() {
    ip -4 route show default 2>/dev/null | sort
}

route_snapshot() {
    ip -4 route show 2>/dev/null | grep -v '^default' | sort
}

# RX/TX bytes from /proc/net/dev -- the same source vpnProcess::updateStats uses
iface_bytes() {
    local iface="$1"
    awk -v i="$iface:" '$1 == i { print $2" "$10 }' /proc/net/dev
}

# Reachability of the target behind the FortiGate
inside_reachable() {
    curl -sf -o /dev/null --max-time "${1:-6}" \
        "http://$LAB_INSIDE_IP:$LAB_INSIDE_PORT/"
}
