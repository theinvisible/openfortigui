#-------------------------------------------------
#
# Project created by QtCreator 2017-03-05T12:10:50
#
#-------------------------------------------------

QT       += core gui network

greaterThan(QT_MAJOR_VERSION, 4): QT += widgets

TARGET = openfortigui
TEMPLATE = app


SOURCES += main.cpp\
        mainwindow.cpp \
    openfortivpn/src/config.c \
    openfortivpn/src/hdlc.c \
    openfortivpn/src/http.c \
    openfortivpn/src/io.c \
    openfortivpn/src/ipv4.c \
    openfortivpn/src/log.c \
    openfortivpn/src/tunnel.c \
    openfortivpn/src/userinput.c \
    openfortivpn/src/xml.c \
    vpnworker.cpp \
    vpnworker2.cpp \
    vpnmanager.cpp \
    ticonfmain.cpp \
    vpnapi.cpp \
    proc/vpnprocess.cpp

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
    vpnworker.h \
    vpnworker2.h \
    config.h \
    vpnmanager.h \
    ticonfmain.h \
    vpnapi.h \
    proc/vpnprocess.h

FORMS    += mainwindow.ui

RESOURCES += \
    res.qrc

unix:!macx:!symbian: LIBS += -lcrypto -lpthread -lssl -lutil
