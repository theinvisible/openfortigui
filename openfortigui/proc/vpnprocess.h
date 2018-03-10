#ifndef VPNPROCESS_H
#define VPNPROCESS_H

#include <QObject>
#include <QLocalSocket>
#include <QThread>

#include "config.h"
#include "vpnworker.h"
#include "vpnapi.h"

class vpnProcess : public QObject
{
    Q_OBJECT
public:
    explicit vpnProcess(QObject *parent = 0);
    void run(const QString &vpnname);

private:
    QString name;
    QLocalSocket *apiServer;
    QThread* thread_vpn;
    vpnWorker *thread_worker;
    QTimer *observer, *observerStats;
    struct tunnel last_tunnel;
    bool init_last_tunnel;

    bool cred_received, passstore_received;
    struct struct_cred_data
    {
        QString username;
        QString password;
    };
    struct_cred_data cred_data;

    vpnStats stats;

    void closeProcess();
    void startVPN();
    void sendCMD(const vpnApi &cmd);
    void updateStats();

    void requestCred();
    void requestPassStore();
    void submitStats();

signals:

public slots:
    void onServerReadyRead();
    void onServerDisconnected();
    void onVPNStatusChanged(vpnClientConnection::connectionStatus status);

private slots:
    void onObserverUpdate();
    void onStatsUpdate();
};

#endif // VPNPROCESS_H
