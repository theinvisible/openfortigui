cmake_minimum_required(VERSION 3.16)

# Umbrella for developer builds: cmake -S <repo> -B cmake-build-debug builds the
# application, with the binary at <build>/bin/openfortigui (tests/fortigate-vm
# relies on that path). No version here on purpose: the version is maintained
# centrally in openfortigui/CMakeLists.txt (project(... VERSION ...)), which is
# also what the Debian package build enters directly.
#
# The KRunner plugin (krunner_openfortigui/) is not part of this umbrella: it
# needs KF6/ECM, which the older target distributions do not ship. It builds
# standalone through its own CMakeLists.txt and debian/.
project(openfortigui-repo LANGUAGES NONE)

add_subdirectory(openfortigui)
