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
part "b2) file permissions: config, profiles and API socket are owner-only"
# --------------------------------------------------------------------------

# main.conf is written by client_init_home with umask defaults on purpose --
# the mode below is the GUI's self-healing migration at work, the profile file
# tests saveVpnProfile(), the socket tests QLocalServer::UserAccessOption.
perm_check() {
    local expected="$1" path="$2" mode
    if ! mode="$(stat -c %a "$path" 2>/dev/null)"; then
        fail "missing for permission check: $path"
        return
    fi
    if [[ "$mode" == "$expected" ]]; then
        ok "mode $expected: $path"
    else
        fail "mode $mode instead of $expected: $path"
    fi
}

perm_check 700 "$LAB_CLIENT_HOME/.openfortigui"
perm_check 600 "$LAB_MAIN_CONF"
perm_check 600 "$LAB_PROFILE_DIR/$PROFILE_SAVE.conf"
perm_check 700 "$LAB_API_SOCK"

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

            # The stop a user performs: the GUI asks the child to end, it quits
            # with exit code 0, and stopVPN() drops the connection right away --
            # so the process handlers stay quiet. Nothing about a normal stop may
            # look like a failure (see part f for the other direction).
            if python3 "$LAB_SRC_DIR/api_send.py" vpn-stop "$PROFILE_PEM" >/dev/null; then
                sleep 6
                if gui_win '^Error$' 200 >/dev/null; then
                    gui_screenshot fail-stop-dialog >/dev/null
                    fail "stopping the VPN raises no error dialog" \
                        "vpnManager::onVPNProcessFinished() may only report an abnormal end."
                else
                    ok "stopping the VPN raises no error dialog"
                fi
            else
                skip "stopping the VPN raises no error dialog" "the GUI was not reachable"
            fi
        fi

        client_stop TERM 20 >/dev/null 2>&1 || client_kill >/dev/null 2>&1 || true
        rm -f "$LAB_PROFILE_DIR/$PROFILE_PEM.conf"
    fi
fi

# --------------------------------------------------------------------------
part "f) a failed sudo does not fail silently"
# --------------------------------------------------------------------------
#
# vpnManager's two process handlers only ever qDebug()ed, so when sudo refused,
# the entry disappeared from the list again and that was all the user got.
# vpnLogger explains the causes it has a pattern for -- sudo asking for a
# password, above all -- but everything else was silent: "user is not allowed to
# execute", a sudoers file with a syntax error, no sudo at all, a crash.
#
# What is faked here is only the sudo: what QProcess reports is the same whether
# the refusal comes from the real one or from a stand-in, and this way the case
# needs neither root nor a FortiGate.

gui_app_stop
sleep 1

# Parts d/e ran the child as root, which leaves the application log owned by root
# -- the GUI could not append to it any more and every qDebug() would be lost.
as_root rm -f "$CLIENT_APP_LOG"

FAKEBIN="$CASE_OUT_DIR/fakebin"
EMPTYBIN="$CASE_OUT_DIR/nobin"
mkdir -p "$FAKEBIN" "$EMPTYBIN"
PROFILE_SUDO="GuiSudoFailure"
client_write_profile "$PROFILE_SUDO" >/dev/null

# fake_sudo <message> <exit code> -- a sudo that says its piece and gives up
fake_sudo() {
    printf '#!/bin/sh\necho "%s"\nexit %s\n' "$1" "$2" >"$FAKEBIN/sudo"
    chmod +x "$FAKEBIN/sudo"
}

# start_with_path <PATH> -- the GUI with a PATH of our choosing
start_with_path() {
    LAB_GUI_PATH="$1" gui_app_start
    LAB_GUI_PATH=""
}

