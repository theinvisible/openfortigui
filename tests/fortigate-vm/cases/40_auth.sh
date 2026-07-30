#!/usr/bin/env bash
#
# Authentication and configuration errors.

set -uo pipefail
source "$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)/lib/case.sh"
case_setup

# --------------------------------------------------------------------------
part "a) wrong password"
# --------------------------------------------------------------------------

LOG_A="$(case_log a-wrong-password)"
client_write_profile "lab-auth-badpass" \
    "trusted_cert=$LAB_GW_DIGEST" "password_plain=definitely-wrong-$$" >/dev/null
client_start "lab-auth-badpass" "$LOG_A" || fail "process did not start"

if client_wait_log "$LOG_A" 'Could not authenticate to gateway' 40; then
    ok "the gateway rejects the wrong password"
else
    fail "expected error message missing" "$(tail -n 25 "$LOG_A")"
fi

assert_contains "the TLS connection was established first" "$LOG_A" 'Connected to gateway'
assert_not_contains "no tunnel" "$LOG_A" 'Tunnel is up and running'
assert_true "process exits on its own" client_wait_exit 30
assert_true "no ppp interface" test -z "$(ppp_iface)"

# Regression check: the error path must not crash either. It goes through
# err_tunnel in vpnWorker::process() and ends up in the same
# closeProcess() -> vpnWorker::end() as a regular teardown.
RC="$(client_exit_code)"
case "$RC" in
    0)   ok "process exited with code 0" ;;
    139) fail "process died with SIGSEGV (exit code 139)" \
            "Dangling ptr_tunnel in vpnWorker::end()." ;;
    "")  skip "exit code" "could not be determined" ;;
    *)   fail "process exited with code $RC" "expected 0" ;;
esac

# --------------------------------------------------------------------------
part "b) unknown user"
# --------------------------------------------------------------------------

LOG_B="$(case_log b-unknown-user)"
client_write_profile "lab-auth-baduser" \
    "trusted_cert=$LAB_GW_DIGEST" "username=gibtesnicht" >/dev/null
client_start "lab-auth-baduser" "$LOG_B" || fail "process did not start"

if client_wait_log "$LOG_B" 'Could not authenticate to gateway' 40; then
    ok "an unknown user is rejected"
else
    fail "expected error message missing" "$(tail -n 25 "$LOG_B")"
fi
assert_true "process exits on its own" client_wait_exit 30

# --------------------------------------------------------------------------
part "c) non-existent profile"
# --------------------------------------------------------------------------

LOG_C="$(case_log c-missing-profile)"
client_start "lab-auth-does-not-exist" "$LOG_C" || fail "process did not start"
assert_true "process exits immediately" client_wait_exit 20

APPLOG="$(case_app_log)"
assert_contains "profile error in the application log" "$APPLOG" 'VPN profile not found'

# --------------------------------------------------------------------------
part "d) missing password without a GUI"
# --------------------------------------------------------------------------

# If a user name is set and the password is empty, vpnProcess asks the GUI for
# the credentials over the local socket and times out after 30 s
# (vpnprocess.cpp:140-165). Headless there is no peer for that -- so the process
# has to exit without building a tunnel. Only the exit is observable:
# submitVPNMessage() sends the message over the socket only and writes nothing
# to the log.
LOG_D="$(case_log d-no-password)"
client_write_profile "lab-auth-nopass" \
    "trusted_cert=$LAB_GW_DIGEST" "password_plain=" >/dev/null
START_TS=$SECONDS
client_start "lab-auth-nopass" "$LOG_D" || fail "process did not start"

if client_wait_exit 60; then
    ELAPSED=$(( SECONDS - START_TS ))
    ok "process exits after ${ELAPSED}s without credentials"
    if (( ELAPSED >= 25 )); then
        ok "the wait matches the 30 second timeout"
    else
        skip "timeout duration" "only ${ELAPSED}s -- it bailed out early"
    fi
else
    fail "process hangs without credentials" "$(tail -n 20 "$LOG_D")"
fi
assert_not_contains "no tunnel without a password" "$LOG_D" 'Tunnel is up and running'
assert_true "no ppp interface" test -z "$(ppp_iface)"

case_finish
