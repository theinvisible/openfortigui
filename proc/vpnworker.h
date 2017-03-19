#ifndef VPNWORKER_H
#define VPNWORKER_H

#include <QObject>
#include "vpnprofile.h"
#include "vpnmanager.h"

extern "C"  {
#include "openfortivpn/src/config.h"
#include "openfortivpn/src/log.h"
#include "openfortivpn/src/tunnel.h"
}

class vpnWorker : public QObject
{
    Q_OBJECT
public:
    explicit vpnWorker(QObject *parent = 0);

    void setConfig(vpnProfile c);

private:
    vpnProfile vpnConfig;

    void updateStatus(vpnClientConnection::connectionStatus status);

signals:
    void statusChanged(vpnClientConnection::connectionStatus status);

public slots:
    void process();
};

#endif // VPNWORKER_H