# error_dialog <check name> <timeout> -- wait for the dialog and dismiss it
error_dialog() {
    local name="$1" timeout="$2" id
    if id="$(gui_wait_window '^Error$' "$timeout" 200)"; then
        ok "$name"
        gui_activate "$id"
        gui_key Return
        sleep 1
        return 0
    fi
    gui_screenshot "fail-${name//[^a-zA-Z0-9]/-}" >/dev/null
    fail "$name" "no dialog appeared within ${timeout}s.
windows on screen:
$(gui_list_windows)
last lines of the application log:
$(tail -n 20 "$(case_app_log)" 2>/dev/null)"
    return 1
}

# f1) the refusal nobody had a pattern for
fake_sudo "sudo: labuser is not allowed to execute '/usr/bin/openfortigui --start-vpn' as root on lab." 1
start_with_path "$FAKEBIN:/usr/bin:/bin"
python3 "$LAB_SRC_DIR/api_send.py" vpn-start "$PROFILE_SUDO" >/dev/null \
    || fail "could not reach the GUI over $LAB_API_SOCK"
error_dialog "sudo refuses the command: an error dialog appears" 20
assert_contains "the exit code reaches the log" "$(case_app_log)" 'finished:: 1'

# f2) no sudo at all -- QProcess::FailedToStart, not a single byte of output
gui_app_stop
sleep 1
start_with_path "$EMPTYBIN"
python3 "$LAB_SRC_DIR/api_send.py" vpn-start "$PROFILE_SUDO" >/dev/null \
    || fail "could not reach the GUI over $LAB_API_SOCK"
error_dialog "sudo is not in PATH: an error dialog appears" 20
assert_contains "the start failure reaches the log" "$(case_app_log)" 'error occurred::'

# f3) one failure, one dialog
#
# For this message vpnLogger has a pattern and an explanation of its own, so the
# generic report has to stay away -- it waits two seconds for exactly that
# reason. Whichever of the two speaks, the user must not have to close a second
# dialog saying the same thing in worse words.
gui_app_stop
sleep 1
fake_sudo "sudo: a terminal is required to read the password; either use the -S option to read from standard input or configure an askpass helper" 1
start_with_path "$FAKEBIN:/usr/bin:/bin"
python3 "$LAB_SRC_DIR/api_send.py" vpn-start "$PROFILE_SUDO" >/dev/null \
    || fail "could not reach the GUI over $LAB_API_SOCK"
if error_dialog "sudo asks for a password: the explanation appears" 20; then
    SECOND=""
    for _ in 1 2 3 4 5 6; do
        sleep 1
        if SECOND="$(gui_win '^Error$' 200)"; then break; fi
        SECOND=""
    done
    if [[ -z "$SECOND" ]]; then
        ok "only one dialog for one failure"
    else
        gui_screenshot fail-second-dialog >/dev/null
        fail "a second error dialog followed" \
            "vpnManager::reportProcessFailure() must drop the generic message once
vpnLogger has reported a specific one for that VPN."
    fi
fi

# f4) a name that does not exist
#
# startVPN() dereferenced the null pointer that getVpnProfileByName() returns for
# an unknown name, so a single api message took the whole GUI down with it -- and
# anything on that socket can send one, a KRunner entry pointing at a renamed
# profile included.
python3 "$LAB_SRC_DIR/api_send.py" vpn-start "NoSuchProfileHere" >/dev/null \
    || fail "could not reach the GUI over $LAB_API_SOCK"
error_dialog "an unknown profile name is reported" 15
if gui_app_running; then
    ok "the GUI survives an unknown profile name"
else
    fail "the GUI died on an unknown profile name" \
        "vpnManager::startVPN() has to check the result of getVpnProfileByName()."
fi

gui_app_stop

