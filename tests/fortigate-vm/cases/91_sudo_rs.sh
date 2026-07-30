#!/usr/bin/env bash
# lab-requires: vm docker
#
# Issue #208: openfortiGUI on Ubuntu 26.04.
#
# 26.04 ships sudo-rs alongside the classic sudo and gives it the higher
# update-alternatives priority, so a fresh installation uses it. sudo-rs does not
# support preserving the whole environment: it takes -E, prints "preserving the
# entire environment is not supported, '-E' is ignored" and carries on without it.
# Nothing fails loudly -- the root child process simply loses XDG_RUNTIME_DIR,
# looks for the api socket under /run/user/0 and never finds the GUI. No status
# updates, no dialogs.
#
# openfortiGUI no longer passes -E and hands the socket over with --api-socket
# instead. This case proves both halves inside a container that is exactly that
# environment: that the shipped sudoers rule works under sudo-rs, and that a real
# tunnel comes up through it.
#
# The container shares the host's network and pid namespace (see lib/docker.sh) --
# what is isolated is the user space and the packaging, not the network.

set -uo pipefail
source "$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)/lib/case.sh"
# shellcheck source=../lib/docker.sh
source "$LAB_SRC_DIR/lib/docker.sh"

case_setup
dock_require
dock_cleanup

if ! DEB="$(dock_find_deb)"; then
    skip "sudo-rs container" "no package found. Build it with:
    cd '$REPO_ROOT/openfortigui' && dpkg-buildpackage -b -uc -us
or point OFGUI_DEB at an existing .deb."
    case_finish
fi
info "package under test: $(basename "$DEB")"

[[ -c /dev/ppp ]] || { skip "sudo-rs container" "/dev/ppp is missing (modprobe ppp_generic)"; case_finish; }

dock_build

PROFILE="lab-sudors"
CLIENT_LOG="$LAB_RUN_DIR/sudors-client.log"
OUT="$CASE_OUT_DIR/container.log"
GUILOG="$(case_log mock-gui)"

client_write_profile "$PROFILE" "trusted_cert=$LAB_GW_DIGEST" >/dev/null

# The mock GUI takes the GUI's place on the host: it owns the api socket the
# container's child process connects back to. That is the connection -E used to
# be needed for.
rm -f "$LAB_API_SOCK"
python3 "$LAB_SRC_DIR/mock_gui.py" watch "$PROFILE" "$CLIENT_LOG" 150 >"$GUILOG" 2>&1 &
MOCK_PID=$!
sleep 2

info "starting the container (this installs the package and connects)"
if dock_run "$DEB" "$PROFILE" "$LAB_MAIN_CONF" "$LAB_API_SOCK" "$CLIENT_LOG" \
        "$TIMEOUT_CONNECT" >"$OUT" 2>&1; then
    ok "the container ran through"
else
    fail "the container exited with an error" "$(tail -n 20 "$OUT")"
fi

kill "$MOCK_PID" 2>/dev/null || true
dock_cleanup

# --------------------------------------------------------------------------
part "a) is sudo-rs really what we are testing?"
# --------------------------------------------------------------------------

assert_eq "sudo in the container is sudo-rs" "sudo-rs" "$(dock_result "$OUT" sudo_impl)"

# --------------------------------------------------------------------------
part "b) the package on Ubuntu 26.04"
# --------------------------------------------------------------------------

assert_eq "the package installs, all dependencies resolvable" "ok" "$(dock_result "$OUT" deb_install)"
assert_eq "the binary landed in /usr/bin" "yes" "$(dock_result "$OUT" binary_installed)"
assert_eq "the sudoers file came with it, mode 0440" "440" "$(dock_result "$OUT" sudoers_mode)"

# --------------------------------------------------------------------------
part "c) does sudo-rs understand the sudoers file?"
# --------------------------------------------------------------------------

