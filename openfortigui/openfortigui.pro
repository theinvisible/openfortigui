#-------------------------------------------------
#
# Project created by QtCreator 2017-03-05T12:10:50
#
#-------------------------------------------------

QT       += core gui network

greaterThan(QT_MAJOR_VERSION, 4): QT += widgets

TARGET = openfortigui
TEMPLATE = app

CONFIG += c++11

TRANSLATIONS = lang/openfortigui_de.ts lang/openfortigui_ca.ts lang/openfortigui_ja.ts

SOURCES += main.cpp\
        mainwindow.cpp \
    openfortivpn/src/config.c \
    openfortivpn/src/hdlc.c \
    openfortivpn_local/src/http.c \
    openfortivpn/src/io.c \
    openfortivpn/src/ipv4.c \
    openfortivpn/src/log.c \
    openfortivpn/src/tunnel.c \
    openfortivpn_local/src/userinput.c \
    openfortivpn/src/xml.c \
    vpnmanager.cpp \
    ticonfmain.cpp \
    vpnapi.cpp \
    proc/vpnprocess.cpp \
    vpnprofile.cpp \
    vpnprofileeditor.cpp \
    proc/vpnworker.cpp \
    vpngroup.cpp \
    vpngroupeditor.cpp \
    vpnsetting.cpp \
    vpnlogin.cpp \
    vpnhelper.cpp \
    vpnlogger.cpp \
    vpnotplogin.cpp \
    setupwizard.cpp \
    vpnchangelog.cpp

HEADERS  += mainwindow.h \
    openfortivpn/src/config.h \
    openfortivpn/src/hdlc.h \
    openfortivpn/src/http.h \
    openfortivpn/src/io.h \
    openfortivpn/src/ipv4.h \
    openfortivpn/src/log.h \
    openfortivpn/src/ssl.h \
    openfortivpn/src/tunnel.h \
    openfortivpn/src/userinput.h \
    openfortivpn/src/xml.h \
    config.h \
    vpnmanager.h \
    ticonfmain.h \
    vpnapi.h \
    proc/vpnprocess.h \
    vpnprofile.h \
    vpnprofileeditor.h \
    proc/vpnworker.h \
    vpngroup.h \
    vpngroupeditor.h \
    vpnsetting.h \
    vpnlogin.h \
    vpnhelper.h \
    vpnlogger.h \
    vpnotplogin.h \
    setupwizard.h \
    vpnchangelog.h

FORMS    += mainwindow.ui \
    vpnprofileeditor.ui \
    vpngroupeditor.ui \
    vpnsetting.ui \
    vpnlogin.ui \
    vpnotplogin.ui \
    setupwizard.ui \
    vpnchangelog.ui

RESOURCES += \
    res.qrc

QMAKE_CFLAGS += $$(CPPFLAGS)
QMAKE_LFLAGS += $$(LDFLAGS)

unix:!symbian: LIBS += -lcrypto -lpthread -lssl -lutil -lqt5keychain
