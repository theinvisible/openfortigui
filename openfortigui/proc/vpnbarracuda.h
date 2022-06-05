#ifndef VPNBARRACUDA_H
#define VPNBARRACUDA_H

#include <QObject>
#include <QDataStream>
#include <QProcess>

#include "../vpnmanager.h"

class vpnBarracuda : public QObject
{
    Q_OBJECT
public:
    explicit vpnBarracuda(QObject *parent = nullptr);
    void start(const QString &vpnname, vpnClientConnection *conn);
    void stop();

private:
    QString vpnname;
    vpnClientConnection *client_con;
    vpnProfile vpn_profile;

    static QString conf_template;

private slots:
    void statusCheck();

signals:
    void VPNStatusChanged(QString vpnname, vpnClientConnection::connectionStatus status);
    void VPNCredRequest(QString vpnname);
    void VPNStatsUpdate(QString vpnname, vpnStats stats);
    void VPNMessage(QString vpnname, vpnMsg msg);

signals:

};

#endif // VPNBARRACUDA_H
