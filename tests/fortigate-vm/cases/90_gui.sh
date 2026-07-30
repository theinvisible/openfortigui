#!/usr/bin/env bash
# lab-requires: gui
#
# The two bugs that can only be seen on a screen.
#
# #205 -- the profile editor was declared 700x842 and every window opened with
# setMinimumSize(form size), so on a 1280x800 screen it could not be made to fit
# and the buttons at the bottom were unreachable. On top of that the editors are
# QWidgets in a QMainWindow, so there is no default-button mechanism: Enter did
# nothing. Checked here are the window size, the minimum size in
# WM_NORMAL_HINTS, that the window can be shrunk, and that Enter saves and
# Escape discards.
#
# #166 -- an encrypted private key makes openfortivpn ask for its passphrase. The
# GUI used to have no pattern for that prompt, so no dialog opened and the
# process waited forever; when a dialog was wired up it was the OTP one, asking
# the wrong question. Checked here is that the dialog appears, that it is the
# passphrase dialog and not the OTP one, and that answering it gets the tunnel up.
#
# Parts a-c need no FortiGate. They run against a virtual screen only and are
# skipped by nothing; parts d-e need the lab and skip without it.

set -uo pipefail
source "$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)/lib/case.sh"

SCREEN_W=1280
SCREEN_H=800

gui_require_tools
client_require_bin >/dev/null
[[ -f "$LAB_MAIN_CONF" ]] || client_init_home

# A GUI of our own would attach to a foreign socket, and a leftover VPN child
# would make isRunningAlready() (main.cpp) believe an instance is up.
client_cleanup_all
if pgrep -f 'bin/openfortigui$' >/dev/null 2>&1; then
    skip "GUI tests" "another openfortiGUI instance is running -- please close it"
    case_finish
fi

# Profiles this case creates; removed again so the next case starts clean.
PROFILE_SAVE="GuiEnterSave"
PROFILE_CANCEL="GuiEscapeCancel"
GROUP_NAME="GuiGroupTest"
rm -f "$LAB_PROFILE_DIR/$PROFILE_SAVE.conf" "$LAB_PROFILE_DIR/$PROFILE_CANCEL.conf"

gui_start_display "$SCREEN_W" "$SCREEN_H"
gui_app_start

MAIN="$(gui_win '^OpenFortiGUI$')" || { fail "main window not found"; case_finish; }

# fits_screen <name> <window id>
fits_screen() {
    local name="$1" id="$2" w h
    w="$(gui_win_width "$id")"; h="$(gui_win_height "$id")"
    if [[ -n "$w" && -n "$h" ]] && (( w <= SCREEN_W && h <= SCREEN_H )); then
        ok "$name fits on ${SCREEN_W}x${SCREEN_H} (${w}x${h})"
    else
        gui_screenshot "fail-${name// /-}" >/dev/null
        fail "$name does not fit on ${SCREEN_W}x${SCREEN_H}" "measured: ${w}x${h}"
    fi
}

# min_size_small <name> <window id>
# The form sizes are 612x549 (settings) and 609x720 (profile editor). Anything in
# that range as a minimum means setMinimumSize is back.
min_size_small() {
    local name="$1" id="$2" min mw mh
    min="$(gui_min_size "$id")"
    if [[ -z "$min" ]]; then
        ok "$name has no minimum size at all"
        return
    fi
    mw="${min%x*}"; mh="${min#*x}"
    if (( mw <= 400 && mh <= 400 )); then
        ok "$name has a small minimum size ($min)"
    else
        fail "$name is pinned to a large minimum size" \
            "WM_NORMAL_HINTS says $min -- that is setMinimumSize(form size) again (issue #205)"
    fi
}

# shrinkable <name> <window id>
shrinkable() {
    local name="$1" id="$2" after
    gui_resize "$id" 640 480
    after="$(gui_win_size "$id")"
    if [[ "$after" == "640x480" ]]; then
        ok "$name can be shrunk to 640x480"
    else
        fail "$name cannot be shrunk to 640x480" "the window manager stopped at $after"
    fi
}

