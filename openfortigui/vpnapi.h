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
        ACTION_STOREPASS_SUBMIT,
        ACTION_VPN_START,
        ACTION_VPN_STOP,
        ACTION_PING,
        ACTION_PONG,
        ACTION_VPNGROUP_START,
        ACTION_VPNGROUP_STOP,
        ACTION_SHOW_MAIN
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
