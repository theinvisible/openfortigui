# shellcheck shell=bash
#
# Shared helpers for the openfortiGUI FortiGate test lab.
# Sourced by "testlab" and by every test case under cases/.

set -o pipefail

# --------------------------------------------------------------------------
# Paths and configuration
# --------------------------------------------------------------------------

LAB_SRC_DIR="${LAB_SRC_DIR:-$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)}"
REPO_ROOT="${REPO_ROOT:-$(cd "$LAB_SRC_DIR/../.." && pwd)}"
export LAB_SRC_DIR REPO_ROOT

if [[ -f "$LAB_SRC_DIR/lab.env" ]]; then
    # shellcheck disable=SC1091
    source "$LAB_SRC_DIR/lab.env"
fi

: "${OFGUI_LAB_DIR:=$HOME/.cache/openfortigui-testlab}"
: "${OFGUI_BIN:=}"

: "${FGT_IMAGE_ZIP:=}"
: "${FGT_LICENSE:=}"

: "${FGT_VM_NAME:=ofgt-lab}"
: "${FGT_CPUS:=1}"
: "${FGT_MEM_MB:=2048}"
: "${FGT_MACHINE:=q35}"
: "${FGT_NIC_MODEL:=virtio-net-pci}"
: "${FGT_LOG_DISK_GB:=0}"

: "${LAB_BR_OUT:=ofgt-out}"
: "${LAB_TAP_OUT:=ofgt-tap0}"
: "${LAB_OUT_NET:=10.99.99.0/24}"
: "${LAB_HOST_IP:=10.99.99.1}"
: "${FGT_WAN_IP:=10.99.99.10}"

: "${LAB_BR_IN:=ofgt-in}"
: "${LAB_TAP_IN:=ofgt-tap1}"
: "${LAB_NETNS:=ofgt-inside}"
: "${LAB_VETH_HOST:=ofgt-veth}"
: "${LAB_VETH_NS:=ofgt-vethns}"
: "${FGT_LAN_IP:=10.99.10.1}"
: "${LAB_INSIDE_IP:=10.99.10.50}"
: "${LAB_INSIDE_PORT:=8080}"
: "${LAB_INSIDE_NET:=10.99.10.0/24}"

: "${LAB_POOL_FIRST:=10.212.134.200}"
: "${LAB_POOL_LAST:=10.212.134.210}"

: "${FGT_HOSTNAME:=FGT-LAB}"
: "${FGT_ADMIN_USER:=admin}"
: "${FGT_ADMIN_PASS:=LabAdmin#2024}"
: "${FGT_ADMIN_SPORT:=443}"
: "${FGT_SSLVPN_PORT:=10443}"

: "${VPN_USER:=vpnuser}"
: "${VPN_PASS:=LabVpn#2024}"
: "${VPN_GROUP:=sslvpn-lab}"
: "${VPN_PORTAL:=lab-portal}"
: "${FGT_POLICY_NAT:=0}"

: "${OFGUI_TEST_DNS:=0}"
: "${TIMEOUT_BOOT:=300}"
: "${TIMEOUT_TLS:=180}"
: "${TIMEOUT_CONNECT:=60}"
: "${TIMEOUT_RECONNECT:=120}"

# AES defaults from openfortigui/config.h -- these let us produce the profile
# password with "openssl enc" without starting the GUI.
: "${OFGUI_AESKEY:=yowp2IwTTRodgdWp}"
: "${OFGUI_AESIV:=VoUT5n5ToogkmQU3}"

