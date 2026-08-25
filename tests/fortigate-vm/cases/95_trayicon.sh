#!/usr/bin/env bash
# lab-requires: gui vm
#
# The tray icon: does it actually follow the tunnel, and does it stay quiet
# otherwise?
#
# Two defects met here. The first is the one behind #210/#212: setIcon() sat
# inside the per-profile loop of refreshVpnProfileList(), so every refresh
# published one NewIcon per profile. Measured on the released 0.9.13 with three
# profiles: 1930 NewIcon signals in ten idle seconds while the refresh loop ran,
# and four per refresh once the loop was fixed. GNOME Shell's appindicator
# extension leaks a Clutter actor per signal, which is the credible mechanism
# behind the session that ran out of memory in #210.
#
# The second was found while fixing the first and never shipped: the connected
# flag was computed inside that same loop, but the search filter "continue"s
# before the status check (mainwindow.cpp, refreshVpnProfileList). Typing a
# filter that hid the connected profile therefore made the icon claim
# "disconnected" while the tunnel was up. Both call sites ask
# vpnManager::isSomeClientConnected() now, which does not care what the list
# shows.
#
# Neither is observable without a StatusNotifierItem host, and the lab has none
# by design -- gui_app_start wipes the environment, so there is no session bus
# (that absence is what cases/90_gui.sh part c2 tests). sni_host.py supplies the
# missing half: a StatusNotifierWatcher that claims a host, and a log of every
# icon the application publishes.

set -uo pipefail
source "$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)/lib/case.sh"
case_setup

gui_require_tools

if pgrep -f 'bin/openfortigui$' >/dev/null 2>&1; then
    skip "tray icon" "another openfortiGUI instance is running -- please close it"
    case_finish
fi

# The profile that gets connected, plus one that does not. The second exists so
# that the filter in part c has something to match and the list never goes empty
# -- an empty list would hide a bug rather than expose it.
PROFILE="TrayIconVPN"
PROFILE_OTHER="TrayIconIdle"
client_write_profile "$PROFILE" "trusted_cert=$LAB_GW_DIGEST" >/dev/null
client_write_profile "$PROFILE_OTHER" "trusted_cert=$LAB_GW_DIGEST" >/dev/null

# This case needs a visible main window, and it is the first one that runs with a
# tray -- so start_minimized would actually take effect here rather than being
# overridden. Set it explicitly instead of trusting the previous case's cleanup.
sed -i 's/^start_minimized=.*/start_minimized=false/' "$LAB_MAIN_CONF"

# The search box is hidden until the toolbar button is used; the setting decides
# whether it comes up visible (mainwindow.cpp:243). Part c needs it.
if grep -q '^show_search=' "$LAB_MAIN_CONF"; then
    sed -i 's/^show_search=.*/show_search=true/' "$LAB_MAIN_CONF"
else
    sed -i 's/^\[main\]$/[main]\nshow_search=true/' "$LAB_MAIN_CONF"
fi

gui_start_display
gui_start_session_bus
gui_app_start

MAIN="$(gui_win '^OpenFortiGUI$')" || { fail "main window not found"; case_finish; }

# --------------------------------------------------------------------------
part "a) the stub is a tray, and the icon starts out disconnected"
# --------------------------------------------------------------------------
#
# Asserted rather than assumed: without a tray every check below would pass
# vacuously, because nothing would ever be published.

assert_contains "the application sees a system tray" "$(case_app_log)" \
    'system tray available: true'

if ! sni_wait_icons 1 20; then
    fail "the application never registered a tray icon" \
        "watcher log:
$(cat "$CASE_OUT_DIR/sni-host.log" 2>/dev/null)"
    case_finish
fi

ICON_OFF="$(sni_icon_sha 1)"
ok "initial icon published (sha $ICON_OFF)"

# An idle GUI must not keep republishing. This is the #210 storm in miniature:
# with the loop in place this alone produced hundreds of lines.
sleep 6
assert_eq "an idle GUI publishes no further icons" 1 "$(sni_icon_count)"

# --------------------------------------------------------------------------
part "b) the icon changes when the tunnel comes up"
# --------------------------------------------------------------------------

if ! python3 "$LAB_SRC_DIR/api_send.py" vpn-start "$PROFILE" >/dev/null; then
    fail "could not reach the GUI over $LAB_API_SOCK"
    case_finish
fi
info "ACTION_VPN_START sent for $PROFILE"

if ! wait_ppp_up "$TIMEOUT_CONNECT"; then
    fail "the tunnel did not come up" "last lines of the application log:
$(tail -n 20 "$(case_app_log)" 2>/dev/null)"
    case_finish
fi
ok "tunnel is up ($(ppp_iface))"

