#!/usr/bin/env bash
#
# Runs inside a distribution container (see Dockerfile) and reports its findings
# as "RESULT:<key>=<value>" lines. The test case on the host turns those into
# assertions -- keeping the bookkeeping outside means the results land in the
# JUnit report like every other check.
#
# Nothing in here may assume a particular distribution. Which sudo exists, and
# under which name, differs: on Ubuntu 26.04 /usr/bin/sudo is sudo-rs via
# update-alternatives and the classic one is /usr/bin/sudo.ws; everywhere else
# there is a single /usr/bin/sudo and no visudo-rs. Everything is therefore
# detected, not assumed.
#
# Usage: in-container.sh <deb|build> <profile> <main-config> <api-socket>
#                        <client-log> <deb-out-dir> <with-runner> [timeout]
#
#   deb|build    path to a .deb to install, or "build" to build it from the source
#                tree mounted at /src using packaging/build-deb.sh
#   with-runner  yes|no -- also build and install the KRunner plugin. It needs KDE
#                Frameworks 6, which Ubuntu 24.04 and Debian bookworm do not have.

set -uo pipefail

DEB="${1:?deb package or the word 'build'}"
PROFILE="${2:?profile name}"
MAIN_CONF="${3:?main config}"
API_SOCK="${4:?api socket}"
CLIENT_LOG="${5:?client log}"
DEB_OUT="${6:?output directory for a built package}"
WITH_RUNNER="${7:-no}"
TIMEOUT="${8:-60}"

BIN=/usr/bin/openfortigui
SUDOERS=/etc/sudoers.d/openfortigui
MARKER_NAME=OFGUI_LAB_MARKER
MARKER_VALUE=keep-me-please

result() { printf 'RESULT:%s=%s\n' "$1" "$2"; }
note()   { printf '        %s\n' "$*"; }

as_labuser() { su labuser -c "$*"; }

# --------------------------------------------------------------------------
# 0) Which distribution, which sudo?
# --------------------------------------------------------------------------

# shellcheck disable=SC1091
. /etc/os-release
result distro "${ID}:${VERSION_ID:-${VERSION_CODENAME:-unknown}}"
note "$PRETTY_NAME"

SUDO_RS_BIN=""
[[ -x /usr/lib/cargo/bin/sudo ]] && SUDO_RS_BIN=/usr/lib/cargo/bin/sudo
if [[ -x /usr/bin/sudo.ws ]]; then
    SUDO_CLASSIC_BIN=/usr/bin/sudo.ws
else
    SUDO_CLASSIC_BIN=/usr/bin/sudo
fi

ACTIVE_IS_RS=no
sudo --version 2>&1 | head -1 | grep -qi 'sudo-rs' && ACTIVE_IS_RS=yes
[[ "$ACTIVE_IS_RS" == yes ]] && result sudo_impl sudo-rs || result sudo_impl classic
[[ -n "$SUDO_RS_BIN" ]] && result have_sudo_rs yes || result have_sudo_rs no

# Is a classic implementation reachable at all? Where sudo itself is classic,
# SUDO_CLASSIC_BIN is that very binary.
if "$SUDO_CLASSIC_BIN" --version 2>&1 | head -1 | grep -qi 'sudo-rs'; then
    result have_classic no
    SUDO_CLASSIC_BIN=""
else
    result have_classic yes
fi
note "sudo: $(sudo --version 2>&1 | head -1)"
note "classic: ${SUDO_CLASSIC_BIN:-none}${SUDO_RS_BIN:+, sudo-rs: $SUDO_RS_BIN}"

# --------------------------------------------------------------------------
# 1) The package: build it or take the one we were given
# --------------------------------------------------------------------------

apt-get update >/tmp/apt-update.log 2>&1 || note "apt-get update failed"

if [[ "$DEB" == "build" ]]; then
    mkdir -p "$DEB_OUT"
    if built="$(/src/packaging/build-deb.sh /src/openfortigui "$DEB_OUT" 2>/tmp/build.log | tail -n1)" \
       && [[ -f "$built" ]]; then
        result deb_build ok
        result deb_file "$(basename "$built")"
        DEB="$built"
    else
        result deb_build fail
        note "$(tail -n 25 /tmp/build.log)"
        result deb_install skipped
        result tunnel skipped
        printf 'RESULT:done=yes\n'
        exit 0
    fi
else
    result deb_build skipped
    result deb_file "$(basename "$DEB")"
fi

if apt-get install -y --no-install-recommends "$DEB" >/tmp/apt.log 2>&1; then
    result deb_install ok
else
    result deb_install fail
    note "$(tail -n 15 /tmp/apt.log)"
fi

if [[ -x "$BIN" ]]; then result binary_installed yes; else result binary_installed no; fi

