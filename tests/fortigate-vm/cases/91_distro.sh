#!/usr/bin/env bash
# lab-requires: vm docker
#
# The package on every distribution it is supposed to work on.
#
# Two things differ between the targets, and both have bitten this project:
#
# * **Which sudo is in charge.** Ubuntu 26.04 ships sudo-rs with the higher
#   update-alternatives priority, so a fresh installation uses it. sudo-rs does
#   not preserve the environment: it accepts -E, warns that it is ignoring it, and
#   carries on. openfortiGUI used to pass -E so the root child would inherit
#   XDG_RUNTIME_DIR and find the GUI's socket -- nothing failed loudly, the GUI
#   just never saw its VPN again (issue #208). The socket path now travels as
#   --api-socket, and this case proves it under each implementation.
#
# * **Which Qt the package is built against.** dh_shlibdeps writes the build
#   host's Qt version into the dependencies, so a package built on 26.04 (Qt 6.10)
#   cannot be installed on 24.04 (Qt 6.4). Every target therefore builds its own
#   package, with packaging/build-deb.sh -- the same script the GitHub workflow
#   uses. That the Qt6 port still compiles against Qt 6.4 is asserted here as a
#   side effect.
#
# Adding a target is one entry in DISTROS below.

set -uo pipefail
source "$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)/lib/case.sh"
# shellcheck source=../lib/docker.sh
source "$LAB_SRC_DIR/lib/docker.sh"

# image | sudo-rs installed and active | classic sudo reachable | KRunner plugin
#
# The only place that holds distribution knowledge. The plugin needs KDE
# Frameworks 6: Ubuntu 24.04 and Debian bookworm ship KF5 only, so there it is not
# built -- and since KF5 support was dropped, not available either.
DISTROS_DEFAULT=(
    "ubuntu:24.04|no|yes|no"
    "ubuntu:26.04|yes|yes|yes"
    "debian:bookworm|no|yes|no"
    "debian:trixie|no|yes|yes"
)

