#ifndef VPNMANAGER_H
#define VPNMANAGER_H

#include "vpnapi.h"

#include <QObject>
#include <QLocalSocket>
#include <QLocalServer>
#include <QStandardItem>
#include <QProcess>

#include <vpnlogger.h>

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
    QStandardItem *item_stats;
    QProcess *proc;

    void setSocket(QLocalSocket *sock);
    void sendCMD(const vpnApi &cmd);

private:
    QString name;
    QLocalSocket *socket;

signals:
    void VPNStatusChanged(QString vpnname, vpnClientConnection::connectionStatus status);
    void VPNCredRequest(QString vpnname);
    void VPNStatsUpdate(QString vpnname, vpnStats stats);

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
    void submitVPNCred(const QString &vpnname, const QString &username, const QString &password);

    void requestStats(const QString &vpnname);

private:

    QLocalServer *server;
    QMap<QString, vpnClientConnection*> connections;
    QThread *logger_thread;
    vpnLogger *logger;

signals:
    void VPNStatusChanged(QString vpnname, vpnClientConnection::connectionStatus status);
    void VPNCredRequest(QString vpnname);
    void VPNStatsUpdate(QString vpnname, vpnStats stats);

public slots:
    void onClientConnected();
    void onClientVPNStatusChanged(QString vpnname, vpnClientConnection::connectionStatus status);
    void onClientVPNCredRequest(QString vpnname);
    void onClientVPNStatsUpdate(QString vpnname, vpnStats stats);
};

#endif // VPNMANAGER_H
