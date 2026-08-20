#!/usr/bin/env bash
#
# Build the openfortiGUI Debian package.
#
# One build path for everybody: the distribution test case
# (tests/fortigate-vm/cases/91_distro.sh) runs this inside a container, and so
# does .github/workflows/build-deb.yml. Change how the package is built here, and
# both follow.
#
# Usage: packaging/build-deb.sh <source-dir> <output-dir>
#          <source-dir>   the directory holding debian/ -- i.e. openfortigui/
#          <output-dir>   where the .deb is put; created if missing
#
# The source tree is copied before building. dpkg-buildpackage writes generated
# files next to the sources and drops the .deb in the *parent* directory, and the
# tree is usually a read-only mount or the developer's working copy -- neither
# should be touched.
#
# Build dependencies come from debian/control, nowhere else. As root they are
# installed if missing; otherwise the missing ones are reported and the build
# stops, because a half-satisfied build fails much later and less clearly.

set -euo pipefail

SRC="${1:?source directory (the one containing debian/)}"
OUT="${2:?output directory}"

log() { printf '==> %s\n' "$*"; }
die() { printf 'build-deb: %s\n' "$*" >&2; exit 1; }

[[ -f "$SRC/debian/control" ]] || die "$SRC/debian/control not found"

SRC="$(cd "$SRC" && pwd)"
mkdir -p "$OUT"
OUT="$(cd "$OUT" && pwd)"

# Only the application embeds openfortivpn; the KRunner plugin does not.
if [[ -d "$SRC/openfortivpn" && ! -d "$SRC/openfortivpn/src" ]]; then
    die "the openfortivpn submodule is empty -- git submodule update --init"
fi

# --------------------------------------------------------------------------
# Build dependencies
# --------------------------------------------------------------------------

if ! command -v dpkg-checkbuilddeps >/dev/null 2>&1; then
    die "dpkg-dev is missing (dpkg-checkbuilddeps)"
fi

if ! dpkg-checkbuilddeps "$SRC/debian/control" 2>/dev/null; then
    missing="$(dpkg-checkbuilddeps "$SRC/debian/control" 2>&1 || true)"
    if [[ "$(id -u)" -eq 0 ]]; then
        log "installing build dependencies"
        export DEBIAN_FRONTEND=noninteractive
        apt-get update -qq
        # From debian/control via apt, so the list stays in one place.
        apt-get build-dep -y -qq "$SRC" \
            || die "apt-get build-dep failed. Missing: $missing"
    else
        die "build dependencies are missing and we are not root:
$missing
    sudo apt-get build-dep '$SRC'"
    fi
fi

# --------------------------------------------------------------------------
# Build in a copy
# --------------------------------------------------------------------------

WORK="$(mktemp -d)"
trap 'rm -rf "$WORK"' EXIT

REPO="$(dirname "$SRC")"
NAME="$(basename "$SRC")"

# --------------------------------------------------------------------------
# Version consistency
# --------------------------------------------------------------------------
# The single source of truth for the program version is the
# project(openfortigui VERSION ...) line in openfortigui/CMakeLists.txt; the
# package version comes from debian/changelog. Out of sync, the package would
# ship a binary that reports a different version than the .deb claims. Both
# packages release in lockstep (the runner depends on openfortigui >= version),
# so the check applies to whichever one is being built.

code_version="$(sed -nE 's/^project\(openfortigui VERSION ([0-9.]+).*/\1/p' \
    "$REPO/openfortigui/CMakeLists.txt")"
[[ -n "$code_version" ]] \
    || die "could not parse VERSION from $REPO/openfortigui/CMakeLists.txt"

pkg_version="$(dpkg-parsechangelog -l "$SRC/debian/changelog" -S Version)"
pkg_version="${pkg_version%-*}"   # strip the Debian revision (-1)

[[ "$code_version" == "$pkg_version" ]] \
    || die "version mismatch: CMakeLists.txt says $code_version, $NAME/debian/changelog says $pkg_version"

# The KRunner plugin compiles the application's sources from ../openfortigui, so
# that directory travels along whenever it is not the one being built.
COPY=("$NAME")
[[ "$NAME" != "openfortigui" && -d "$REPO/openfortigui" ]] && COPY+=("openfortigui")

log "copying ${COPY[*]}"
# Build leftovers stay behind: dpkg-buildpackage runs debian/rules clean first
# anyway, but a stale CMakeCache.txt in the source directory makes CMake refuse
# an out-of-source configure outright.
tar -C "$REPO" \
    --exclude='*/debian/openfortigui' --exclude='*/debian/.debhelper' \
    --exclude='*/build' --exclude='*/obj-*' --exclude='*/cmake-build-*' \
    --exclude='CMakeCache.txt' --exclude='*/CMakeFiles' \
    --exclude='*.o' --exclude='moc_*' \
    --exclude='ui_*.h' --exclude='qrc_*' --exclude='.git' \
    -cf - "${COPY[@]}" | tar -C "$WORK" -xf -

BUILD_SRC="$WORK/$NAME"
# A binary from an old in-source build must not end up in the package by accident.
rm -f "$BUILD_SRC/openfortigui" "$BUILD_SRC/Makefile" "$BUILD_SRC/.qmake.stash"

log "dpkg-buildpackage in $BUILD_SRC"
( cd "$BUILD_SRC" && dpkg-buildpackage -b -uc -us "-j$(nproc)" )

# --------------------------------------------------------------------------
# Collect the result
# --------------------------------------------------------------------------

shopt -s nullglob
# Both packages land here (openfortigui_*, openfortigui-runner_*); the separate
# debug symbols are of no interest.
debs=()
for candidate in "$WORK"/*.deb; do
    [[ "$candidate" == *-dbgsym_* ]] && continue
    debs+=("$candidate")
done
(( ${#debs[@]} )) || die "the build produced no .deb in $WORK"

for deb in "${debs[@]}"; do
    cp -f "$deb" "$OUT/"
    log "$OUT/$(basename "$deb")"
done

# The path the caller is after
printf '%s\n' "$OUT/$(basename "${debs[0]}")"