# This is the question the issue asks: the file has no SETENV: tag, but it does
# use a wildcard in the command arguments.
assert_eq "visudo-rs parses /etc/sudoers.d/openfortigui" "ok" "$(dock_result "$OUT" visudo_rs)"
assert_eq "'sudo -l' works for a member of group sudo" "ok" "$(dock_result "$OUT" sudo_l)"
assert_eq "the rule appears in 'sudo -l'" "yes" "$(dock_result "$OUT" sudo_l_rule)"
assert_eq "'sudo -l' reports no parse errors" "clean" "$(dock_result "$OUT" sudo_l_stderr)"
assert_eq "the wildcard in '--start-vpn *' takes effect" "yes" "$(dock_result "$OUT" wildcard_allowed)"
assert_eq "the rule permits nothing beyond that" "yes" "$(dock_result "$OUT" help_denied)"

# --------------------------------------------------------------------------
part "d) -E, and why it had to go"
# --------------------------------------------------------------------------

# sudo-rs: not a hard error but a warning, which is what made the bug so hard to
# see -- the process starts, it just cannot find the GUI afterwards.
assert_eq "sudo-rs warns that -E is ignored" "yes" "$(dock_result "$OUT" dash_e_warning)"
assert_eq "sudo-rs does NOT pass the environment through" "no" "$(dock_result "$OUT" dash_e_keeps_env)"
assert_eq "sudo-rs ignores -E even with a SETENV: rule" "no" "$(dock_result "$OUT" rs_setenv_keeps_env)"

# The classic sudo is stricter than expected: -E needs the SETENV: tag in the
# matching rule, otherwise it refuses to run the command at all. With the tag it
# works. So the difference is the tag, not the implementation.
assert_eq "the classic sudo refuses -E without SETENV:" "no" "$(dock_result "$OUT" classic_keeps_env)"
assert_eq "the classic sudo honours -E with SETENV:" "yes" "$(dock_result "$OUT" classic_setenv_keeps_env)"

# And this is the verdict on the code as it used to be: "sudo -E openfortigui
# --start-vpn ...", against the sudoers rule the package actually ships (which has
# no SETENV: tag). sudo-rs drops the environment, the classic sudo refuses to run
# at all -- so the -E was never a working way to reach the GUI's socket.
assert_eq "the old 'sudo -E' loses the environment under sudo-rs" \
    "ignored" "$(dock_result "$OUT" legacy_dash_e_rs)"
assert_eq "the old 'sudo -E' is refused outright by the classic sudo" \
    "refused" "$(dock_result "$OUT" legacy_dash_e_classic)"

# --------------------------------------------------------------------------
part "e) a real tunnel through sudo-rs"
# --------------------------------------------------------------------------

TUNNEL="$(dock_result "$OUT" tunnel)"
if [[ "$TUNNEL" == "up" ]]; then
    ok "tunnel is up, started via sudo-rs ($(dock_result "$OUT" tunnel_seconds)s)"
else
    fail "no tunnel through sudo-rs" \
        "The child process is started exactly as vpnManager::startVPN() does it.
container output:
$(grep -vE '^RESULT:' "$OUT" | tail -n 25)
client log:
$(tail -n 20 "$CLIENT_LOG" 2>/dev/null)"
fi

assert_eq "ppp interface present" "yes" "$(dock_result "$OUT" ppp_iface)"

# The point of the whole exercise: without an inherited environment the child
# still finds the GUI, because the socket path is passed as an argument.
assert_contains "the child reached the GUI on the host" "$GUILOG" 'HELLO'
assert_contains "status updates arrive" "$GUILOG" '"status"'

# --------------------------------------------------------------------------
part "f) nothing left in root's home"
# --------------------------------------------------------------------------

assert_eq "no /root/.openfortigui in the container" "absent" "$(dock_result "$OUT" root_tree)"

APPLOG="$(case_app_log)"
assert_not_contains "the child did not miss the api socket" "$CLIENT_LOG" 'Socket not open'

client_cleanup_all
rm -f "$LAB_PROFILE_DIR/$PROFILE.conf"

case_finish