# --------------------------------------------------------------------------
part "g) a chatty child does not corrupt the log or the GUI"
# --------------------------------------------------------------------------
#
# vpnLogger ran QProcess::read() in its own thread on a process owned by the main
# thread. While it walked the ring buffer, the main thread's socket notifier
# appended to it -- corruption and a crash in QRingBuffer::read() under load (PR
# #207). Measured before the fix: single reads of 25 MB out of a process living in
# "Qt mainThread".
#
# The read now happens in the process' own thread and only the bytes are handed
# over, coalesced by a 150 ms timer instead of by a 200 ms sleep. What is
# assertable from outside is that nothing is lost: every line the child wrote has
# to end up in the log, the last chunk included -- that one only arrives because
# procFinished() flushes what is still pending.

# The payload is generated first so its exact size is known. Counting markers
# would not do: the logger writes one timestamp per chunk, so a line that falls on
# a chunk boundary appears split in the log without a single byte being lost.
PAYLOAD="$CASE_OUT_DIR/chatty-payload.txt"
seq 1 200000 | sed 's/^/DEBUG:  tunnel chunk /' >"$PAYLOAD"
CHATTY_BYTES="$(stat -c%s "$PAYLOAD")"

PROFILE_CHATTY="GuiChattyChild"
client_write_profile "$PROFILE_CHATTY" >/dev/null
CHATTY_LOG="$LAB_CLIENT_HOME/.openfortigui/logs/vpn/$PROFILE_CHATTY.log"
rm -f "$CHATTY_LOG"

printf '#!/bin/sh\ncat %s\nexit 0\n' "$PAYLOAD" >"$FAKEBIN/sudo"
chmod +x "$FAKEBIN/sudo"

start_with_path "$FAKEBIN:/usr/bin:/bin"
python3 "$LAB_SRC_DIR/api_send.py" vpn-start "$PROFILE_CHATTY" >/dev/null \
    || fail "could not reach the GUI over $LAB_API_SOCK"

# Wait until the log stops growing.
LAST=0
STABLE=0
for _ in $(seq 1 60); do
    sleep 1
    SIZE="$(stat -c%s "$CHATTY_LOG" 2>/dev/null || echo 0)"
    if [[ "$SIZE" == "$LAST" ]]; then
        STABLE=$(( STABLE + 1 ))
        (( STABLE >= 3 )) && break
    else
        STABLE=0
    fi
    LAST="$SIZE"
done
info "$(( LAST / 1024 ))k of $(( CHATTY_BYTES / 1024 ))k payload bytes logged"

if gui_app_running; then
    ok "the GUI survives a chatty child ($(( CHATTY_BYTES / 1024 / 1024 )) MB in one go)"
else
    fail "the GUI died while logging" \
        "This is the crash from PR #207: a QProcess may only be read in its own thread.
$(tail -n 5 "$CASE_OUT_DIR/gui-stdout.log" 2>/dev/null)"
fi

# Timestamps only add bytes, so anything below the payload size means data was
# dropped -- the tail of it arrives only because procFinished() flushes what is
# still pending.
if (( LAST >= CHATTY_BYTES )); then
    ok "no output was lost"
else
    fail "output was lost" \
        "the payload is $CHATTY_BYTES bytes, $CHATTY_LOG holds $LAST."
fi

# The child ended with exit code 0, so nothing about it may look like a failure.
sleep 3
if gui_win '^Error$' 200 >/dev/null; then
    gui_screenshot fail-chatty-dialog >/dev/null
    fail "a chatty child that exits cleanly raises no dialog" "see part f"
else
    ok "a chatty child that exits cleanly raises no dialog"
fi

gui_app_stop
rm -f "$CHATTY_LOG"
rm -f "$LAB_PROFILE_DIR/$PROFILE_CHATTY.conf"
rm -f "$LAB_PROFILE_DIR/$PROFILE_SUDO.conf"

# Leave no test profiles behind for the following cases.
rm -f "$LAB_PROFILE_DIR/$PROFILE_SAVE.conf" "$LAB_PROFILE_DIR/$PROFILE_CANCEL.conf"
rm -f "$LAB_CLIENT_HOME/.openfortigui/vpngroups/$GROUP_NAME.conf"

case_finish
