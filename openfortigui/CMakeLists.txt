cmake_minimum_required(VERSION 3.16)

# Single source of truth for the program version: this project() line. The code
# reads it through the OPENFORTIGUI_VERSION compile definition below, the KRunner
# plugin parses this line (see ../krunner_openfortigui/CMakeLists.txt), and
# packaging/build-deb.sh refuses to build when debian/changelog disagrees --
# so the version is maintained only here.
project(openfortigui VERSION 0.9.13 LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 17)

set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)
set(CMAKE_AUTOUIC ON)

# 6.4 is what Ubuntu 24.04 and Debian bookworm ship, and the oldest the code is
# built against. Pinned so a too-old Qt6 fails here instead of somewhere in the
# middle of the build.
find_package(Qt6 6.4 REQUIRED COMPONENTS Core Widgets Network)
find_package(Qt6Keychain REQUIRED)

# Add 'd' suffix to debug libs
set(CMAKE_DEBUG_POSTFIX _debug)
# One bin/ at the top of the build tree, whether this file is entered directly
# (the Debian package build) or through the umbrella ../CMakeLists.txt (IDE,
# tests/fortigate-vm expects <build>/bin/openfortigui).
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin)

# These replace openfortivpn's configure run -- keep them in sync with
# openfortivpn/configure.ac when bumping the submodule. HAVE_RESOLVCONF stays
# undefined on purpose: openfortiGUI writes /etc/resolv.conf directly, so
# RESOLVCONF_PATH is empty as well. LEGACY_PPPD is only used by
# openfortivpn/src/main.c, which we do not compile.
add_definitions(-DHAVE_USR_SBIN_PPPD=1
    -DHAVE_PROC_NET_ROUTE=1
    -DHAVE_STRUCT_TERMIOS=1
    -DHAVE_RT_ENTRY_WITH_RT_DST=1
    -DHAVE_SYSTEMD=0
    -DPPP_PATH="/usr/sbin/pppd"
    -DRESOLVCONF_PATH=""
    -DHAVE_PTY_H=1
    -DHAVE_SO_BINDTODEVICE=1
    -DHAVE_PTHREAD_MUTEXATTR_SETROBUST=1
    -DHAVE_VDPRINTF=1)

# Translations are maintained by hand: after string changes run
#   lupdate6 . -ts lang/openfortigui_*.ts
# translate, then regenerate the .qm files embedded via res.qrc with
#   lrelease6 lang/openfortigui_*.ts
# The .qm files are committed, so building needs no Qt linguist tools.

#########################################################
# Sources
#########################################################

include_directories(${CMAKE_CURRENT_SOURCE_DIR} ${CMAKE_CURRENT_SOURCE_DIR}/openfortivpn)

list (APPEND FORTI_LIBS
    Qt6::Widgets
    Qt6::Network
    Qt6::Core
    crypto
    ssl
    pthread
    util
    qt6keychain
)

list (APPEND FORTI_SOURCE_FILES
main.cpp
mainwindow.cpp
openfortivpn/src/config.c
openfortivpn/src/hdlc.c
openfortivpn/src/http.c
openfortivpn/src/http_server.c
openfortivpn/src/io.c
openfortivpn/src/ipv4.c
openfortivpn/src/log.c
openfortivpn/src/tunnel.c
openfortivpn/src/userinput.c
openfortivpn/src/xml.c
vpnmanager.cpp
ticonfmain.cpp
vpnapi.cpp
proc/vpnprocess.cpp
proc/vpnbarracuda.cpp
vpnprofile.cpp
vpnprofileeditor.cpp
proc/vpnworker.cpp
vpngroup.cpp
vpngroupeditor.cpp
vpnsetting.cpp
vpnlogin.cpp
vpnhelper.cpp
vpnlogger.cpp
vpnotplogin.cpp
setupwizard.cpp
vpnchangelog.cpp
)

# Add resource file if needed
list (APPEND FORTI_SOURCE_FILES res.qrc)


#Add UI Files if needed
list (APPEND FORTI_SOURCE_FILES
    mainwindow.ui
    vpnprofileeditor.ui
    vpngroupeditor.ui
    vpnsetting.ui
    vpnlogin.ui
    vpnotplogin.ui
    setupwizard.ui
    vpnchangelog.ui
)

# Find includes in corresponding build directories
set(CMAKE_INCLUDE_CURRENT_DIR ON)


add_executable(${PROJECT_NAME} ${FORTI_SOURCE_FILES})

target_compile_definitions(${PROJECT_NAME} PRIVATE OPENFORTIGUI_VERSION="${PROJECT_VERSION}")

target_link_libraries (${PROJECT_NAME} ${FORTI_LIBS} ${EXTRA_LIBRARIES})

#########################################################
# Install (used by debian/rules via dh_auto_install)
#########################################################

include(GNUInstallDirs)

install(TARGETS ${PROJECT_NAME} RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR})
install(FILES app-entry/openfortigui.desktop
    DESTINATION ${CMAKE_INSTALL_DATADIR}/applications)
install(FILES app-entry/openfortigui.png
    DESTINATION ${CMAKE_INSTALL_DATADIR}/pixmaps)
# The sudoers fragment must be 0440 or sudo rejects it.
install(FILES sudo/openfortigui
    DESTINATION /etc/sudoers.d
    PERMISSIONS OWNER_READ GROUP_READ)