# LAB_DISTROS limits the run, e.g. LAB_DISTROS="debian:trixie"
if [[ -n "${LAB_DISTROS:-}" ]]; then
    read -r -a WANTED <<<"$LAB_DISTROS"
    DISTROS=()
    for want in "${WANTED[@]}"; do
        for entry in "${DISTROS_DEFAULT[@]}"; do
            [[ "${entry%%|*}" == "$want" ]] && DISTROS+=("$entry")
        done
    done
    (( ${#DISTROS[@]} )) || die "no known target matches LAB_DISTROS='$LAB_DISTROS'.
Known: $(printf '%s ' "${DISTROS_DEFAULT[@]%%|*}")"
else
    DISTROS=("${DISTROS_DEFAULT[@]}")
fi

case_setup
dock_require

[[ -c /dev/ppp ]] || { skip "distribution matrix" "/dev/ppp is missing (modprobe ppp_generic)"; case_finish; }

PROFILE="lab-distro"
client_write_profile "$PROFILE" "trusted_cert=$LAB_GW_DIGEST" >/dev/null

for entry in "${DISTROS[@]}"; do
    IFS='|' read -r IMAGE EXP_SUDO_RS EXP_CLASSIC EXP_RUNNER <<<"$entry"
    SLUG="$(dock_slug "$IMAGE")"
    OUT="$CASE_OUT_DIR/$SLUG.log"
    GUILOG="$(case_log "mock-gui-$SLUG")"
    CLIENT_LOG="$LAB_RUN_DIR/distro-$SLUG.log"

    part "$IMAGE"

    dock_cleanup "$IMAGE"
    dock_build "$IMAGE" "$EXP_SUDO_RS" "$EXP_RUNNER"

    # A package from an earlier run is reused unless something in the sources is
    # newer -- building takes minutes, the rest of the case takes seconds.
    if DEB="$(dock_cached_deb "$IMAGE")"; then
        info "reusing $(basename "$DEB")"
        DEB_ARG="$DEB"
    else
        info "building the package inside the container (a few minutes)"
        DEB_ARG="build"
    fi

    # The mock GUI takes the GUI's place on the host: it owns the api socket the
    # container's child process connects back to. That is the connection -E used
    # to be needed for.
    rm -f "$LAB_API_SOCK"
    python3 "$LAB_SRC_DIR/mock_gui.py" watch "$PROFILE" "$CLIENT_LOG" 600 >"$GUILOG" 2>&1 &
    MOCK_PID=$!
    sleep 2

    if dock_run "$IMAGE" "$DEB_ARG" "$PROFILE" "$LAB_MAIN_CONF" "$LAB_API_SOCK" \
            "$CLIENT_LOG" "$(dock_deb_dir "$IMAGE")" "$EXP_RUNNER" \
            "$TIMEOUT_CONNECT" >"$OUT" 2>&1; then
        ok "$IMAGE: the container ran through"
    else
        fail "$IMAGE: the container exited with an error" "$(tail -n 20 "$OUT")"
    fi

    kill "$MOCK_PID" 2>/dev/null || true
    dock_cleanup "$IMAGE"
    client_cleanup_all

    r() { dock_result "$OUT" "$1"; }

    # -- the package ------------------------------------------------------
    if [[ "$DEB_ARG" == "build" ]]; then
        assert_eq "$IMAGE: dpkg-buildpackage succeeds" "ok" "$(r deb_build)"
    fi
    assert_eq "$IMAGE: the package installs, all dependencies resolvable" "ok" "$(r deb_install)"
    assert_eq "$IMAGE: the binary landed in /usr/bin" "yes" "$(r binary_installed)"
    assert_eq "$IMAGE: the sudoers file came with it, mode 0440" "440" "$(r sudoers_mode)"
    info "$IMAGE: $(r distro), Qt $(r qt_version), $(r deb_file)"

    # -- the KRunner plugin ------------------------------------------------
    if [[ "$EXP_RUNNER" == yes ]]; then
        assert_eq "$IMAGE: the KRunner plugin builds" "ok" "$(r runner_build)"
        assert_eq "$IMAGE: the plugin package installs" "ok" "$(r runner_install)"
        # KRunner 6 only finds plugins under Qt's plugin directory.
        PLUGIN="$(r runner_plugin)"
        if [[ "$PLUGIN" == */qt6/plugins/kf6/krunner/krunner_openfortigui.so ]]; then
            ok "$IMAGE: the plugin sits where KRunner looks ($PLUGIN)"
        else
            fail "$IMAGE: the plugin is not in the KRunner plugin directory" \
                "found: ${PLUGIN:-nothing}
Expected below /usr/lib/*/qt6/plugins/kf6/krunner/ -- see KDE_INSTALL_USE_QT_SYS_PATHS
in krunner_openfortigui/debian/rules."
        fi
        assert_eq "$IMAGE: the plugin has no unresolved symbols" "clean" "$(r runner_ldd)"
        assert_eq "$IMAGE: the plugin metadata is embedded" "yes" "$(r runner_metadata)"
    else
        skip "$IMAGE: KRunner plugin" "no KDE Frameworks 6 on this distribution"
    fi

    # -- sudo identity ----------------------------------------------------
    if [[ "$EXP_SUDO_RS" == yes ]]; then
        assert_eq "$IMAGE: sudo is sudo-rs" "sudo-rs" "$(r sudo_impl)"
    else
        assert_eq "$IMAGE: sudo is the classic implementation" "classic" "$(r sudo_impl)"
    fi

    # -- the sudoers rule -------------------------------------------------
    assert_eq "$IMAGE: visudo parses /etc/sudoers.d/openfortigui" "yes" "$(r visudo_ok)"
    [[ "$(r have_sudo_rs)" == yes ]] \
        && assert_eq "$IMAGE: visudo-rs parses it too" "ok" "$(r visudo_rs)"
    assert_eq "$IMAGE: 'sudo -l' works for a member of group sudo" "ok" "$(r sudo_l)"
    assert_eq "$IMAGE: the rule appears in 'sudo -l'" "yes" "$(r sudo_l_rule)"
    assert_eq "$IMAGE: 'sudo -l' reports no parse errors" "clean" "$(r sudo_l_stderr)"
    assert_eq "$IMAGE: the wildcard in '--start-vpn *' takes effect" "yes" "$(r wildcard_allowed)"
    assert_eq "$IMAGE: the rule permits nothing beyond that" "yes" "$(r help_denied)"

    # -- -E, and why it had to go -----------------------------------------
    if [[ "$(r sudo_impl)" == "sudo-rs" ]]; then
        assert_eq "$IMAGE: sudo-rs warns that -E is ignored" "yes" "$(r dash_e_warning)"
        assert_eq "$IMAGE: sudo-rs ignores -E even with a SETENV: rule" "no" "$(r rs_setenv_keeps_env)"
        assert_eq "$IMAGE: the old 'sudo -E' loses the environment" "ignored" "$(r legacy_dash_e)"
    else
        assert_eq "$IMAGE: the old 'sudo -E' is refused outright" "refused" "$(r legacy_dash_e)"
    fi
    assert_eq "$IMAGE: -E does not carry the environment" "no" "$(r dash_e_keeps_env)"
    if [[ "$EXP_CLASSIC" == yes ]]; then
        assert_eq "$IMAGE: the classic sudo refuses -E without SETENV:" "no" "$(r classic_keeps_env)"
        assert_eq "$IMAGE: the classic sudo honours -E with SETENV:" "yes" "$(r classic_setenv_keeps_env)"
    fi

    # -- the real tunnel ---------------------------------------------------
    if [[ "$(r tunnel)" == "up" ]]; then
        ok "$IMAGE: tunnel is up ($(r tunnel_seconds)s)"
    else
        fail "$IMAGE: no tunnel" \
            "The child process is started exactly as vpnManager::startVPN() does it.
container output:
$(grep -vE '^RESULT:' "$OUT" | tail -n 25)
client log:
$(tail -n 20 "$CLIENT_LOG" 2>/dev/null)"
    fi
    assert_eq "$IMAGE: ppp interface present" "yes" "$(r ppp_iface)"

    # The point of the whole exercise: without an inherited environment the child
    # still finds the GUI, because the socket path is passed as an argument.
    assert_contains "$IMAGE: the child reached the GUI on the host" "$GUILOG" 'HELLO'
    assert_contains "$IMAGE: status updates arrive" "$GUILOG" '"status"'

    assert_eq "$IMAGE: no /root/.openfortigui in the container" "absent" "$(r root_tree)"
    assert_not_contains "$IMAGE: the child did not miss the api socket" "$CLIENT_LOG" 'Socket not open'
done

rm -f "$LAB_PROFILE_DIR/$PROFILE.conf"

case_finish