LAB_IMG_DIR="$OFGUI_LAB_DIR/img"
LAB_RUN_DIR="$OFGUI_LAB_DIR/run"
LAB_OUT_DIR="$OFGUI_LAB_DIR/out"
LAB_CLIENT_HOME="$OFGUI_LAB_DIR/client/home"
LAB_MAIN_CONF="$LAB_CLIENT_HOME/.openfortigui/main.conf"
LAB_PROFILE_DIR="$LAB_CLIENT_HOME/.openfortigui/vpnprofiles"
LAB_CONSOLE_SOCK="$LAB_RUN_DIR/console.sock"
LAB_QMP_SOCK="$LAB_RUN_DIR/qmp.sock"
LAB_QEMU_PID="$LAB_RUN_DIR/qemu.pid"
LAB_STATE="$LAB_RUN_DIR/state.env"

# --------------------------------------------------------------------------
# Output
# --------------------------------------------------------------------------

if [[ -t 1 && "${NO_COLOR:-}" == "" ]]; then
    C_RED=$'\033[31m'; C_GREEN=$'\033[32m'; C_YELLOW=$'\033[33m'
    C_BLUE=$'\033[34m'; C_BOLD=$'\033[1m'; C_DIM=$'\033[2m'; C_OFF=$'\033[0m'
else
    C_RED=''; C_GREEN=''; C_YELLOW=''; C_BLUE=''; C_BOLD=''; C_DIM=''; C_OFF=''
fi

_ts() { date '+%H:%M:%S'; }

step() { printf '%s\n' "${C_BOLD}${C_BLUE}==>${C_OFF} ${C_BOLD}$*${C_OFF}"; }
info() { printf '%s\n' "${C_DIM}[$(_ts)]${C_OFF} $*"; }
warn() { printf '%s\n' "${C_YELLOW}[warn]${C_OFF} $*" >&2; }
err()  { printf '%s\n' "${C_RED}[error]${C_OFF} $*" >&2; }
die()  { err "$*"; exit 1; }

have() { command -v "$1" >/dev/null 2>&1; }

# --------------------------------------------------------------------------
# Root privileges
# --------------------------------------------------------------------------

if [[ ${EUID:-$(id -u)} -eq 0 ]]; then
    SUDO_CMD=()
else
    SUDO_CMD=(sudo)
fi

