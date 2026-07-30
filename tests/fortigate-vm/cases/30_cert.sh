#!/usr/bin/env bash
#
# Certificate paths.
#
# The FortiGate presents Fortinet_Factory, i.e. a self-signed certificate --
# exactly the case openfortiGUI handles with trusted_cert, trust_all_gw_certs
# and insecure_ssl. This also tests that the digest appears in the log in the
# form MainWindow::onClientCertValidationFAiled() extracts it with the regex
# "--trusted-cert (.*?)\n" (mainwindow.cpp:780).

set -uo pipefail
source "$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)/lib/case.sh"
case_setup

# --------------------------------------------------------------------------
part "a) an unknown gateway certificate is rejected"
# --------------------------------------------------------------------------

LOG_A="$(case_log a-untrusted)"
client_write_profile "lab-cert-untrusted" "trusted_cert=" "trust_all_gw_certs=false" >/dev/null
client_start "lab-cert-untrusted" "$LOG_A" || fail "process did not start"

if client_wait_log "$LOG_A" 'digest is not in the local whitelist' 40; then
    ok "certificate validation fails as expected"
else
    fail "expected certificate warning missing" "$(tail -n 25 "$LOG_A")"
fi

# Exactly the extraction the GUI performs
DIGEST_FROM_LOG="$(sed -nE 's/.*--trusted-cert ([0-9a-f]{64}).*/\1/p' "$LOG_A" | head -n1)"
if [[ -n "$DIGEST_FROM_LOG" ]]; then
    assert_eq "digest from the log matches the gateway certificate" \
        "$LAB_GW_DIGEST" "$DIGEST_FROM_LOG"
else
    fail "no '--trusted-cert <digest>' in the log" \
        "Without that line MainWindow cannot find the hash.
$(grep -iE 'trusted.cert|whitelist' "$LOG_A" | head -n 5)"
fi

assert_contains "connection is closed" "$LOG_A" 'Closed connection to gateway'
assert_true "no ppp interface was created" test -z "$(ppp_iface)"
client_stop TERM 20 >/dev/null 2>&1 || true

# --------------------------------------------------------------------------
part "b) a wrong trusted_cert is rejected"
# --------------------------------------------------------------------------

LOG_B="$(case_log b-wrong-digest)"
WRONG_DIGEST="$(printf '%064d' 0 | tr '0' 'a')"
client_write_profile "lab-cert-wrong" "trusted_cert=$WRONG_DIGEST" >/dev/null
client_start "lab-cert-wrong" "$LOG_B" || fail "process did not start"

if client_wait_log "$LOG_B" 'digest is not in the local whitelist' 40; then
    ok "a wrong digest is not accepted"
else
    fail "the wrong digest went unnoticed" "$(tail -n 25 "$LOG_B")"
fi
assert_true "no ppp interface was created" test -z "$(ppp_iface)"
client_stop TERM 20 >/dev/null 2>&1 || true

# --------------------------------------------------------------------------
part "c) insecure_ssl does NOT bypass certificate validation"
# --------------------------------------------------------------------------

# Contrary to what the name suggests, insecure_ssl in openfortivpn 1.24.1 only
# affects the cipher list and the TLS protocol options
# (openfortivpn/src/tunnel.c:1039-1084). The digest whitelist is checked
# independently of it. A profile with insecure_ssl=true but no trusted_cert
# therefore does not connect.
LOG_C="$(case_log c-insecure-alone)"
client_write_profile "lab-cert-insecure" "trusted_cert=" "insecure_ssl=true" >/dev/null
client_start "lab-cert-insecure" "$LOG_C" || fail "process did not start"

if client_wait_log "$LOG_C" 'digest is not in the local whitelist' 40; then
    ok "insecure_ssl alone does not disable the digest check"
else
    if grep -q 'Tunnel is up and running' "$LOG_C" 2>/dev/null; then
        fail "insecure_ssl skipped certificate validation" \
            "Behaviour has changed -- openfortivpn used to check the digest
independently of insecure_ssl (tunnel.c:1039)."
    else
        fail "unexpected outcome" "$(tail -n 20 "$LOG_C")"
    fi
