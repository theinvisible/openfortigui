#!/usr/bin/env bash
#
# The VPN process must not depend on an inherited environment.
#
# It runs as root under sudo, so its environment is not the GUI's. Everything it
# needs therefore has to be handed over explicitly:
#
#   --main-config   config, profiles, logs and the certificate cache
#   --api-socket    the QLocalServer the GUI listens on
#
# It used to be different. The socket was opened by plain name, which Qt resolves
# against the runtime location -- /run/user/1000 for the GUI, /run/user/0 or
# /tmp/runtime-root for the root child. Only "sudo -E" held the two together,
# and with it the whole status channel: no status updates, no OTP prompt, no
# credential dialog (issues #158, #107, #179, #132). sudo-rs on Ubuntu 26.04 does
# not support -E at all, which broke connecting outright (#208).
#
# This case runs the client with a deliberately WRONG HOME. Anything still
# derived from HOME lands in a directory that does not exist, so it fails
# loudly instead of silently working on the test machine.

set -uo pipefail
source "$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)/lib/case.sh"
case_setup

MOCK="$LAB_SRC_DIR/mock_gui.py"
BAD_HOME="/nonexistent-openfortigui-env-test"
ROOT_LOG="/root/.openfortigui/logs/openfortigui.log"

# Modification time of the root log, or "absent". Older openfortiGUI versions
# left a whole tree in /root/.openfortigui behind, so its mere existence proves
# nothing -- only that this run did not touch it ("testlab clean" removes such
# leftovers).
root_log_state() {
    as_root stat -c %Y "$ROOT_LOG" 2>/dev/null || printf 'absent'
}

# env takes these verbatim: XDG_RUNTIME_DIR gone, HOME pointing nowhere.
# sudo with env_reset usually strips XDG_RUNTIME_DIR anyway -- being explicit
# keeps the case honest if that ever changes.
CLIENT_EXTRA_ENV=(-u XDG_RUNTIME_DIR HOME="$BAD_HOME")

# --------------------------------------------------------------------------
part "a) connect with a scrubbed environment"
# --------------------------------------------------------------------------

PROFILE="lab-env-scrubbed"
LOG="$(case_log client)"
MOCKOUT="$CASE_OUT_DIR/mockgui-watch.log"

client_write_profile "$PROFILE" "trusted_cert=$LAB_GW_DIGEST" >/dev/null
info "HOME=$BAD_HOME, no XDG_RUNTIME_DIR, api socket $LAB_API_SOCK"

ROOT_LOG_BEFORE="$(root_log_state)"
CONF_OWNER_BEFORE="$(stat -c '%u:%g' "$LAB_MAIN_CONF")"

MOCK_PID=""
if [[ -f "$MOCK" ]]; then
    python3 "$MOCK" watch "$PROFILE" "$LOG" 180 >"$MOCKOUT" 2>&1 &
    MOCK_PID=$!
    sleep 1
else
    skip "GUI stand-in" "mock_gui.py is missing"
fi

client_start "$PROFILE" "$LOG" || fail "VPN process did not start"

if client_wait_log "$LOG" 'Tunnel is up and running' "$TIMEOUT_CONNECT"; then
    ok "tunnel is up despite a wrong HOME"
else
    fail "tunnel did not come up with a scrubbed environment" \
        "If the log complains about a missing profile, --main-config is not
taking effect early enough -- see applyEarlyArgs() in main.cpp.
$(tail -n 25 "$LOG" 2>/dev/null)"
    [[ -n "$MOCK_PID" ]] && kill "$MOCK_PID" 2>/dev/null
    client_cleanup_all
    case_finish
fi

# The point of the whole exercise: the child found the GUI without sharing its
# environment.
if [[ -n "$MOCK_PID" ]]; then
    if grep -q 'HELLO' "$MOCKOUT" 2>/dev/null; then
        ok "child registered with the GUI stand-in over --api-socket"
    else
        fail "no HELLO at the GUI stand-in" \
            "The child did not reach $LAB_API_SOCK, so the GUI would see no
