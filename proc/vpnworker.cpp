#include "vpnworker.h"

extern "C"  {
#include "openfortivpn/src/config.h"
#include "openfortivpn/src/log.h"
#include "openfortivpn/src/tunnel.h"
}

#include <QDebug>

vpnWorker::vpnWorker(QObject *parent) : QObject(parent)
{

}

void vpnWorker::setConfig(vpnProfile c)
{
    config = c;
}

void vpnWorker::process()
{
    struct vpn_config cfg;
    memset(&cfg, 0, sizeof (cfg));
    strncpy(cfg.gateway_host, config.gateway_host.toStdString().c_str(), FIELD_SIZE);
    cfg.gateway_host[FIELD_SIZE] = '\0';
    cfg.gateway_port = config.gateway_port;
    strncpy(cfg.username, config.username.toStdString().c_str(), FIELD_SIZE);
    cfg.username[FIELD_SIZE] = '\0';
    strncpy(cfg.password, config.password.toStdString().c_str(), FIELD_SIZE);
    cfg.password[FIELD_SIZE] = '\0';
    cfg.set_routes = (config.set_routes) ? 1 : 0;
    run_tunnel(&cfg);
}
