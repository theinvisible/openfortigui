# shellcheck shell=bash
#
# Helpers for driving the real openfortiGUI on a virtual X server.
#
# Everything here exists because a few of the reported bugs are only visible on
# a screen: whether a window fits on 1280x800, whether Enter saves, whether the
# passphrase dialog is labelled as such. Xvfb plus a window manager gives us
# that without a desk.
#
# Four things were learned the hard way and are encoded below -- change them and
# the tests stop working:
#
#   * Qt6 prefers Wayland whenever WAYLAND_DISPLAY is set, and then ignores
#     DISPLAY entirely. Without unsetting it the windows open on the developer's
#     real desktop and nothing is found on Xvfb.
#   * "xdotool getwindowgeometry" reports the wrong absolute position under a
#     reparenting window manager (off by the title bar height). Positions come
#     from "xwininfo -id" instead; sizes agree between the two.
#   * A window manager is not optional: without one nothing has input focus and
#     WM_NORMAL_HINTS is not enforced, so both the keyboard and the minimum-size
#     assertions would be meaningless.
#   * The name openfortiGUI belongs to a 1x1 helper window (the tray icon). Real
#     windows are picked by name *and* a minimum width.

: "${LAB_GUI_WIDTH:=1280}"
: "${LAB_GUI_HEIGHT:=800}"
: "${LAB_GUI_WM:=openbox}"

LAB_GUI_DISPLAY=""
LAB_GUI_XVFB_PID=""
LAB_GUI_WM_PID=""
LAB_GUI_APP_PID=""

# --------------------------------------------------------------------------
# Requirements
# --------------------------------------------------------------------------

