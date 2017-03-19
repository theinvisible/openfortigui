#ifndef VPNMANAGER_H
#define VPNMANAGER_H

#include "vpnapi.h"

#include <QObject>
#include <QLocalSocket>
#include <QLocalServer>

class vpnClientConnection : public QObject
{
    Q_OBJECT
public:

    explicit vpnClientConnection(const QString &n, QObject *parent = 0);

    enum connectionStatus
    {
        STATUS_DISCONNECTED = 0,
        STATUS_CONNECTING,
        STATUS_CONNECTED
    };

    connectionStatus status;

    void setSocket(QLocalSocket *sock);
    void sendCMD(const vpnApi &cmd);

private:
    QString name;
    QLocalSocket *socket;

signals:
    void VPNStatusChanged(QString vpnname, vpnClientConnection::connectionStatus status);

public slots:
    void onClientReadyRead();
    void onClientDisconnected();
};

class vpnManager : public QObject
{
    Q_OBJECT
public:

    explicit vpnManager(QObject *parent = 0);
    ~vpnManager();

    void startVPN(const QString &name);
    void stopVPN(const QString &name);

    vpnClientConnection *getClientConnection(const QString &name);

private:

    QLocalServer *server;
    QMap<QString, vpnClientConnection*> connections;

signals:
    void VPNStatusChanged(QString vpnname, vpnClientConnection::connectionStatus status);

public slots:
    void onClientConnected();
    void onClientVPNStatusChanged(QString vpnname, vpnClientConnection::connectionStatus status);
};

#endif // VPNMANAGER_H