as_root() {
    if [[ ${#SUDO_CMD[@]} -eq 0 ]]; then
        "$@"
    else
        "${SUDO_CMD[@]}" "$@"
    fi
}

# Grab a sudo ticket up front so that no password prompt interrupts a test
# half way through.
ensure_sudo() {
    [[ ${#SUDO_CMD[@]} -eq 0 ]] && return 0
    if sudo -n true 2>/dev/null; then return 0; fi
    info "root privileges are required (network setup and VPN client)."
    sudo -v || die "sudo failed."
}

# --------------------------------------------------------------------------
# Small utilities
# --------------------------------------------------------------------------

lab_mkdirs() {
    mkdir -p "$LAB_IMG_DIR" "$LAB_RUN_DIR" "$LAB_OUT_DIR" \
             "$LAB_CLIENT_HOME/.openfortigui"
}

# hex_of <string> -> hex representation (for openssl -K/-iv)
hex_of() { printf '%s' "$1" | od -An -tx1 | tr -d ' \n'; }

# pid_alive <pid>
# Deliberately via /proc rather than "kill -0": the VPN and netns processes run
# as root, and kill -0 returns EPERM for those to an unprivileged caller --
# i.e. a false "does not exist".
pid_alive() {
    [[ -n "${1:-}" ]] && [[ -d "/proc/$1" ]]
}

# pid_cmdline_has <pid> <pattern>  -- guards against recycled PIDs
pid_cmdline_has() {
    local pid="$1" needle="$2"
    [[ -r "/proc/$pid/cmdline" ]] || return 0
    tr '\0' ' ' <"/proc/$pid/cmdline" 2>/dev/null | grep -qF -- "$needle"
}

# wait_for <timeout-seconds> <description> <command...>
# Polls once per second until the command succeeds.
wait_for() {
    local timeout="$1" desc="$2"; shift 2
    local waited=0
    while ! "$@" >/dev/null 2>&1; do
        if (( waited >= timeout )); then
            err "timed out after ${timeout}s: $desc"
            return 1
        fi
        sleep 1
        waited=$(( waited + 1 ))
        if (( waited % 15 == 0 )); then
            info "waiting for $desc (${waited}/${timeout}s)"
        fi
    done
    return 0
}

# tcp_open <host> <port>
tcp_open() {
    timeout 2 bash -c "exec 3<>/dev/tcp/$1/$2" 2>/dev/null
}

# --------------------------------------------------------------------------
# Assertions for test cases
#
# Every case runs as its own process. Results are written to $CASE_RESULT_FILE
# as TSV (status<TAB>name<TAB>detail) and aggregated into junit.xml by the
# runner.
# --------------------------------------------------------------------------

CASE_CHECKS_OK=0
CASE_CHECKS_FAILED=0

_case_record() {
    local status="$1" name="$2" detail="${3:-}"
    [[ -n "${CASE_RESULT_FILE:-}" ]] || return 0
    printf '%s\t%s\t%s\n' "$status" "$name" "${detail//$'\n'/ | }" >>"$CASE_RESULT_FILE"
}

ok() {
    CASE_CHECKS_OK=$(( CASE_CHECKS_OK + 1 ))
    printf '   %sok%s   %s\n' "$C_GREEN" "$C_OFF" "$1"
    _case_record ok "$1"
}

fail() {
    CASE_CHECKS_FAILED=$(( CASE_CHECKS_FAILED + 1 ))
    printf '   %sFAIL%s %s\n' "$C_RED" "$C_OFF" "$1"
    [[ -n "${2:-}" ]] && printf '        %s\n' "${2//$'\n'/$'\n'        }"
    _case_record fail "$1" "${2:-}"
}

skip() {
    printf '   %sskip%s %s%s\n' "$C_YELLOW" "$C_OFF" "$1" "${2:+ ($2)}"
    _case_record skip "$1" "${2:-}"
}

# assert_true <name> <command...>
assert_true() {
    local name="$1"; shift
    local out
    if out="$("$@" 2>&1)"; then
        ok "$name"
    else
        fail "$name" "$*
$out"
    fi
}

# assert_false <name> <command...>
assert_false() {
    local name="$1"; shift
    local out
    if out="$("$@" 2>&1)"; then
        fail "$name" "command unexpectedly succeeded: $*
$out"
    else
        ok "$name"
    fi
}

# assert_eq <name> <expected> <actual>
assert_eq() {
    if [[ "$2" == "$3" ]]; then
        ok "$1"
    else
        fail "$1" "expected: '$2'
actual:   '$3'"
    fi
}

# assert_contains <name> <file> <extended regex>
assert_contains() {
    local name="$1" file="$2" pattern="$3"
    if [[ -f "$file" ]] && grep -Eq -- "$pattern" "$file"; then
        ok "$name"
    else
        local tail=""
        [[ -f "$file" ]] && tail="$(tail -n 15 "$file")"
        fail "$name" "pattern '$pattern' not found in $file
--- last lines ---
$tail"
    fi
}

# assert_not_contains <name> <file> <extended regex>
assert_not_contains() {
    local name="$1" file="$2" pattern="$3"
    if [[ -f "$file" ]] && grep -Eq -- "$pattern" "$file"; then
        fail "$name" "pattern '$pattern' unexpectedly found:
$(grep -En -- "$pattern" "$file" | head -n 5)"
    else
        ok "$name"
    fi
}

case_summary() {
    local total=$(( CASE_CHECKS_OK + CASE_CHECKS_FAILED ))
    if (( CASE_CHECKS_FAILED == 0 )); then
        printf '   %s%d/%d checks passed%s\n' "$C_GREEN" "$CASE_CHECKS_OK" "$total" "$C_OFF"
        return 0
    fi
    printf '   %s%d/%d checks failed%s\n' \
        "$C_RED" "$CASE_CHECKS_FAILED" "$total" "$C_OFF"
    return 1
}
