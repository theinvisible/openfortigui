#ifndef VPNPROFILE_H
#define VPNPROFILE_H

#include <QObject>

class vpnProfile
{

public:
    explicit vpnProfile();

    enum Origin
    {
        Origin_LOCAL = 0,
        Origin_GLOBAL,
        Origin_BOTH
    };

    QString name;
    Origin origin_location;

    QString gateway_host;
    qint16 gateway_port;
    QString username;
    QString password;
    QString otp;
    QString realm;

    bool set_routes;
    bool set_dns;
    bool pppd_no_peerdns;

    QString ca_file;
    QString user_cert;
    QString user_key;
    QString trusted_cert;
    bool verify_cert;
    bool insecure_ssl;
    bool debug;
    bool autostart;
};

#endif // VPNPROFILE_H
