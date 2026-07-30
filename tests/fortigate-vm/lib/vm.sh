# shellcheck shell=bash
#
# VM management: unpack the image, qcow2 overlays, start/stop QEMU.
# Deliberately without libvirt -- only qemu is installed on this host and
# /dev/kvm is granted to the user via an ACL, so the VM needs no root
# privileges (only the pre-created taps do, see lib/net.sh).

# --------------------------------------------------------------------------
# Image
# --------------------------------------------------------------------------

# Sets FGT_IMAGE_ZIP (if empty), FGT_VERSION, LAB_BASE_IMG, LAB_GOLDEN_IMG.
vm_resolve_image() {
    if [[ -z "$FGT_IMAGE_ZIP" ]]; then
        local newest
        newest="$(ls -t "$HOME"/FGT_VM64_KVM-*.kvm.zip 2>/dev/null | head -n1)"
        [[ -n "$newest" ]] || die "no FortiGate KVM image found. Set FGT_IMAGE_ZIP (see lab.env.example)."
        FGT_IMAGE_ZIP="$newest"
    fi
    [[ -f "$FGT_IMAGE_ZIP" ]] || die "image not found: $FGT_IMAGE_ZIP"

    FGT_VERSION="$(basename "$FGT_IMAGE_ZIP" \
        | sed -nE 's/.*-v([0-9]+\.[0-9]+\.[0-9]+)\..*-build([0-9]+).*/\1-b\2/p')"
    [[ -n "$FGT_VERSION" ]] || FGT_VERSION="unknown"

    LAB_BASE_IMG="$LAB_IMG_DIR/base-$FGT_VERSION.qcow2"
    LAB_GOLDEN_IMG="$LAB_IMG_DIR/golden-$FGT_VERSION.qcow2"
    LAB_RUN_IMG="$LAB_RUN_DIR/disk.qcow2"
    LAB_LOG_IMG="$LAB_IMG_DIR/logdisk-$FGT_VERSION.qcow2"
    export FGT_IMAGE_ZIP FGT_VERSION LAB_BASE_IMG LAB_GOLDEN_IMG LAB_RUN_IMG
}

# vm_prepare [--rebuild]
# Unpacks the base image (idempotent) and creates the golden overlay.
vm_prepare() {
    local rebuild=0
    [[ "${1:-}" == "--rebuild" ]] && rebuild=1

    vm_resolve_image
    lab_mkdirs

    if [[ ! -f "$LAB_BASE_IMG" ]]; then
        step "Unpacking the base image ($FGT_VERSION)"
        local tmp
        tmp="$(mktemp -d "$LAB_IMG_DIR/.unzip.XXXXXX")"
        unzip -q -o "$FGT_IMAGE_ZIP" -d "$tmp" \
            || { rm -rf "$tmp"; die "unzip of $FGT_IMAGE_ZIP failed"; }
        local qcow
        qcow="$(find "$tmp" -name '*.qcow2' -print -quit)"
        [[ -n "$qcow" ]] || { rm -rf "$tmp"; die "no .qcow2 found inside the ZIP"; }
        mv "$qcow" "$LAB_BASE_IMG"
        rm -rf "$tmp"
        chmod a-w "$LAB_BASE_IMG"
        info "base image: $LAB_BASE_IMG"
    else
        info "base image present: $LAB_BASE_IMG"
    fi

    if (( rebuild )) && [[ -f "$LAB_GOLDEN_IMG" ]]; then
        info "discarding the existing golden image"
        rm -f "$LAB_GOLDEN_IMG"
    fi

    if [[ ! -f "$LAB_GOLDEN_IMG" ]]; then
        step "Creating the golden overlay (unprovisioned)"
        qemu-img create -q -f qcow2 -b "$LAB_BASE_IMG" -F qcow2 "$LAB_GOLDEN_IMG" \
            || die "qemu-img create (golden) failed"
        : >"$LAB_IMG_DIR/golden-$FGT_VERSION.unprovisioned"
    fi

    if (( FGT_LOG_DISK_GB > 0 )) && [[ ! -f "$LAB_LOG_IMG" ]]; then
        step "Creating the FortiOS log disk (${FGT_LOG_DISK_GB}G)"
        qemu-img create -q -f qcow2 "$LAB_LOG_IMG" "${FGT_LOG_DISK_GB}G" \
            || die "qemu-img create (logdisk) failed"
    fi
}

vm_golden_is_provisioned() {
    vm_resolve_image
    [[ -f "$LAB_GOLDEN_IMG" && ! -f "$LAB_IMG_DIR/golden-$FGT_VERSION.unprovisioned" ]]
}

vm_in_provision_mode() {
    [[ -f "$LAB_RUN_DIR/provision-mode" ]]
}

vm_mark_golden_provisioned() {
    vm_resolve_image
    rm -f "$LAB_IMG_DIR/golden-$FGT_VERSION.unprovisioned"
}