gui_require_tools() {
    local missing=() tool
    for tool in Xvfb xdotool xwininfo xprop "$LAB_GUI_WM"; do
        have "$tool" || missing+=("$tool")
    done
    (( ${#missing[@]} == 0 )) && return 0
    die "missing tools for the GUI tests: ${missing[*]}
    sudo apt install xvfb xdotool openbox x11-utils x11-apps"
}

# --------------------------------------------------------------------------
# Display
# --------------------------------------------------------------------------

# gui_start_display [width] [height]
gui_start_display() {
    local w="${1:-$LAB_GUI_WIDTH}" h="${2:-$LAB_GUI_HEIGHT}" n
    gui_require_tools

    for n in $(seq 90 119); do
        [[ -e "/tmp/.X${n}-lock" ]] && continue
        LAB_GUI_DISPLAY=":$n"
        break
    done
    [[ -n "$LAB_GUI_DISPLAY" ]] || die "no free X display between :90 and :119"

    Xvfb "$LAB_GUI_DISPLAY" -screen 0 "${w}x${h}x24" -nolisten tcp -ac \
        >"$CASE_OUT_DIR/xvfb.log" 2>&1 &
    LAB_GUI_XVFB_PID=$!

    local waited=0
    until gui_env xdpyinfo >/dev/null 2>&1; do
        (( waited >= 20 )) && die "Xvfb did not come up on $LAB_GUI_DISPLAY"
        sleep 0.5
        waited=$(( waited + 1 ))
    done

    gui_env "$LAB_GUI_WM" --sm-disable >"$CASE_OUT_DIR/wm.log" 2>&1 &
    LAB_GUI_WM_PID=$!
    sleep 1

    info "virtual screen ${w}x${h} on $LAB_GUI_DISPLAY, window manager $LAB_GUI_WM"
}

gui_stop_display() {
    gui_app_stop
    [[ -n "$LAB_GUI_WM_PID"   ]] && kill "$LAB_GUI_WM_PID"   2>/dev/null
    [[ -n "$LAB_GUI_XVFB_PID" ]] && kill "$LAB_GUI_XVFB_PID" 2>/dev/null
    sleep 0.5
    [[ -n "$LAB_GUI_DISPLAY" ]] && rm -f "/tmp/.X${LAB_GUI_DISPLAY#:}-lock"
    LAB_GUI_WM_PID=""; LAB_GUI_XVFB_PID=""; LAB_GUI_DISPLAY=""
}

# gui_env <command...> -- run an X client against the test display.
# WAYLAND_DISPLAY has to go, see the header.
gui_env() {
    env -u WAYLAND_DISPLAY "DISPLAY=$LAB_GUI_DISPLAY" "$@"
}

# Short forms used by the test cases
xd()  { gui_env xdotool "$@"; }
xwi() { gui_env xwininfo "$@"; }

# --------------------------------------------------------------------------
# The application
# --------------------------------------------------------------------------

# gui_app_start [extra args...]
#
# Starts the GUI with the lab configuration. The environment is stripped down on
# purpose: no session bus (the GUI would export its menu bar to the desktop's
# global menu and the window would have none), no Wayland, no desktop theme.
gui_app_start() {
    local bin; bin="$(client_require_bin)"

    [[ -f "$LAB_MAIN_CONF" ]] || die "$LAB_MAIN_CONF is missing -- run client_init_home first"

    env -i PATH=/usr/bin:/bin \
        HOME="$LAB_CLIENT_HOME" \
        "DISPLAY=$LAB_GUI_DISPLAY" \
        QT_QPA_PLATFORM=xcb \
        "$bin" --main-config "$LAB_MAIN_CONF" --api-socket "$LAB_API_SOCK" "$@" \
        >"$CASE_OUT_DIR/gui-stdout.log" 2>&1 &
    LAB_GUI_APP_PID=$!

    if ! gui_wait_window '^OpenFortiGUI$' 20 >/dev/null; then
        gui_app_stop
        die "the main window did not appear. Last lines of the application log:
$(tail -n 10 "$(case_app_log)" 2>/dev/null)"
    fi
    info "GUI running (pid $LAB_GUI_APP_PID), api socket $LAB_API_SOCK"
}

gui_app_stop() {
    [[ -z "$LAB_GUI_APP_PID" ]] && return 0
    kill "$LAB_GUI_APP_PID" 2>/dev/null
    local waited=0
    while pid_alive "$LAB_GUI_APP_PID"; do
        (( waited >= 10 )) && { kill -9 "$LAB_GUI_APP_PID" 2>/dev/null; break; }
        sleep 0.5
        waited=$(( waited + 1 ))
    done
    LAB_GUI_APP_PID=""
}

gui_app_running() { pid_alive "$LAB_GUI_APP_PID"; }

# --------------------------------------------------------------------------
# Windows
# --------------------------------------------------------------------------

# gui_win <name regex> [min width]
# Prints the id of the first visible window matching the name that is at least
# <min width> wide -- that filter keeps the 1x1 tray helper windows out.
gui_win() {
    local re="$1" minw="${2:-300}" id w
    for id in $(xd search --onlyvisible --name "$re" 2>/dev/null); do
        w="$(xwi -id "$id" 2>/dev/null | sed -n 's/^ *Width: *//p')"
        [[ -n "$w" ]] && (( w >= minw )) && { printf '%s' "$id"; return 0; }
    done
    return 1
}

# gui_wait_window <name regex> <timeout seconds> [min width]
gui_wait_window() {
    local re="$1" timeout="$2" minw="${3:-300}" waited=0 id
    while true; do
        if id="$(gui_win "$re" "$minw")"; then printf '%s' "$id"; return 0; fi
        (( waited >= timeout )) && return 1
        sleep 1
        waited=$(( waited + 1 ))
    done
}

# gui_wait_no_window <name regex> <timeout seconds>
gui_wait_no_window() {
    local re="$1" timeout="$2" waited=0
    while gui_win "$re" >/dev/null; do
        (( waited >= timeout )) && return 1
        sleep 1
        waited=$(( waited + 1 ))
    done
    return 0
}

# gui_win_size <id> -> "WIDTHxHEIGHT"
gui_win_size() {
    xwi -id "$1" 2>/dev/null \
        | sed -n 's/^ *Width: *\(.*\)/\1/p;s/^ *Height: *\(.*\)/\1/p' | paste -sd'x'
}

gui_win_width()  { gui_win_size "$1" | cut -dx -f1; }
gui_win_height() { gui_win_size "$1" | cut -dx -f2; }

# gui_win_pos <id> -> "X Y" (absolute, correct under a reparenting WM)
gui_win_pos() {
    xwi -id "$1" 2>/dev/null | sed -n \
        's/.*Absolute upper-left X: *\(.*\)/\1/p;s/.*Absolute upper-left Y: *\(.*\)/\1/p' \
        | paste -sd' '
}

# gui_min_size <id> -> "WIDTHxHEIGHT" from WM_NORMAL_HINTS, empty if unset.
#
# This is the direct regression guard for issue #205: the nine copies of
# setMinimumSize(form size) in mainwindow.cpp ended up here, which is why the
# profile editor could not be made to fit on a 800 px high screen.
gui_min_size() {
    gui_env xprop -id "$1" WM_NORMAL_HINTS 2>/dev/null \
        | sed -n 's/.*program specified minimum size: *\([0-9]*\) by \([0-9]*\).*/\1x\2/p'
}

gui_resize() { xd windowsize "$1" "$2" "$3"; sleep 1.2; }

gui_activate() { xd windowactivate --sync "$1" 2>/dev/null; sleep 0.4; }

# gui_key <keys...>  /  gui_type <text>
# Deliberately without --window: Qt ignores some synthetic events, so we drive
# the focused window instead and make sure the right one is active.
gui_key()  { xd key --clearmodifiers "$@"; sleep 0.4; }
gui_type() { xd type --delay 60 "$1"; sleep 0.4; }

# gui_click <id> <dx> <dy> -- click relative to the client area's upper left
gui_click() {
    local id="$1" dx="$2" dy="$3" x y
    read -r x y <<<"$(gui_win_pos "$id")"
    [[ -n "$x" && -n "$y" ]] || return 1
    xd mousemove $(( x + dx )) $(( y + dy )) click 1
    sleep 1
}

# gui_screenshot <name> -- raw X dump into the case output, for failures
gui_screenshot() {
    have xwd || return 0
    gui_env xwd -root -silent >"$CASE_OUT_DIR/$1.xwd" 2>/dev/null || true
    printf '%s/%s.xwd' "$CASE_OUT_DIR" "$1"
}

# gui_list_windows -- every named window, for diagnostics
gui_list_windows() {
    local id n
    for id in $(xd search --onlyvisible --name '.' 2>/dev/null); do
        n="$(xd getwindowname "$id" 2>/dev/null)"
        [[ -n "$n" ]] && printf "  '%s' %s\n" "$n" "$(gui_win_size "$id")"
    done
}

# --------------------------------------------------------------------------
# Ways into the interesting windows
#
# The main window offers no keyboard shortcut for either of these, so both go
# through the pointer. The coordinates are the menu bar and the tool bar of the
# main window, measured relative to its client area.
# --------------------------------------------------------------------------

# gui_open_menu_item <main window id> <steps down>
# Opens the first menu ("File") and walks down <steps> entries.
gui_open_menu_item() {
    local main="$1" steps="${2:-1}" i
    gui_activate "$main"
    gui_click "$main" 18 8 || return 1
    for (( i = 0; i < steps; i++ )); do gui_key Down; done
    gui_key Return
}

# gui_open_add_menu <main window id> <steps down>
# The "Add" tool button is an InstantPopup with two entries, VPN and VPN-Group.
# Its position is searched rather than hard-coded so a changed tool bar does not
# silently break the test.
gui_open_add_menu() {
    local main="$1" steps="${2:-1}" expect="$3" dx i id
    for dx in 190 160 220 130 250 100 280; do
        gui_activate "$main"
        gui_click "$main" "$dx" 45 || continue
        for (( i = 0; i < steps; i++ )); do gui_key Down; done
        gui_key Return
        sleep 1.5
        if id="$(gui_win "$expect")"; then printf '%s' "$id"; return 0; fi
        # Wrong button: close whatever it opened and try the next position.
        for id in $(xd search --onlyvisible --name ' - ' 2>/dev/null); do
            gui_activate "$id"; gui_key Escape
        done
        gui_key Escape
    done
    return 1
}
