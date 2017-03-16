#ifndef VPNAPI_H
#define VPNAPI_H

#include <QString>
#include <QDataStream>

enum vpnApiAction
{
    VPN_HELLO = 0,
    VPN_STOP
};

class vpnApi
{
public:
    vpnApi();

    QString objName;
    int action;
};

QDataStream &operator<<(QDataStream &ds, const vpnApi &obj);
QDataStream &operator>>(QDataStream &ds, vpnApi &obj) ;

#endif // VPNAPI_H
