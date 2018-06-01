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
    pppd_no_peerdns = false;
    half_internet_routers = false;

    ca_file = "";
    user_cert = "";
    user_key = "";
    verify_cert = false;
    insecure_ssl = false;
    autostart = false;
}