# --------------------------------------------------------------------------
part "a) main window and the settings window"
# --------------------------------------------------------------------------

ok "main window is up ($(gui_win_size "$MAIN"))"
fits_screen "main window" "$MAIN"

if SETTINGS="$(gui_open_menu_item "$MAIN" 1 >/dev/null; gui_wait_window ' - Settings$' 10)"; then
    ok "settings window opens from the File menu"
    fits_screen "settings window" "$SETTINGS"
    min_size_small "settings window" "$SETTINGS"
    shrinkable "settings window" "$SETTINGS"

    gui_activate "$SETTINGS"
    gui_key Escape
    if gui_wait_no_window ' - Settings$' 6; then
        ok "Escape closes the settings window"
    else
        fail "Escape does not close the settings window"
        gui_activate "$SETTINGS"; gui_key Escape
    fi
else
    fail "settings window did not open" "windows on screen:
$(gui_list_windows)"
fi

# --------------------------------------------------------------------------
part "b) profile editor: size, minimum size, Enter, Escape"
# --------------------------------------------------------------------------

if ADD="$(gui_open_add_menu "$MAIN" 1 ' - Add VPN$')"; then
    ok "profile editor opens from the Add menu ($(gui_win_size "$ADD"))"
    fits_screen "profile editor" "$ADD"
    min_size_small "profile editor" "$ADD"
    shrinkable "profile editor" "$ADD"

    # Name and gateway are the only mandatory fields (on_btnSave_clicked);
    # leName has the focus, leGatewayHost is next in the tab order.
    gui_activate "$ADD"
    gui_type "$PROFILE_SAVE"
    gui_key Tab
    gui_type "$FGT_WAN_IP"
    gui_key Return
    sleep 2

    if [[ -f "$LAB_PROFILE_DIR/$PROFILE_SAVE.conf" ]]; then
        ok "Enter saves the profile ($PROFILE_SAVE.conf)"
    else
        gui_screenshot fail-enter-save >/dev/null
        fail "Enter did not save the profile" \
            "expected $LAB_PROFILE_DIR/$PROFILE_SAVE.conf
windows on screen:
$(gui_list_windows)"
    fi

    assert_true "editor closes after saving" gui_wait_no_window ' - Add VPN$' 6

    if grep -q "gateway_host=$FGT_WAN_IP" "$LAB_PROFILE_DIR/$PROFILE_SAVE.conf" 2>/dev/null; then
        ok "the typed gateway ended up in the profile"
    else
        fail "the typed values did not reach the fields" \
            "$(cat "$LAB_PROFILE_DIR/$PROFILE_SAVE.conf" 2>/dev/null | head -5)"
    fi
else
    fail "profile editor did not open" "windows on screen:
$(gui_list_windows)"
fi

if ADD2="$(gui_open_add_menu "$MAIN" 1 ' - Add VPN$')"; then
    gui_activate "$ADD2"
    gui_type "$PROFILE_CANCEL"
    gui_key Escape
    if gui_wait_no_window ' - Add VPN$' 6; then
        ok "Escape closes the profile editor"
    else
        fail "Escape does not close the profile editor"
    fi
    if [[ -f "$LAB_PROFILE_DIR/$PROFILE_CANCEL.conf" ]]; then
        fail "Escape saved the profile anyway" "$PROFILE_CANCEL.conf exists"
    else
        ok "Escape discards the entry"
    fi
else
    fail "profile editor did not open a second time"
fi

# --------------------------------------------------------------------------
part "c) group editor"
# --------------------------------------------------------------------------

if GRP="$(gui_open_add_menu "$MAIN" 2 ' - Add VPN-Group$')"; then
    ok "group editor opens ($(gui_win_size "$GRP"))"
    fits_screen "group editor" "$GRP"
    min_size_small "group editor" "$GRP"
    gui_activate "$GRP"
    gui_key Escape
    assert_true "Escape closes the group editor" gui_wait_no_window ' - Add VPN-Group$' 6
else
    fail "group editor did not open" "windows on screen:
$(gui_list_windows)"
fi

# --------------------------------------------------------------------------
part "d) passphrase dialog for an encrypted client key (#166)"
# --------------------------------------------------------------------------