# A fresh throwaway overlay on top of golden, so every run starts from an
# identical state.
vm_fresh_overlay() {
    vm_resolve_image
    [[ -f "$LAB_GOLDEN_IMG" ]] || die "golden image missing -- run 'testlab prepare' first"
    rm -f "$LAB_RUN_IMG"
    qemu-img create -q -f qcow2 -b "$LAB_GOLDEN_IMG" -F qcow2 "$LAB_RUN_IMG" \
        || die "qemu-img create (run) failed"
}

# --------------------------------------------------------------------------
# QEMU
# --------------------------------------------------------------------------

vm_running() {
    [[ -f "$LAB_QEMU_PID" ]] || return 1
    local pid
    pid="$(cat "$LAB_QEMU_PID" 2>/dev/null)"
    pid_alive "$pid" && pid_cmdline_has "$pid" "qemu-system"
}

vm_pid() { cat "$LAB_QEMU_PID" 2>/dev/null; }

# vm_start [--provision-mode]
# In provisioning mode we work on the golden image directly (the configuration
# is meant to persist); otherwise on the throwaway overlay.
vm_start() {
    local mode="${1:-run}"
    vm_resolve_image
    lab_mkdirs

    if vm_running; then
        info "VM is already running (PID $(vm_pid))"
        return 0
    fi

    net_require_up

    local disk
    if [[ "$mode" == "--provision-mode" ]]; then
        disk="$LAB_GOLDEN_IMG"
        # Marker so that 'provision' and 'license' know their changes end up
        # persistently in the golden image.
        : >"$LAB_RUN_DIR/provision-mode"
        info "VM starts in provisioning mode (writes to golden)"
    else
        rm -f "$LAB_RUN_DIR/provision-mode"
        vm_fresh_overlay
        disk="$LAB_RUN_IMG"
    fi

    rm -f "$LAB_CONSOLE_SOCK" "$LAB_QMP_SOCK" "$LAB_QEMU_PID"

    local -a args=(
        -name "$FGT_VM_NAME"
        -machine "$FGT_MACHINE,accel=kvm"
        -cpu host
        -smp "$FGT_CPUS"
        -m "$FGT_MEM_MB"
        -drive "if=${FGT_DISK_IF:-virtio},file=$disk,format=qcow2,cache=writeback"
        -netdev "tap,id=nout,ifname=$LAB_TAP_OUT,script=no,downscript=no"
        -device "$FGT_NIC_MODEL,netdev=nout,mac=52:54:00:0f:61:01"
        -netdev "tap,id=nin,ifname=$LAB_TAP_IN,script=no,downscript=no"
        -device "$FGT_NIC_MODEL,netdev=nin,mac=52:54:00:0f:61:02"
        # logfile= keeps a complete console transcript, even while no client is
        # attached to the socket.
        -chardev "socket,id=cons,path=$LAB_CONSOLE_SOCK,server=on,wait=off,logfile=$LAB_OUT_DIR/console.log,logappend=on"
        -serial chardev:cons
        -qmp "unix:$LAB_QMP_SOCK,server=on,wait=off"
        -display none
        -daemonize
        -pidfile "$LAB_QEMU_PID"
    )

    if (( FGT_LOG_DISK_GB > 0 )); then
        args+=( -drive "if=${FGT_DISK_IF:-virtio},file=$LAB_LOG_IMG,format=qcow2,cache=writeback" )
    fi

    step "Starting the VM (${FGT_CPUS} vCPU, ${FGT_MEM_MB} MB, $FGT_NIC_MODEL)"
    printf '%s\n' "qemu-system-x86_64 ${args[*]}" >"$LAB_OUT_DIR/qemu-cmdline.txt"
    if ! qemu-system-x86_64 "${args[@]}" 2>"$LAB_OUT_DIR/qemu-stderr.log"; then
        err "QEMU failed to start:"
        sed 's/^/    /' "$LAB_OUT_DIR/qemu-stderr.log" >&2
        die "If q35/virtio causes trouble, set FGT_MACHINE=pc or FGT_NIC_MODEL=e1000."
    fi

    wait_for 15 "QEMU sockets" test -S "$LAB_QMP_SOCK" || die "QMP socket never appeared"
    info "VM running (PID $(vm_pid)), console: $LAB_CONSOLE_SOCK"
}

# vm_qmp '{"execute":"..."}'
vm_qmp() {
    have socat || die "socat is required"
    [[ -S "$LAB_QMP_SOCK" ]] || return 1
    printf '{"execute":"qmp_capabilities"}\n%s\n' "$1" \
        | timeout 10 socat - "UNIX-CONNECT:$LAB_QMP_SOCK" 2>/dev/null
}

