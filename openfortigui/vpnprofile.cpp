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

#include <memory>
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
    sni = "";
    cookie = "";

    set_routes = true;
    set_dns = false;
    half_internet_routers = false;
    persistent = false;

    ca_file = "";
    user_cert = "";
    user_key = "";
    verify_cert = false;
    autostart = false;
    always_ask_otp = false;
    otp_prompt = "";
    otp_delay = 0;

    pppd_no_peerdns = false;
    pppd_accept_remote = false;
    pppd_log_file = "";
    pppd_plugin_file = "";
    pppd_ifname = "";
    pppd_ipparam = "";
    pppd_call = "";

    insecure_ssl = false;
    seclevel1 = false;
    min_tls = "default";

    trust_all_gw_certs = false;
    device_type = Device::Device_Fortigate;
}

QString vpnProfile::readPassword()
{
    return readSecret("password");
}

QString vpnProfile::readCookie()
{
    return readSecret("cookie");
}

/*
 * Read a single encrypted value straight out of the profile file. Used where the
 * profile list was loaded without secrets (setReadProfilePasswords(false)) but
 * one of them is needed after all.
 */
QString vpnProfile::readSecret(const QString &key)
{
    auto main_settings = std::make_unique<tiConfMain>();
    QString retPass = "";

    QMap<vpnProfile::Origin, QString> profileDirs;
    profileDirs[vpnProfile::Origin_LOCAL] = tiConfMain::formatPath(main_settings->getValue("paths/localvpnprofiles").toString());
    profileDirs[vpnProfile::Origin_GLOBAL] = tiConfMain::formatPath(main_settings->getValue("paths/globalvpnprofiles").toString());

    QString profileDir = profileDirs[origin_location];

    QString aeskey, aesiv;
    if(!vpnHelper::mainAesKeyIv(*main_settings, aeskey, aesiv))
    {
        qWarning() << "vpnProfile::readSecret() -> AES key/IV unavailable, returning no secret for" << name;
        return "";
    }

    auto f = std::make_unique<QSettings>(profileDir + QDir::separator() + name + ".conf", QSettings::IniFormat);
    f->beginGroup("vpn");
    retPass = vpnHelper::Qaes128_decrypt(f->value(key).toString(), aeskey, aesiv);
    f->endGroup();

    return retPass;
}
