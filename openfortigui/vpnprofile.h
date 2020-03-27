/*
 *  Copyright (C) 2018 Rene Hadler
 *
 *  This program is free software: you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation, either version 3 of the License, or
 *  (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

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
    quint16 gateway_port;
    QString username;
    QString password;
    QString otp;
    QString realm;

    bool set_routes;
    bool set_dns;
    bool half_internet_routers;

    QString ca_file;
    QString user_cert;
    QString user_key;
    QString trusted_cert;
    bool verify_cert;
    bool insecure_ssl;
    bool debug;
    bool autostart;
    bool alwaysAskOtp;

    bool pppd_no_peerdns;
    QString pppd_log_file;
    QString pppd_plugin_file;
    QString pppd_ifname;
    QString pppd_ipparam;
    QString pppd_call;

    QString readPassword();
};

#endif // VPNPROFILE_H
