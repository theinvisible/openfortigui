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

#include "vpnprofile.h"

#include "ticonfmain.h"
#include "vpnhelper.h"

#include <QDir>
#include <QDebug>

vpnProfile::vpnProfile()
{
    name = "";

    gateway_host = "";
    gateway_port = 0;
    username = "";
    password = "";
    otp = "";
    realm = "";

    set_routes = true;
    set_dns = false;
    half_internet_routers = false;

    ca_file = "";
    user_cert = "";
    user_key = "";
    verify_cert = false;
    insecure_ssl = false;
    autostart = false;
    always_ask_otp = false;
    otp_prompt = "";
    otp_delay = 0;

    pppd_no_peerdns = false;
    pppd_log_file = "";
    pppd_plugin_file = "";
    pppd_ifname = "";
    pppd_ipparam = "";
    pppd_call = "";
}

QString vpnProfile::readPassword()
{
    tiConfMain *main_settings = new tiConfMain();
    QString retPass = "";

    QMap<vpnProfile::Origin, QString> profileDirs;
    profileDirs[vpnProfile::Origin_LOCAL] = tiConfMain::formatPath(main_settings->getValue("paths/localvpnprofiles").toString());
    profileDirs[vpnProfile::Origin_GLOBAL] = tiConfMain::formatPath(main_settings->getValue("paths/globalvpnprofiles").toString());

    QString profileDir = profileDirs[origin_location];

    QString aeskey, aesiv;
    if(main_settings->getValue("main/use_system_password_store").toBool()) {
        aeskey = vpnHelper::systemPasswordStoreRead("aeskey").data;
        aesiv = vpnHelper::systemPasswordStoreRead("aesiv").data;
    } else {
        aeskey = main_settings->getValue("main/aeskey").toString();
        aesiv = main_settings->getValue("main/aesiv").toString();
    }

    QSettings *f = new QSettings(profileDir + QDir::separator() + name + ".conf", QSettings::IniFormat);
    f->beginGroup("vpn");
    retPass = vpnHelper::Qaes128_decrypt(f->value("password").toString(), aeskey, aesiv);
    f->endGroup();
    delete f;

    return retPass;
}