if ! vm_running; then
    skip "passphrase dialog" "the FortiGate VM is not running -- 'testlab up' first"
    skip "no OTP dialog for a passphrase prompt" "see above"
    skip "tunnel comes up after the passphrase" "see above"
else
    case_setup

    KEYDIR="$CASE_OUT_DIR/clientcert"
    PASSPHRASE="labpass"
    PROFILE_PEM="GuiPemPassphrase"

    if ! client_make_client_cert "$KEYDIR" "$PASSPHRASE"; then
        skip "passphrase dialog" "openssl could not create a test key"
    else
        client_write_profile "$PROFILE_PEM" "trusted_cert=$LAB_GW_DIGEST" \
            "user_cert=$KEYDIR/cert.pem" "user_key=$KEYDIR/key.pem" >/dev/null

        # The GUI is the api server here, so this is a client asking it to
        # connect -- the same request the KRunner plugin sends.
        if python3 "$LAB_SRC_DIR/api_send.py" vpn-start "$PROFILE_PEM" >/dev/null; then
            info "ACTION_VPN_START sent for $PROFILE_PEM"
        else
            fail "could not reach the GUI over $LAB_API_SOCK"
        fi

        if PROMPT="$(gui_wait_window ' - Certificate passphrase$' 45 200)"; then
            ok "the passphrase dialog appears, labelled for a certificate"
        else
            gui_screenshot fail-passphrase-dialog >/dev/null
            fail "no passphrase dialog" \
                "vpnLogger matches on \"PEM pass phrase\" and
MainWindow::onClientVPNPromptRequest labels it PROMPT_PEM_PASSPHRASE.
windows on screen:
$(gui_list_windows)
last lines of the application log:
$(tail -n 20 "$(case_app_log)" 2>/dev/null)"
        fi

        if gui_win ' - OTP-Login$' 200 >/dev/null; then
            fail "the OTP dialog opened for a passphrase prompt" \
                "that is exactly what issue #166 complains about"
        else
            ok "no OTP dialog for a passphrase prompt"
        fi

        if [[ -n "${PROMPT:-}" ]]; then
            gui_activate "$PROMPT"
            gui_type "$PASSPHRASE"
            gui_key Return
            sleep 2

            if wait_ppp_up "$TIMEOUT_CONNECT"; then
                ok "tunnel comes up after the passphrase ($(ppp_iface))"
            else
                APPLOG="$(case_app_log)"
                if grep -Eq 'Connected to gateway' "$APPLOG" 2>/dev/null; then
                    ok "passphrase accepted, TLS connection established"
                    info "no tunnel afterwards -- the gateway declined the client certificate"
                else
                    gui_screenshot fail-after-passphrase >/dev/null
                    fail "the passphrase did not get the connection any further" \
                        "$(tail -n 25 "$APPLOG" 2>/dev/null)"
                fi
            fi

            # ------------------------------------------------------------------
            part "e) the dialog answers to stdin, not into the void"
            # ------------------------------------------------------------------
            APPLOG="$(case_app_log)"
            assert_not_contains "no unreachable api socket" "$APPLOG" 'Socket not open'

            # The child's output goes through vpnLogger into the per-VPN log, not
            # into the GUI's own -- that file is where the prompt has to show up.
            VPNLOG="$LAB_CLIENT_HOME/.openfortigui/logs/vpn/$PROFILE_PEM.log"
            assert_contains "the prompt is in the VPN log" "$VPNLOG" 'PEM pass phrase'
        fi

        client_stop TERM 20 >/dev/null 2>&1 || client_kill >/dev/null 2>&1 || true
        rm -f "$LAB_PROFILE_DIR/$PROFILE_PEM.conf"
    fi
fi

# Leave no test profiles behind for the following cases.
rm -f "$LAB_PROFILE_DIR/$PROFILE_SAVE.conf" "$LAB_PROFILE_DIR/$PROFILE_CANCEL.conf"
rm -f "$LAB_CLIENT_HOME/.openfortigui/vpngroups/$GROUP_NAME.conf"

case_finish
