#ifndef VPNWORKER_H
#define VPNWORKER_H

#include <QObject>
#include "vpnprofile.h"

class vpnWorker : public QObject
{
    Q_OBJECT
public:
    explicit vpnWorker(QObject *parent = 0);

    void setConfig(vpnProfile c);

private:
    vpnProfile vpnConfig;

signals:

public slots:
    void process();
};

#endif // VPNWORKER_H