vm_stop() {
    if ! vm_running; then
        rm -f "$LAB_QEMU_PID" "$LAB_QMP_SOCK" "$LAB_CONSOLE_SOCK"
        return 0
    fi
    local pid
    pid="$(vm_pid)"
    step "Shutting the VM down (PID $pid)"

    vm_qmp '{"execute":"system_powerdown"}' >/dev/null 2>&1 || true
    local waited=0
    while pid_alive "$pid" && (( waited < 45 )); do
        sleep 1; waited=$(( waited + 1 ))
    done

    if pid_alive "$pid"; then
        # FortiOS does not react to an ACPI shutdown reliably. The configuration
        # is already on disk after every "end", so a hard QMP quit is harmless.
        warn "ACPI shutdown had no effect, sending QMP quit"
        vm_qmp '{"execute":"quit"}' >/dev/null 2>&1 || true
        waited=0
        while pid_alive "$pid" && (( waited < 10 )); do
            sleep 1; waited=$(( waited + 1 ))
        done
    fi

    if pid_alive "$pid"; then
        warn "forcing SIGKILL"
        kill -9 "$pid" 2>/dev/null || true
        sleep 1
    fi

    rm -f "$LAB_QEMU_PID" "$LAB_QMP_SOCK" "$LAB_CONSOLE_SOCK"
    info "VM stopped"
}

vm_console() {
    have socat || die "socat is required"
    [[ -S "$LAB_CONSOLE_SOCK" ]] || die "no console -- is the VM running?"
    cat <<EOF
${C_BOLD}FortiGate console${C_OFF}  (quit with Ctrl-O)
Login: $FGT_ADMIN_USER / $FGT_ADMIN_PASS   (empty password on first boot)
Note: one client at a time -- no concurrent 'provision'.
EOF
    socat -,raw,echo=0,escape=0x0f "UNIX-CONNECT:$LAB_CONSOLE_SOCK"
}

# Waits until the SSL-VPN port speaks TLS.
vm_wait_sslvpn() {
    wait_for "$TIMEOUT_TLS" "SSL-VPN on $FGT_WAN_IP:$FGT_SSLVPN_PORT" \
        tcp_open "$FGT_WAN_IP" "$FGT_SSLVPN_PORT"
}

# SHA-256 digest of the gateway certificate -- identical to openfortivpn's
# X509_digest() over the DER certificate.
vm_gateway_cert_digest() {
    openssl s_client -connect "$FGT_WAN_IP:$FGT_SSLVPN_PORT" \
            -servername "$FGT_WAN_IP" </dev/null 2>/dev/null \
        | openssl x509 -outform der 2>/dev/null \
        | sha256sum | cut -d' ' -f1
}

# --------------------------------------------------------------------------
# FortiOS-CLI (fgt_provision.py)
# --------------------------------------------------------------------------

# fgt_cli_raw <action> [args...]  -- e.g. wait-login, provision, show, cmd
fgt_cli_raw() {
    [[ -S "$LAB_CONSOLE_SOCK" ]] || die "no FortiGate console -- is the VM running?"
    env \
        LAB_CONSOLE_SOCK="$LAB_CONSOLE_SOCK" \
        LAB_PROVISION_LOG="$LAB_OUT_DIR/provision.log" \
        FGT_ADMIN_USER="$FGT_ADMIN_USER" \
        FGT_ADMIN_PASS="$FGT_ADMIN_PASS" \
        FGT_ADMIN_SPORT="$FGT_ADMIN_SPORT" \
        FGT_HOSTNAME="$FGT_HOSTNAME" \
        FGT_WAN_IP="$FGT_WAN_IP" \
        FGT_LAN_IP="$FGT_LAN_IP" \
        LAB_INSIDE_NET="$LAB_INSIDE_NET" \
        LAB_HOST_IP="$LAB_HOST_IP" \
        FGT_SSLVPN_PORT="$FGT_SSLVPN_PORT" \
        VPN_USER="$VPN_USER" \
        VPN_PASS="$VPN_PASS" \
        VPN_GROUP="$VPN_GROUP" \
        VPN_PORTAL="$VPN_PORTAL" \
        FGT_POLICY_NAT="$FGT_POLICY_NAT" \
        TIMEOUT_BOOT="$TIMEOUT_BOOT" \
        python3 "$LAB_SRC_DIR/fgt_provision.py" "$@"
}

# fgt_cli "<FortiOS command>" [...]
fgt_cli() {
    [[ $# -gt 0 ]] || die "no FortiOS command given"
    fgt_cli_raw cmd "$@"
}

# Quick TLS handshake diagnostics (evaluation mode may restrict the crypto).
vm_tls_info() {
    openssl s_client -connect "$FGT_WAN_IP:$FGT_SSLVPN_PORT" \
            -servername "$FGT_WAN_IP" </dev/null 2>/dev/null \
        | grep -E '^(New|Protocol|Cipher|Server public key|Verification)' \
        | sed 's/^/    /'
}