# Which Qt did it end up linked against? Interesting per distribution.
result qt_version "$(dpkg-query -W -f='${Version}' 'libqt6core6*' 2>/dev/null | head -1)"

# --------------------------------------------------------------------------
# 1b) The KRunner plugin, where KF6 exists
# --------------------------------------------------------------------------

if [[ "$WITH_RUNNER" != yes ]]; then
    result runner skipped
else
    if runner_deb="$(/src/packaging/build-deb.sh /src/krunner_openfortigui "$DEB_OUT" 2>/tmp/runner-build.log | tail -n1)" \
       && [[ -f "$runner_deb" ]]; then
        result runner_build ok
    else
        result runner_build fail
        note "$(tail -n 25 /tmp/runner-build.log)"
    fi

    if [[ -f "${runner_deb:-}" ]] \
       && apt-get install -y --no-install-recommends "$runner_deb" >/tmp/runner-apt.log 2>&1; then
        result runner_install ok
    else
        result runner_install fail
        note "$(tail -n 15 /tmp/runner-apt.log)"
    fi

    # KRunner 6 looks for plugins in Qt's plugin directory under kf6/krunner.
    plugin="$(find /usr/lib -path '*/qt6/plugins/kf6/krunner/krunner_openfortigui.so' 2>/dev/null | head -n1)"
    if [[ -n "$plugin" ]]; then
        result runner_plugin "$plugin"
        # Unresolved symbols would make KRunner refuse the plugin at load time.
        if ldd -r "$plugin" 2>&1 | grep -qiE 'not found|undefined symbol'; then
            result runner_ldd unresolved
            note "$(ldd -r "$plugin" 2>&1 | grep -iE 'not found|undefined' | head -n 5)"
        else
            result runner_ldd clean
        fi
        # KF6 keeps the plugin metadata embedded (CBOR) instead of a .desktop
        # file. grep -a rather than strings: binutils is not in the image.
        if grep -aq 'KPlugin' "$plugin" && grep -aq 'krunner_openfortigui' "$plugin"; then
            result runner_metadata yes
        else
            result runner_metadata no
        fi
    else
        result runner_plugin missing
    fi
    result runner done
fi

# --------------------------------------------------------------------------
# 2) The sudoers file the package ships
# --------------------------------------------------------------------------

if [[ -f "$SUDOERS" ]]; then
    result sudoers_mode "$(stat -c '%a' "$SUDOERS")"
    note "$SUDOERS: $(cat "$SUDOERS")"
else
    result sudoers_mode missing
fi

# The question from #208, asked of whichever parser is present.
if visudo -c -f "$SUDOERS" >/tmp/visudo.log 2>&1; then
    result visudo_ok yes
else
    result visudo_ok no
fi
note "visudo: $(cat /tmp/visudo.log)"

if command -v visudo-rs >/dev/null 2>&1; then
    if visudo-rs -c -f "$SUDOERS" >/tmp/visudo-rs.log 2>&1; then
        result visudo_rs ok
    else
        result visudo_rs fail
    fi
    note "visudo-rs: $(cat /tmp/visudo-rs.log)"
fi

# --------------------------------------------------------------------------
# 3) Does the rule take effect for a member of group sudo?
# --------------------------------------------------------------------------

if as_labuser "sudo -l" >/tmp/sudo-l.log 2>/tmp/sudo-l.err; then
    result sudo_l ok
else
    result sudo_l fail
fi
if grep -q 'openfortigui --start-vpn' /tmp/sudo-l.log; then
    result sudo_l_rule yes
else
    result sudo_l_rule no
fi
# A file the parser dislikes shows up here, not in the exit code
if [[ -s /tmp/sudo-l.err ]]; then
    result sudo_l_stderr dirty
    note "stderr: $(cat /tmp/sudo-l.err)"
else
    result sudo_l_stderr clean
fi

# The wildcard in the command arguments -- "--start-vpn *" -- is the part of the
# rule that is not obviously portable. The real config is used on purpose: a
# non-existent one makes setMainConfig() fall back to $HOME, and as root that
# would litter /root/.openfortigui, which part 6 asserts about.
if as_labuser "sudo -n $BIN --start-vpn --vpn-name __probe__ --main-config '$MAIN_CONF'" \
        >/tmp/wildcard.log 2>&1; then
    result wildcard_allowed yes
else
    if grep -qiE 'not allowed|may not run|authentication' /tmp/wildcard.log; then
        result wildcard_allowed no
    else
        # Permitted, but the program itself refused to run.
        result wildcard_allowed yes
    fi
fi
note "wildcard probe: $(head -n 3 /tmp/wildcard.log)"

# Counter-check: the rule must not be a blank cheque.
if as_labuser "sudo -n $BIN --help" >/tmp/help.log 2>&1; then
    result help_denied no
else
    result help_denied yes
