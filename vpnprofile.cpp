#include "vpnprofile.h"

vpnProfile::vpnProfile()
{
    name = "";

    gateway_host = "";
    gateway_port = 0;
    username = "";
    password = "";
    otp = "";

    set_routes = true;
    set_dns = false;
    pppd_use_peerdns = false;

    ca_file = "";
    user_cert = "";
    user_key = "";
    verify_cert = false;
    insecure_ssl = false;
}