fi
client_stop TERM 20 >/dev/null 2>&1 || true

# Counter-check: with a pinned certificate, insecure_ssl does not get in the way
LOG_C2="$(case_log c-insecure-pinned)"
client_write_profile "lab-cert-insecure2" \
    "trusted_cert=$LAB_GW_DIGEST" "insecure_ssl=true" >/dev/null
client_start "lab-cert-insecure2" "$LOG_C2" || fail "process did not start"
if client_wait_log "$LOG_C2" 'Tunnel is up and running' "$TIMEOUT_CONNECT"; then
    ok "insecure_ssl together with trusted_cert connects"
else
    fail "insecure_ssl with trusted_cert did not connect" "$(tail -n 20 "$LOG_C2")"
fi
client_stop TERM 30 >/dev/null 2>&1 || true

# --------------------------------------------------------------------------
part "d) trust_all_gw_certs picks the hash up automatically"
# --------------------------------------------------------------------------

# With trust_all_gw_certs, vpnProcess::startVPN() (vpnprocess.cpp:180) reads the
# hash from gw_cert.cache. That path used to depend on HOME rather than on
# --main-config, because tiConfMain::main_gw_cert_cache is initialized statically
# (ticonfmain.cpp:32) and setMainConfig() did not recompute it. It does now;
# 80_env b) is the case that proves it, by running the same path with a
# deliberately wrong HOME.
CACHE="$LAB_CLIENT_HOME/.openfortigui/gw_cert.cache"
cat >"$CACHE" <<EOF
[gw_cert_hashes]
lab-cert-cache=$LAB_GW_DIGEST
EOF

LOG_D="$(case_log d-cache)"
client_write_profile "lab-cert-cache" "trusted_cert=" "trust_all_gw_certs=true" >/dev/null
client_start "lab-cert-cache" "$LOG_D" || fail "process did not start"

if client_wait_log "$LOG_D" 'Tunnel is up and running' "$TIMEOUT_CONNECT"; then
    ok "the hash from gw_cert.cache is used"
else
    fail "connecting with the gw_cert.cache hash failed" \
        "If the log says 'read gw_cert_cache' but no hash took effect,
openfortiGUI looks for the file outside $LAB_CLIENT_HOME -- see
tiConfMain::main_gw_cert_cache (ticonfmain.cpp:32).
$(tail -n 25 "$LOG_D")"
fi
client_stop TERM 30 >/dev/null 2>&1 || true
rm -f "$CACHE"

# --------------------------------------------------------------------------
part "e) TLS variants"
# --------------------------------------------------------------------------

for variant in "min_tls=1.2" "seclevel1=true" "min_tls=1.0"; do
    key="${variant%%=*}"; val="${variant#*=}"
    LOG_E="$(case_log "e-${key}-${val}")"
    client_write_profile "lab-cert-tls" "trusted_cert=$LAB_GW_DIGEST" "$key=$val" >/dev/null
    client_start "lab-cert-tls" "$LOG_E" || { fail "process did not start ($variant)"; continue; }
    if client_wait_log "$LOG_E" 'Tunnel is up and running' "$TIMEOUT_CONNECT"; then
        ok "tunnel is up with $variant"
    else
        fail "tunnel with $variant did not come up" "$(tail -n 20 "$LOG_E")"
    fi
    client_stop TERM 30 >/dev/null 2>&1 || true
done

# min_tls=1.3 depends on the FortiGate's ssl-max-proto-ver and is not guaranteed
# in evaluation mode -- so it is reported, not asserted.
LOG_F="$(case_log e-min_tls-1.3)"
client_write_profile "lab-cert-tls13" "trusted_cert=$LAB_GW_DIGEST" "min_tls=1.3" >/dev/null
client_start "lab-cert-tls13" "$LOG_F" || true
if client_wait_log "$LOG_F" 'Tunnel is up and running' 40; then
    ok "TLS 1.3 is supported by this FortiGate"
else
    skip "min_tls=1.3" "the FortiGate does not offer TLS 1.3 (informational)"
fi
client_stop TERM 30 >/dev/null 2>&1 || true

