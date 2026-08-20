cmake_minimum_required(VERSION 3.16)
project(krunner_openfortigui)

set(QT_MIN_VERSION 6.4.0)
set(KF_MIN_VERSION 6.0.0)

find_package(ECM ${KF_MIN_VERSION} REQUIRED NO_MODULE)
set(CMAKE_MODULE_PATH ${ECM_MODULE_PATH})

include(KDEInstallDirs)
include(KDECMakeSettings)
# Deliberately no KDECompilerSettings: it defines QT_NO_CAST_FROM_ASCII, and the
# application sources compiled in below rely on implicit QString conversions the
# way the main build allows them.

# Gui is needed for QValidator, which ticonfmain.cpp includes -- not for widgets.
find_package(Qt6 ${QT_MIN_VERSION} REQUIRED CONFIG COMPONENTS Core Gui Network)
find_package(KF6 ${KF_MIN_VERSION} REQUIRED COMPONENTS Runner CoreAddons)
# KF6::ConfigCore is in the link interface of KF6::Runner, so its package has to
# be found as well. Not as a KF6 component: ECM's FindKF6 cannot resolve a
# component literally named "Config".
find_package(KF6Config ${KF_MIN_VERSION} REQUIRED)
find_package(OpenSSL REQUIRED)
find_package(Qt6Keychain REQUIRED)

# The configuration layer is not copied into this directory any more: the plugin
# compiles the application's own files, so both resolve profile paths the same way.
# Copies of ticonfmain.cpp and vpnhelper.cpp used to live here and had drifted --
# theirs was still the formatPath() that replaced every "~" in a path (issue #157).
set(OFGUI_DIR ${CMAKE_CURRENT_SOURCE_DIR}/../openfortigui)

set(${PROJECT_NAME}_SRCS
    krunner_openfortigui.cpp
    ${OFGUI_DIR}/ticonfmain.cpp
    ${OFGUI_DIR}/vpnprofile.cpp
    ${OFGUI_DIR}/vpngroup.cpp
    ${OFGUI_DIR}/vpnhelper.cpp
    ${OFGUI_DIR}/vpnapi.cpp
)

kcoreaddons_add_plugin(${PROJECT_NAME}
    SOURCES ${${PROJECT_NAME}_SRCS}
    INSTALL_NAMESPACE "kf6/krunner")

target_include_directories(${PROJECT_NAME} PRIVATE ${OFGUI_DIR})

# The application sources include config.h, which requires OPENFORTIGUI_VERSION.
# Its single source of truth is the project(... VERSION ...) line in the
# application's CMakeLists.txt -- parse it from there instead of repeating it.
file(STRINGS ${OFGUI_DIR}/CMakeLists.txt _ofgui_project REGEX "^project\\(openfortigui VERSION ")
string(REGEX MATCH "[0-9]+\\.[0-9]+[0-9.]*" OPENFORTIGUI_VERSION "${_ofgui_project}")
if(NOT OPENFORTIGUI_VERSION)
    message(FATAL_ERROR "could not parse VERSION from ${OFGUI_DIR}/CMakeLists.txt")
endif()
target_compile_definitions(${PROJECT_NAME} PRIVATE OPENFORTIGUI_VERSION="${OPENFORTIGUI_VERSION}")

target_link_libraries(${PROJECT_NAME}
    KF6::Runner
    Qt6::Core
    Qt6::Gui
    Qt6::Network
    OpenSSL::SSL
    OpenSSL::Crypto
    qt6keychain)
