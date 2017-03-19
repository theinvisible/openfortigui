#ifndef VPNMANAGER_H
#define VPNMANAGER_H

#include "vpnapi.h"

#include <QObject>
#include <QLocalSocket>
#include <QLocalServer>

class vpnClientConnection;

class vpnManager : public QObject
{
    Q_OBJECT
public:

    explicit vpnManager(QObject *parent = 0);

    void startvpn1();
    void startVPN(const QString &name);
    void stopVPN(const QString &name);

private:

    QLocalServer *server;
    QMap<QString, vpnClientConnection*> connections;

signals:

public slots:
    void onClientConnected();
};

class vpnClientConnection : public QObject
{
    Q_OBJECT
public:

    explicit vpnClientConnection(const QString &n, QObject *parent = 0);

    enum connectionStatus
    {
        STATUS_DISCONNECTED = 0,
        STATUS_CONNECTED
    };

    connectionStatus status;

    void setSocket(QLocalSocket *sock);
    void sendCMD(const vpnApi &cmd);

private:
    QString name;
    QLocalSocket *socket;

signals:

public slots:
    void onClientReadyRead();
    void onClientDisconnected();
};

#endif // VPNMANAGER_H
