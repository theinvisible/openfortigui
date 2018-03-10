#ifndef VPNAPI_H
#define VPNAPI_H

#include <QString>
#include <QDataStream>

class vpnApi
{
public:
    vpnApi();

    enum vpnApiAction
    {
        ACTION_HELLO = 0,
        ACTION_STOP,
        ACTION_VPN_UPDATE_STATUS,
        ACTION_CRED_REQUEST,
        ACTION_CRED_SUBMIT,
        ACTION_VPNSTATS_REQUEST,
        ACTION_VPNSTATS_SUBMIT,
        ACTION_STOREPASS_REQUEST,
        ACTION_STOREPASS_SUBMIT
    };

    QString objName;
    int action;
    QByteArray data;
};

class vpnStats
{
public:
    qint64 bytes_read, bytes_written, vpn_start;
};

QDataStream &operator<<(QDataStream &ds, const vpnApi &obj);
QDataStream &operator>>(QDataStream &ds, vpnApi &obj) ;

#endif // VPNAPI_H
