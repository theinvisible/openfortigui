#ifndef VPNPROCESS_H
#define VPNPROCESS_H

#include <QObject>
#include <QLocalSocket>
#include <QThread>

#include "config.h"
#include "vpnworker.h"

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
    QTimer *observer;
    struct tunnel last_tunnel;
    bool init_last_tunnel;

    void closeProcess();
    void startVPN();
    void sendCMD(const vpnApi &cmd);

signals:

public slots:
    void onServerReadyRead();
    void onServerDisconnected();
    void onVPNStatusChanged(vpnClientConnection::connectionStatus status);

private slots:
    void onObserverUpdate();
};

#endif // VPNPROCESS_H