fi

# --------------------------------------------------------------------------
# 4) -E, the flag openfortiGUI used to depend on
# --------------------------------------------------------------------------

# env_probe <sudo binary> <command> -- output including stderr
env_probe() {
    as_labuser "$MARKER_NAME=$MARKER_VALUE $1 -n -E $2" 2>&1
}
kept() { grep -q "$MARKER_NAME=$MARKER_VALUE" <<<"$1"; }

# The sudo that is actually in charge here.
ACTIVE_OUT="$(env_probe sudo /usr/bin/env)"
if grep -q "is ignored" <<<"$ACTIVE_OUT"; then
    result dash_e_warning yes
else
    result dash_e_warning no
fi
if kept "$ACTIVE_OUT"; then result dash_e_keeps_env yes; else result dash_e_keeps_env no; fi
note "active sudo -E: $(head -n 1 <<<"$ACTIVE_OUT")"

# The classic implementation, wherever it lives: without the SETENV: tag it does
# not merely ignore -E, it refuses to run at all. openfortiGUI's own rule never
# carried the tag -- so the -E it used to pass was never reliable.
if [[ -n "$SUDO_CLASSIC_BIN" ]]; then
    WS_OUT="$(env_probe "$SUDO_CLASSIC_BIN" /usr/bin/env)"
    if kept "$WS_OUT"; then result classic_keeps_env yes; else result classic_keeps_env no; fi
    note "classic -E without SETENV: $(head -n 1 <<<"$WS_OUT")"

    WS_SETENV_OUT="$(env_probe "$SUDO_CLASSIC_BIN" /usr/local/bin/envprobe)"
    if kept "$WS_SETENV_OUT"; then
        result classic_setenv_keeps_env yes
    else
        result classic_setenv_keeps_env no
        note "classic -E with SETENV: $(head -n 1 <<<"$WS_SETENV_OUT")"
    fi
fi

# sudo-rs ignores -E even when the rule explicitly allows it.
if [[ -n "$SUDO_RS_BIN" ]]; then
    RS_SETENV_OUT="$(env_probe "$SUDO_RS_BIN" /usr/local/bin/envprobe)"
    if kept "$RS_SETENV_OUT"; then
        result rs_setenv_keeps_env yes
    else
        result rs_setenv_keeps_env no
    fi
fi

# What the code used to do, against the rule the package actually ships.
LEGACY="$(as_labuser "sudo -n -E $BIN --start-vpn --vpn-name __probe__ --main-config '$MAIN_CONF'" 2>&1)"
if grep -q 'is ignored' <<<"$LEGACY"; then
    result legacy_dash_e ignored
elif grep -qi 'not allowed to preserve the environment' <<<"$LEGACY"; then
    result legacy_dash_e refused
else
    result legacy_dash_e carried
fi
note "legacy 'sudo -E openfortigui': $(head -n 1 <<<"$LEGACY")"

# --------------------------------------------------------------------------
# 5) The real thing: a tunnel started the way the GUI starts it
# --------------------------------------------------------------------------

if [[ ! -x "$BIN" ]]; then
    result tunnel skipped
else
    # Truncated as labuser, not as root: otherwise the file ends up root-owned
    # and the child process cannot append to it.
    as_labuser ": >'$CLIENT_LOG'" 2>/dev/null || true
    # Exactly the command line vpnManager::startVPN() builds -- minus the -E it
    # used to carry.
    as_labuser "sudo -n $BIN --start-vpn --vpn-name '$PROFILE' \
        --main-config '$MAIN_CONF' --api-socket '$API_SOCK' >>'$CLIENT_LOG' 2>&1 &"

    waited=0
    tunnel=down
    while (( waited < TIMEOUT )); do
        if grep -q 'Tunnel is up and running' "$CLIENT_LOG" 2>/dev/null; then
            tunnel=up
            break
        fi
        sleep 1
        waited=$(( waited + 1 ))
    done
    result tunnel "$tunnel"
    result tunnel_seconds "$waited"

    if ip -4 addr show 2>/dev/null | grep -q 'ppp'; then
        result ppp_iface yes
    else
        result ppp_iface no
    fi

    [[ "$tunnel" == "up" ]] || note "$(tail -n 20 "$CLIENT_LOG" 2>/dev/null)"

    # Let the host observe the connection for a moment, then take it down again.
    sleep 5
    pkill -f 'openfortigui --start-vpn' 2>/dev/null || true
    sleep 3
fi

# --------------------------------------------------------------------------
# 6) Did anything land in root's home?
# --------------------------------------------------------------------------

if [[ -e /root/.openfortigui ]]; then
    result root_tree present
    note "$(find /root/.openfortigui | head -n 10)"
else
    result root_tree absent
fi

printf 'RESULT:done=yes\n'