# --------------------------------------------------------------------------
part "f) SNI"
# --------------------------------------------------------------------------

# openfortivpn puts config.sni into the TLS handshake and falls back to
# gateway_host when it is empty (tunnel.c:1308-1315). openfortiGUI passes the
# profile field through in vpnWorker::process(). Requires submodule >= v1.22.
LOG_G="$(case_log f-sni)"
SNI_NAME="vpn.example.test"
client_write_profile "lab-cert-sni" "trusted_cert=$LAB_GW_DIGEST" "sni=$SNI_NAME" >/dev/null
client_start "lab-cert-sni" "$LOG_G" || fail "process did not start"

if client_wait_log "$LOG_G" 'Tunnel is up and running' "$TIMEOUT_CONNECT"; then
    ok "tunnel is up with an SNI set"
else
    fail "tunnel with an SNI did not come up" "$(tail -n 20 "$LOG_G")"
fi
assert_contains "the configured SNI reaches the handshake" "$LOG_G" \
    "Set SNI for TLS handshake: $SNI_NAME"
client_stop TERM 30 >/dev/null 2>&1 || true

# Empty SNI must keep using the gateway host, not send an empty name
LOG_H="$(case_log f-sni-default)"
client_write_profile "lab-cert-nosni" "trusted_cert=$LAB_GW_DIGEST" >/dev/null
client_start "lab-cert-nosni" "$LOG_H" || fail "process did not start"
if client_wait_log "$LOG_H" 'Tunnel is up and running' "$TIMEOUT_CONNECT"; then
    ok "tunnel is up without an SNI"
else
    fail "tunnel without an SNI did not come up" "$(tail -n 20 "$LOG_H")"
fi
assert_contains "without an SNI the gateway host is used" "$LOG_H" \
    "Set SNI for TLS handshake: $FGT_WAN_IP"
client_stop TERM 30 >/dev/null 2>&1 || true

# --------------------------------------------------------------------------
part "g) an encrypted client key asks for its passphrase"
# --------------------------------------------------------------------------

# openfortivpn asks for the passphrase of an encrypted private key through
# pem_passphrase_cb() (tunnel.c), which prints "Enter PEM pass phrase: " to
# stdout and then waits on stdin. vpnLogger recognises prompts by exactly that
# text and opens a dialog; without a matching pattern the process waits forever,
# which is what issue #166 describes.
#
# Headless there is nobody to answer, so this only pins the prompt down: it has
# to appear in the output, or the GUI has no way of knowing about it. The
# FortiGate does not need to accept the certificate for that -- the key is loaded
# before the handshake.
KEYDIR="$CASE_OUT_DIR/clientcert"
mkdir -p "$KEYDIR"
if openssl req -x509 -newkey rsa:2048 -keyout "$KEYDIR/key.pem" \
        -out "$KEYDIR/cert.pem" -days 2 -passout pass:labpass \
        -subj "/CN=openfortigui-lab" >/dev/null 2>&1; then
    LOG_I="$(case_log g-pem-passphrase)"
    client_write_profile "lab-cert-pemkey" "trusted_cert=$LAB_GW_DIGEST" \
        "user_cert=$KEYDIR/cert.pem" "user_key=$KEYDIR/key.pem" >/dev/null
    client_start "lab-cert-pemkey" "$LOG_I" || fail "process did not start"

    if client_wait_log "$LOG_I" 'Enter PEM pass phrase' 30; then
        ok "the passphrase prompt appears in the output"
    else
        fail "no passphrase prompt for the encrypted key" \
            "vpnLogger matches on \"PEM pass phrase\" (vpnlogger.cpp). If the
wording changed upstream, the pattern has to follow or the dialog never opens.
$(tail -n 20 "$LOG_I")"
    fi
    # It is waiting on stdin now -- no tunnel, and it will not exit on its own.
    assert_not_contains "no tunnel while waiting for the passphrase" "$LOG_I" \
        'Tunnel is up and running'
    client_stop TERM 20 >/dev/null 2>&1 || client_kill >/dev/null 2>&1 || true
else
    skip "encrypted client key" "openssl could not create a test key"
fi

case_finish
