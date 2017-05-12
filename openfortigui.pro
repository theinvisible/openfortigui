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

TRANSLATIONS = lang/openfortigui_de.ts

SOURCES += main.cpp\
        mainwindow.cpp \
    openfortivpn/src/config.c \
    openfortivpn/src/hdlc.c \
    openfortivpn_local/src/http.c \
    openfortivpn/src/io.c \
    openfortivpn/src/ipv4.c \
    openfortivpn/src/log.c \
    openfortivpn/src/tunnel.c \
    openfortivpn/src/userinput.c \
    openfortivpn/src/xml.c \
    vpnmanager.cpp \
    ticonfmain.cpp \
    vpnapi.cpp \
    proc/vpnprocess.cpp \
    vpnprofile.cpp \
    vpnprofileeditor.cpp \
    qtinyaes/QTinyAes/qtinyaes.cpp \
    qtinyaes/tiny-AES128-C/aes.c \
    proc/vpnworker.cpp \
    vpngroup.cpp \
    vpngroupeditor.cpp \
    vpnsetting.cpp \
    vpnlogin.cpp

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
    qtinyaes/tiny-AES128-C/aes.h \
    qtinyaes/QTinyAes/qtinyaes.h \
    proc/vpnworker.h \
    vpngroup.h \
    vpngroupeditor.h \
    vpnsetting.h \
    vpnlogin.h

FORMS    += mainwindow.ui \
    vpnprofileeditor.ui \
    vpngroupeditor.ui \
    vpnsetting.ui \
    vpnlogin.ui

RESOURCES += \
    res.qrc

unix:!macx:!symbian: LIBS += -lcrypto -lpthread -lssl -lutil
