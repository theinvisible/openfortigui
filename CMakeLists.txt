cmake_minimum_required (VERSION 3.9)
project (openfortigui)

set (CMAKE_CONFIGURATION_TYPES Debug Release)

# Set the CXX standard as C++11
set(CMAKE_CXX_STANDARD 11)

set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)
set(CMAKE_AUTOUIC ON)

find_package(Qt5 REQUIRED Core Widgets Network Keychain DBus)

# Add 'd' suffix to debug libs
set(CMAKE_DEBUG_POSTFIX _debug)
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY bin)

# Generate debug symbol file (pdb)
if (${CMAKE_SYSTEM_NAME} MATCHES "Windows")
    set(CMAKE_SHARED_LINKER_FLAGS "${CMAKE_SHARED_LINKER_FLAGS} /DEBUG")
endif ()

# Set lib flag
# add_definitions(-DFORTI_LIBRARY)
add_definitions(-DHAVE_USR_SBIN_PPPD=1
    -DHAVE_PROC_NET_ROUTE=1
    -DHAVE_STRUCT_TERMIOS=1
    -DHAVE_RT_ENTRY_WITH_RT_DST=1
    -DHAVE_SYSTEMD=0
    -DHAVE_X509_CHECK_HOST=1
    -DPPP_PATH="/usr/sbin/pppd"
    -DRESOLVCONF_PATH=""
    -DHAVE_PTY_H=1
    -DSUPPORT_OBSOLETE_CODE=1
    -DHAVE_SO_BINDTODEVICE=1
    -DOPENSSL_ENGINE=1)

#########################################################
# Sources
#########################################################

# Add the qt include dirs
include_directories(${QT_INCLUDE_DIRS})

include_directories(openfortigui openfortigui/openfortivpn)

list (APPEND FORTI_LIBS
    Qt5::Widgets
    Qt5::Network
    Qt5::Core
    crypto
    ssl
    pthread
    util
    qt5keychain
)

list (APPEND FORTI_SOURCE_FILES
openfortigui/main.cpp
openfortigui/mainwindow.cpp
openfortigui/openfortivpn/src/config.c
openfortigui/openfortivpn/src/hdlc.c
openfortigui/openfortivpn/src/http.c
openfortigui/openfortivpn/src/io.c
openfortigui/openfortivpn/src/ipv4.c
openfortigui/openfortivpn/src/log.c
openfortigui/openfortivpn/src/tunnel.c
openfortigui/openfortivpn/src/userinput.c
openfortigui/openfortivpn/src/xml.c
openfortigui/vpnmanager.cpp
openfortigui/ticonfmain.cpp
openfortigui/vpnapi.cpp
openfortigui/proc/vpnprocess.cpp
openfortigui/proc/vpnbarracuda.cpp
openfortigui/vpnprofile.cpp
openfortigui/vpnprofileeditor.cpp
openfortigui/proc/vpnworker.cpp
openfortigui/vpngroup.cpp
openfortigui/vpngroupeditor.cpp
openfortigui/vpnsetting.cpp
openfortigui/vpnlogin.cpp
openfortigui/vpnhelper.cpp
openfortigui/vpnlogger.cpp
openfortigui/vpnotplogin.cpp
openfortigui/setupwizard.cpp
openfortigui/vpnchangelog.cpp
)

# Add resource file if needed
list (APPEND FORTI_SOURCE_FILES openfortigui/res.qrc)


#Add UI Files if needed
list (APPEND FORTI_SOURCE_FILES
    openfortigui/mainwindow.ui
    openfortigui/vpnprofileeditor.ui
    openfortigui/vpngroupeditor.ui
    openfortigui/vpnsetting.ui
    openfortigui/vpnlogin.ui
    openfortigui/vpnotplogin.ui
    openfortigui/setupwizard.ui
    openfortigui/vpnchangelog.ui
)

# Find includes in corresponding build directories
set(CMAKE_INCLUDE_CURRENT_DIR ON)


add_executable(${PROJECT_NAME} ${FORTI_SOURCE_FILES})

target_link_libraries (${PROJECT_NAME} ${FORTI_LIBS} ${EXTRA_LIBRARIES})
