# shellcheck shell=bash
#
# Scaffolding for the test cases under cases/. Sourced there first; it takes
# care of the libraries, the output directory, the result file and cleanup.
# It also makes every case runnable on its own:
#
#   tests/fortigate-vm/cases/10_connect.sh

LAB_SRC_DIR="${LAB_SRC_DIR:-$(cd "$(dirname "${BASH_SOURCE[1]}")/.." && pwd)}"

# shellcheck source=common.sh
source "$LAB_SRC_DIR/lib/common.sh"
# shellcheck source=vm.sh
source "$LAB_SRC_DIR/lib/vm.sh"
# shellcheck source=net.sh
source "$LAB_SRC_DIR/lib/net.sh"
# shellcheck source=client.sh
source "$LAB_SRC_DIR/lib/client.sh"

CASE_NAME="${CASE_NAME:-$(basename "${BASH_SOURCE[1]}" .sh)}"
CASE_OUT_DIR="${CASE_OUT_DIR:-$LAB_OUT_DIR/$CASE_NAME}"
CASE_RESULT_FILE="${CASE_RESULT_FILE:-$LAB_OUT_DIR/results/$CASE_NAME.tsv}"

mkdir -p "$CASE_OUT_DIR" "$(dirname "$CASE_RESULT_FILE")"
: >"$CASE_RESULT_FILE"

CLIENT_APP_LOG="$LAB_CLIENT_HOME/.openfortigui/logs/openfortigui.log"

case_setup() {
    net_require_up
    vm_running || die "VM is not running"
    client_require_bin >/dev/null
    ensure_sudo
    [[ -f "$LAB_MAIN_CONF" ]] || client_init_home

    # Leftovers from a previous case
    client_cleanup_all

    # The application log is written by root -- truncate it before every case so
    # that assertions do not trip over stale content.
    as_root rm -f "$CLIENT_APP_LOG" 2>/dev/null || true

    if [[ -z "${LAB_GW_DIGEST:-}" ]]; then
        LAB_GW_DIGEST="$(vm_gateway_cert_digest)"
        [[ ${#LAB_GW_DIGEST} -eq 64 ]] \
            || die "cannot read the gateway certificate (TLS handshake failed?)"
    fi
    export LAB_GW_DIGEST
}

case_teardown() {
    local rc=$?
    client_cleanup_all
    # Keep the application log for troubleshooting
    if as_root test -f "$CLIENT_APP_LOG"; then
        as_root cat "$CLIENT_APP_LOG" >"$CASE_OUT_DIR/openfortigui.log" 2>/dev/null || true
    fi
    cp -f "$LAB_INSIDE_LOG" "$CASE_OUT_DIR/inside-http.log" 2>/dev/null || true
    return $rc
}

# case_log <suffix> -> path for a log file belonging to this case
case_log() { printf '%s/%s.log' "$CASE_OUT_DIR" "$1"; }

# The application log is owned by root. Make a copy and print its path so that
# assertions can access it reliably.
case_app_log() {
    local dest="$CASE_OUT_DIR/openfortigui.log"
    as_root cat "$CLIENT_APP_LOG" >"$dest" 2>/dev/null || : >"$dest"
    printf '%s' "$dest"
}

# Sub-section heading in the output
part() { printf '  %s%s%s\n' "$C_BOLD" "$1" "$C_OFF"; }

case_finish() {
    case_summary
    exit $?
}

trap case_teardown EXIT
