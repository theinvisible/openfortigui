#include "vpnapi.h"

#include <QVariant>

vpnApi::vpnApi()
{

}


QDataStream &operator<<(QDataStream &ds, const vpnApi &obj)
{
    ds << obj.action;
    ds << obj.objName;

    return ds;
}

QDataStream &operator>>(QDataStream &ds, vpnApi &obj)
{
    ds >> obj.action;
    ds >> obj.objName;

    return ds;
}
