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

#ifndef CONFIG_H
#define CONFIG_H

namespace openfortigui_config
{
    inline constexpr const char *name = "openfortiGUI";
    inline constexpr const char *version = "0.9.10";
    inline constexpr const char *file_main = "~/.openfortigui/main.conf";
    inline constexpr const char *file_gw_cert_cache = "~/.openfortigui/gw_cert.cache";
    inline constexpr const char *initd_default = "/etc/init.d/openfortigui";
    inline constexpr const char *api_vpn_base_name = "openfortiguivpn";
    inline constexpr const char *vpnprofiles_local = "~/.openfortigui/vpnprofiles";
    inline constexpr const char *vpnprofiles_global = "/etc/openfortigui/vpnprofiles";
    inline constexpr const char *vpngroups_local = "~/.openfortigui/vpngroups";

    inline constexpr const char *password_manager_namespace = "openfortigui";
    inline constexpr const char *aeskey = "yowp2IwTTRodgdWp";
    inline constexpr const char *aesiv = "VoUT5n5ToogkmQU3";

    inline constexpr const char *validatorName = "^[a-zA-ZäöüÄÖÜ0-9 \\-_]{3,}$";

    inline constexpr int changelogRev = 23;
}

#endif // CONFIG_H
