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

#ifndef CONFIH_H
#define CONFIH_H

namespace openfortigui_config
{
    static const char __attribute__ ((unused)) *name = "openfortiGUI";
    static const char __attribute__ ((unused)) *version = "0.9.4-dev";
    static const char __attribute__ ((unused)) *file_main = "~/.openfortigui/main.conf";
    static const char __attribute__ ((unused)) *initd_default = "/etc/init.d/openfortigui";
    static const char __attribute__ ((unused)) *api_vpn_base_name = "openfortiguivpn";
    static const char __attribute__ ((unused)) *vpnprofiles_local = "~/.openfortigui/vpnprofiles";
    static const char __attribute__ ((unused)) *vpnprofiles_global = "/etc/openfortigui/vpnprofiles";
    static const char __attribute__ ((unused)) *vpngroups_local = "~/.openfortigui/vpngroups";

    static const char __attribute__ ((unused)) *password_manager_namespace = "openfortigui";
    static const char __attribute__ ((unused)) *aeskey = "yowp2IwTTRodgdWp";
    static const char __attribute__ ((unused)) *aesiv = "VoUT5n5ToogkmQU3";

    static const char __attribute__ ((unused)) *validatorName = "^[a-zA-ZäöüÄÖÜ0-9 \\-_]{3,}$";

    static const int __attribute__ ((unused)) changelogRev = 16;
}

#endif // CONFIH_H
