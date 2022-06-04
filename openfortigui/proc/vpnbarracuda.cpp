#include "vpnbarracuda.h"

vpnBarracuda::vpnBarracuda(QObject *parent)
    : QObject{parent}
{

}

void vpnBarracuda::start(const QString &vpnname, vpnClientConnection *conn)
{
    this->vpnname = vpnname;
    client_con = conn;
}
