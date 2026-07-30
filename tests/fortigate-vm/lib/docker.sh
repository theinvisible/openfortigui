# shellcheck shell=bash
#
# Container support for the distribution matrix (cases/91_distro.sh).
#
# One image per target, built from docker/distro/Dockerfile with the base image as
# a build argument. The container supplies the user space under test -- the
# distribution, its sudo, its Qt -- and nothing else. Network and pid namespace
# are shared with the host on purpose:
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

: "${LAB_DOCKER_PREFIX:=openfortigui-lab}"

# dock_require -> the docker daemon has to be usable
dock_require() {
    have docker || die "docker is missing: sudo apt install docker.io"
    docker info >/dev/null 2>&1 \
        || die "cannot talk to the docker daemon -- is the user in the 'docker' group?"
}

# dock_slug <image> -> "ubuntu:24.04" becomes "ubuntu-24.04"
dock_slug() { printf '%s' "${1//[:\/]/-}"; }

# dock_tag <image> -> the tag of our image for that target
dock_tag() { printf '%s:%s' "$LAB_DOCKER_PREFIX" "$(dock_slug "$1")"; }

# dock_deb_dir <image> -> where a package built for that target is kept
dock_deb_dir() { printf '%s/deb/%s' "$OFGUI_LAB_DIR" "$(dock_slug "$1")"; }

# dock_cached_deb <image>
#
# Prints the path of a cached package if it is still newer than every source
# file, otherwise nothing. Building takes minutes, so a run that changed no code
# should not pay for it again.
dock_cached_deb() {
    local dir; dir="$(dock_deb_dir "$1")"
    local deb; deb="$(find "$dir" -maxdepth 1 -name 'openfortigui_*.deb' 2>/dev/null | head -n1)"
    [[ -n "$deb" ]] || return 1

    # Any source file newer than the package invalidates it.
    local newer
    newer="$(find "$REPO_ROOT/openfortigui" "$REPO_ROOT/krunner_openfortigui" \
        "$REPO_ROOT/packaging" \
        \( -name '*.cpp' -o -name '*.h' -o -name '*.ui' -o -name '*.pro' \
           -o -name '*.qrc' -o -name '*.sh' -o -path '*/debian/*' \) \
        -newer "$deb" -print -quit 2>/dev/null)"
    [[ -n "$newer" ]] && return 1

    printf '%s' "$deb"
}

# dock_build <image> <use_sudo_rs yes|no> <with_runner yes|no>
#
# The build context is assembled in a temporary directory: the Dockerfile needs
# the debian/control files for the build dependencies, and copying only those
# keeps the layer cache alive across source changes.
dock_build() {
    local image="$1" use_rs="${2:-no}" with_runner="${3:-no}" ctx
    ctx="$(mktemp -d)"
    cp "$LAB_SRC_DIR/docker/distro/Dockerfile" \
       "$LAB_SRC_DIR/docker/distro/in-container.sh" "$ctx/"
    cp "$REPO_ROOT/openfortigui/debian/control" "$ctx/control"
    cp "$REPO_ROOT/krunner_openfortigui/debian/control" "$ctx/control-runner"

    info "building the image for $image (sudo-rs: $use_rs, KRunner plugin: $with_runner)"
    docker build \
        --build-arg "BASE_IMAGE=$image" \
        --build-arg "USE_SUDO_RS=$use_rs" \
        --build-arg "WITH_RUNNER=$with_runner" \
        --build-arg "WITH_BUILD_DEPS=yes" \
        -t "$(dock_tag "$image")" "$ctx" \
        >"$CASE_OUT_DIR/docker-build-$(dock_slug "$image").log" 2>&1
    local rc=$?
    rm -rf "$ctx"
    (( rc == 0 )) || die "image build for $image failed:
$(tail -n 20 "$CASE_OUT_DIR/docker-build-$(dock_slug "$image").log")"
}

# dock_run <image> <deb|build> <args for in-container.sh...>
dock_run() {
    local image="$1" deb="$2"; shift 2
    # The source tree is always mounted read-only: the application package may
    # come from the cache, but the KRunner plugin is always built from source, and
    # a build inside the container copies the tree before touching it, so nothing
    # of ours ends up root-owned in the working copy.
    local -a mounts=(-v "$OFGUI_LAB_DIR:$OFGUI_LAB_DIR" -v "$REPO_ROOT:/src:ro")

    if [[ "$deb" != "build" ]]; then
        mounts+=(-v "$deb:/tmp/openfortigui.deb:ro")
        deb=/tmp/openfortigui.deb
    fi

    docker run --rm \
        --name "openfortigui-$(dock_slug "$image")" \
        --network host --pid host \
        --cap-add NET_ADMIN --device /dev/ppp \
        "${mounts[@]}" \
        "$(dock_tag "$image")" \
        "$deb" "$@"
}

# dock_cleanup <image> -> remove a container left over from an aborted run
dock_cleanup() {
    docker rm -f "openfortigui-$(dock_slug "$1")" >/dev/null 2>&1 || true
}

# dock_result <output file> <key> -> the value the container reported
dock_result() {
    sed -n "s/^RESULT:$2=//p" "$1" | tail -n1
}
