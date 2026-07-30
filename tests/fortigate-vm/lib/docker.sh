# shellcheck shell=bash
#
# Container support for the sudo-rs case (issue #208).
#
# The container supplies the user space under test -- Ubuntu 26.04 with sudo-rs --
# and nothing else. Network and pid namespace are shared with the host on purpose:
#
#   --network host  the FortiGate hangs off the lab bridge, and the existing cases
#                   already connect from there. What is isolated is the sudo and
#                   packaging layer, not the network; the ppp interface appears on
#                   the host, where client_cleanup_all() removes it again.
#   --pid host      otherwise the pids inside the container are namespace-local and
#                   pid_alive()/client_pid() (lib/common.sh) would look in vain.
#
# The lab directory is mounted at its own path so that --main-config and
# --api-socket mean the same thing on both sides; a QLocalSocket on an absolute
# path is a filesystem socket and survives the mount.

: "${LAB_DOCKER_IMAGE:=openfortigui-sudors:test}"
: "${OFGUI_DEB:=}"

# dock_require -> the docker daemon has to be usable
dock_require() {
    have docker || die "docker is missing: sudo apt install docker.io"
    docker info >/dev/null 2>&1 \
        || die "cannot talk to the docker daemon -- is the user in the 'docker' group?"
}

# dock_find_deb -> path of the package to install in the container
dock_find_deb() {
    if [[ -n "$OFGUI_DEB" ]]; then
        [[ -f "$OFGUI_DEB" ]] || die "OFGUI_DEB=$OFGUI_DEB does not exist"
        printf '%s' "$OFGUI_DEB"
        return 0
    fi
    local newest
    newest="$(find "$REPO_ROOT" -maxdepth 1 -name 'openfortigui_*.deb' \
        -printf '%T@ %p\n' 2>/dev/null | sort -rn | head -n1 | cut -d' ' -f2-)"
    [[ -n "$newest" ]] || return 1
    printf '%s' "$newest"
}

# dock_build -> build the image (cheap after the first time, layers are cached)
dock_build() {
    local ctx="$LAB_SRC_DIR/docker/sudo-rs"
    info "building the container image $LAB_DOCKER_IMAGE"
    docker build -t "$LAB_DOCKER_IMAGE" "$ctx" >"$CASE_OUT_DIR/docker-build.log" 2>&1 \
        || die "image build failed, see $CASE_OUT_DIR/docker-build.log:
$(tail -n 15 "$CASE_OUT_DIR/docker-build.log")"
}

# dock_run <deb> <args for in-container.sh...>
dock_run() {
    local deb="$1"; shift
    docker run --rm \
        --name openfortigui-sudors \
        --network host --pid host \
        --cap-add NET_ADMIN --device /dev/ppp \
        -v "$OFGUI_LAB_DIR:$OFGUI_LAB_DIR" \
        -v "$deb:/tmp/openfortigui.deb:ro" \
        "$LAB_DOCKER_IMAGE" \
        /tmp/openfortigui.deb "$@"
}

# dock_cleanup -> kill a container left over from an aborted run
dock_cleanup() {
    docker rm -f openfortigui-sudors >/dev/null 2>&1 || true
}

# dock_result <output file> <key> -> the value the container reported
dock_result() {
    sed -n "s/^RESULT:$2=//p" "$1" | tail -n1
}
