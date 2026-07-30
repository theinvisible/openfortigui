#!/usr/bin/env bash
#
# Runs inside the sudo-rs container (see Dockerfile) and reports its findings as
# "RESULT:<key>=<value>" lines. The test case on the host turns those into
# assertions -- keeping the bookkeeping outside means the results land in the
# JUnit report like every other check.
#
# Usage: in-container.sh <deb> <profile> <main-config> <api-socket> <client-log> [timeout]

set -uo pipefail

DEB="${1:?deb package}"
PROFILE="${2:?profile name}"
MAIN_CONF="${3:?main config}"
API_SOCK="${4:?api socket}"
CLIENT_LOG="${5:?client log}"
TIMEOUT="${6:-60}"

BIN=/usr/bin/openfortigui
SUDOERS=/etc/sudoers.d/openfortigui
MARKER_NAME=OFGUI_LAB_MARKER
MARKER_VALUE=keep-me-please

result() { printf 'RESULT:%s=%s\n' "$1" "$2"; }
note()   { printf '        %s\n' "$*"; }

as_labuser() { su labuser -c "$*"; }

# --------------------------------------------------------------------------
# 1) Are we really testing sudo-rs?
# --------------------------------------------------------------------------

if sudo --version 2>&1 | head -1 | grep -qi 'sudo-rs'; then
    result sudo_impl sudo-rs
else
    result sudo_impl "other"
fi
note "sudo --version: $(sudo --version 2>&1 | head -1)"

# --------------------------------------------------------------------------
# 2) Install the package under test
# --------------------------------------------------------------------------

# The image carries no package lists (they go stale), so refresh them here.
apt-get update >/tmp/apt-update.log 2>&1 || note "apt-get update failed"

if apt-get install -y --no-install-recommends "$DEB" >/tmp/apt.log 2>&1; then
    result deb_install ok
else
    result deb_install fail
    note "$(tail -n 15 /tmp/apt.log)"
fi

if [[ -x "$BIN" ]]; then result binary_installed yes; else result binary_installed no; fi

# --------------------------------------------------------------------------
# 3) The sudoers file the package ships
# --------------------------------------------------------------------------

if [[ -f "$SUDOERS" ]]; then
    result sudoers_mode "$(stat -c '%a' "$SUDOERS")"
    note "$SUDOERS: $(cat "$SUDOERS")"
else
    result sudoers_mode missing
fi

# The actual question from #208: does sudo-rs' own parser accept the file?
if visudo-rs -c -f "$SUDOERS" >/tmp/visudo.log 2>&1; then
    result visudo_rs ok
else
    result visudo_rs fail
fi
note "visudo-rs: $(cat /tmp/visudo.log)"

# --------------------------------------------------------------------------
# 4) Does the rule take effect for a member of group sudo?
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
# A file sudo-rs cannot parse shows up here, not in the exit code
if [[ -s /tmp/sudo-l.err ]]; then
    result sudo_l_stderr dirty
    note "stderr: $(cat /tmp/sudo-l.err)"
else
    result sudo_l_stderr clean
fi

# The wildcard in the command arguments -- "--start-vpn *" -- is the part of the
# rule that is not obviously portable between implementations.
# The real config is used on purpose: a non-existent one makes setMainConfig()
# fall back to $HOME, and as root that would litter /root/.openfortigui -- which
# is what part f) asserts about.
if as_labuser "sudo -n $BIN --start-vpn --vpn-name __probe__ --main-config '$MAIN_CONF'" \
        >/tmp/wildcard.log 2>&1; then
    result wildcard_allowed yes
else
    if grep -qiE 'not allowed|may not run|authentication' /tmp/wildcard.log; then
        result wildcard_allowed no
    else
        # Permitted, but the program itself refused to run (no such config).
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
# 5) -E, the flag openfortiGUI used to depend on
# --------------------------------------------------------------------------

# env_probe <sudo binary> <command> -- returns the output including stderr
env_probe() {
    as_labuser "$MARKER_NAME=$MARKER_VALUE $1 -n -E $2" 2>&1
}

kept() { grep -q "$MARKER_NAME=$MARKER_VALUE" <<<"$1"; }

RS_OUT="$(env_probe sudo /usr/bin/env)"
if grep -q "is ignored" <<<"$RS_OUT"; then
    result dash_e_warning yes
else
    result dash_e_warning no
fi
if kept "$RS_OUT"; then result dash_e_keeps_env yes; else result dash_e_keeps_env no; fi
note "sudo-rs -E: $(grep -iE 'ignored|not supported' <<<"$RS_OUT" | head -1)"

# Same rule, classic sudo: without the SETENV: tag it does not just ignore -E, it
# refuses to run at all. openfortiGUI's own sudoers rule never carried the tag --
# so the -E it used to pass was never reliable on either implementation.
WS_OUT="$(env_probe /usr/bin/sudo.ws /usr/bin/env)"
if kept "$WS_OUT"; then result classic_keeps_env yes; else result classic_keeps_env no; fi
note "classic -E without SETENV: $(head -n 1 <<<"$WS_OUT")"

# With the tag it works -- proof that the difference is the tag, not the flag.
WS_SETENV_OUT="$(env_probe /usr/bin/sudo.ws /usr/local/bin/envprobe)"
if kept "$WS_SETENV_OUT"; then
    result classic_setenv_keeps_env yes
else
    result classic_setenv_keeps_env no
    note "classic -E with SETENV: $(head -n 1 <<<"$WS_SETENV_OUT")"
fi

# And sudo-rs ignores -E even when the rule explicitly allows it.
RS_SETENV_OUT="$(env_probe sudo /usr/local/bin/envprobe)"
if kept "$RS_SETENV_OUT"; then
    result rs_setenv_keeps_env yes
else
    result rs_setenv_keeps_env no
fi

# What the code used to do, against the rule the package actually ships: "sudo -E
# openfortigui --start-vpn ...". Neither implementation carries the environment
# across, and the classic one does not even run the command.
LEGACY_RS="$(as_labuser "sudo -n -E $BIN --start-vpn --vpn-name __probe__ --main-config '$MAIN_CONF'" 2>&1)"
if grep -q 'is ignored' <<<"$LEGACY_RS"; then
    result legacy_dash_e_rs ignored
else
    result legacy_dash_e_rs carried
fi

LEGACY_WS="$(as_labuser "/usr/bin/sudo.ws -n -E $BIN --start-vpn --vpn-name __probe__ --main-config '$MAIN_CONF'" 2>&1)"
if grep -qi 'not allowed to preserve the environment' <<<"$LEGACY_WS"; then
    result legacy_dash_e_classic refused
else
    result legacy_dash_e_classic accepted
fi
note "classic sudo -E with the shipped rule: $(head -n 1 <<<"$LEGACY_WS")"

# --------------------------------------------------------------------------
# 6) The real thing: a tunnel started through sudo-rs
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
# 7) Did anything land in root's home?
# --------------------------------------------------------------------------

if [[ -e /root/.openfortigui ]]; then
    result root_tree present
    note "$(find /root/.openfortigui | head -n 10)"
else
    result root_tree absent
fi

printf 'RESULT:done=yes\n'