status updates and show no credential/OTP dialog.
$(cat "$MOCKOUT" 2>/dev/null)"
    fi
    assert_true "api socket was created at the handed-over path" test -S "$LAB_API_SOCK"

    # vpn_start is set when the tunnel comes up (vpnprocess.cpp, STATE_UP) and
    # feeds the "Connected" column. It stayed 0 for years, so the column could
    # not be built at all (issue #185). The statistics timer runs every 2 s, so
    # give it a few rounds. Qt writes JSON indented -- mind the space.
    VPN_START_RE='"vpn_start":[[:space:]]*[1-9][0-9]{8,}'
    waited=0
    while ! grep -qE "$VPN_START_RE" "$MOCKOUT" 2>/dev/null && (( waited < 15 )); do
        sleep 1; waited=$(( waited + 1 ))
    done
    if grep -qE "$VPN_START_RE" "$MOCKOUT" 2>/dev/null; then
        ok "a non-zero vpn_start arrives at the GUI"
    else
        fail "no usable vpn_start in the statistics" \
            "Expected \"vpn_start\": <unix timestamp>, got:
$(grep -o 'recv.*' "$MOCKOUT" | head -n 5)"
    fi
fi

# The application log follows --main-config, not HOME (setMainConfig recomputes
# the paths, and applyEarlyArgs runs before the first qDebug).
APPLOG="$(case_app_log)"
assert_true "application log written into the test home" test -s "$APPLOG"
assert_not_contains "no Critical/Fatal messages" "$APPLOG" 'openfortiGUI::(Critical|Fatal)'
assert_false "no stray directory in the wrong HOME" as_root test -d "$BAD_HOME"
assert_eq "nothing written to /root/.openfortigui/logs" \
    "$ROOT_LOG_BEFORE" "$(root_log_state)"

# The root child must not rewrite main.conf. QSettings replaces the file
# atomically, so a write as root changes its owner -- after that
# tiConfMain::isWritable() is false and the GUI's settings dialog stays disabled
# permanently. initMainConf() therefore migrates only when geteuid() != 0.
assert_eq "main.conf still belongs to the user" \
    "$CONF_OWNER_BEFORE" "$(stat -c '%u:%g' "$LAB_MAIN_CONF")"

client_stop TERM 30 >/dev/null 2>&1 || true
[[ -n "$MOCK_PID" ]] && { kill "$MOCK_PID" 2>/dev/null; wait "$MOCK_PID" 2>/dev/null; }
client_cleanup_all

# --------------------------------------------------------------------------
part "b) gw_cert.cache follows --main-config, not HOME"
# --------------------------------------------------------------------------

# tiConfMain::main_gw_cert_cache is initialized statically (ticonfmain.cpp:32);
# setMainConfig() has to recompute it, otherwise trust_all_gw_certs reads the
# cache relative to HOME -- root's /root in the child process. 30_cert d) covers
# the same path but with a correct HOME, so only this part actually pins it down.
CACHE="$LAB_CLIENT_HOME/.openfortigui/gw_cert.cache"
LOG_B="$(case_log client-cache)"

as_root tee "$CACHE" >/dev/null <<EOF
[gw_cert_hashes]
lab-env-cache=$LAB_GW_DIGEST
EOF

client_write_profile "lab-env-cache" "trusted_cert=" "trust_all_gw_certs=true" >/dev/null
client_start "lab-env-cache" "$LOG_B" || fail "VPN process did not start"

if client_wait_log "$LOG_B" 'Tunnel is up and running' "$TIMEOUT_CONNECT"; then
    ok "hash from gw_cert.cache used with a wrong HOME"
else
    fail "connecting with the gw_cert.cache hash failed" \
        "The cache was probably looked for outside $LAB_CLIENT_HOME --
setMainConfig() must recompute main_gw_cert_cache from
openfortigui_config::file_gw_cert_cache.
$(tail -n 25 "$LOG_B" 2>/dev/null)"
fi

client_stop TERM 30 >/dev/null 2>&1 || true
as_root rm -f "$CACHE"

case_finish