if sni_wait_icons 2 20; then
    ICON_ON="$(sni_icon_sha 2)"
    if [[ "$ICON_ON" != "$ICON_OFF" ]]; then
        ok "the tray icon changed on connect (sha $ICON_OFF -> $ICON_ON)"
    else
        fail "the tray icon was republished unchanged on connect" \
            "expected the connected icon, got the same bytes ($ICON_ON)"
    fi
else
    fail "the tray icon did not change when the tunnel came up" \
        "icon log:
$(cat "$LAB_GUI_SNI_LOG" 2>/dev/null)"
    ICON_ON=""
fi

# The state change arrives once per profile via onClientVPNStatusChanged and
# again through the refresh it triggers. Exactly one publish is the point of
# MainWindow::updateTrayIcon().
assert_eq "connecting publishes the icon once, not once per refresh" \
    2 "$(sni_icon_count)"

# --------------------------------------------------------------------------
part "c) filtering the list does not change the icon"
# --------------------------------------------------------------------------
#
# The regression guard for the loop-local flag: this filter hides the connected
# profile, so the old code published the disconnected icon over a live tunnel.
#
# Whether the filter really took effect is not taken on trust. A keystroke that
# misses the search box would leave the icon untouched too, and the check would
# pass while testing nothing. The exported tray menu says what the list actually
# holds, so it is asked before and after.

BEFORE="$(sni_icon_count)"

if ! sni_menu_labels | grep -qx "$PROFILE"; then
    skip "filtering leaves the icon alone" \
        "the tray menu does not list $PROFILE, so there is nothing to filter out"
else
    # 200/135 from the client-area origin is the search box: below the menu bar,
    # the tool bar and the tab bar, above the profile list.
    gui_activate "$MAIN"
    gui_click "$MAIN" 200 135 >/dev/null || fail "could not click the search box"
    gui_type "Idle"
    sleep 3

    LABELS="$(sni_menu_labels)"
    if grep -qx "$PROFILE_OTHER" <<<"$LABELS" && ! grep -qx "$PROFILE" <<<"$LABELS"; then
        ok "the filter took effect ($PROFILE is out of the list, $PROFILE_OTHER is in)"

        if [[ "$(sni_icon_count)" == "$BEFORE" ]]; then
            ok "filtering out the connected profile leaves the icon alone"
        elif [[ "$(sni_icon_sha)" == "$ICON_OFF" ]]; then
            fail "filtering the list made the icon claim 'disconnected'" \
                "the tunnel is still up -- this is the loop-local flag bug.
icon log:
$(cat "$LAB_GUI_SNI_LOG" 2>/dev/null)"
        else
            fail "filtering the list republished the icon" \
                "icon log:
$(cat "$LAB_GUI_SNI_LOG" 2>/dev/null)"
        fi
    else
        skip "filtering leaves the icon alone" \
            "the keystrokes did not reach the search box -- the menu still lists
$(tr '\n' ' ' <<<"$LABELS")"
    fi

    # Clear the filter again so part d sees the whole list.
    gui_activate "$MAIN"
    gui_key ctrl+a
    gui_key BackSpace
    sleep 2
    assert_eq "clearing the filter leaves the icon alone too" \
        "$BEFORE" "$(sni_icon_count)"
fi

# --------------------------------------------------------------------------
part "d) the icon changes back when the tunnel goes down"
# --------------------------------------------------------------------------

BEFORE="$(sni_icon_count)"

if ! python3 "$LAB_SRC_DIR/api_send.py" vpn-stop "$PROFILE" >/dev/null; then
    fail "could not send ACTION_VPN_STOP"
else
    WAITED=0
    while [[ -n "$(ppp_iface)" ]]; do
        (( WAITED >= 60 )) && break
        sleep 0.5
        WAITED=$(( WAITED + 1 ))
    done

    if [[ -z "$(ppp_iface)" ]]; then
        ok "the tunnel is down"
    else
        fail "the tunnel did not go down" "ppp interface still present: $(ppp_iface)"
    fi

    if sni_wait_icons $(( BEFORE + 1 )) 20; then
        assert_eq "the icon is the disconnected one again" \
            "$ICON_OFF" "$(sni_icon_sha)"
        assert_eq "disconnecting publishes the icon once" \
            $(( BEFORE + 1 )) "$(sni_icon_count)"
    else
        fail "the tray icon did not change when the tunnel went down" \
            "icon log:
$(cat "$LAB_GUI_SNI_LOG" 2>/dev/null)"
    fi
fi

info "icon log:"
while read -r line; do info "  $line"; done <"$LAB_GUI_SNI_LOG"

gui_app_stop
gui_stop_session_bus
rm -f "$LAB_PROFILE_DIR/$PROFILE.conf" "$LAB_PROFILE_DIR/$PROFILE_OTHER.conf"
sed -i 's/^show_search=.*/show_search=false/' "$LAB_MAIN_CONF"

case_finish
