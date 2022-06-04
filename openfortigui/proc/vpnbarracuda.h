#ifndef VPNBARRACUDA_H
#define VPNBARRACUDA_H

#include <QObject>
#include <QDataStream>
#include <QProcess>

#include "../vpnmanager.h"

class vpnBarracuda : public QObject
{
    Q_OBJECT
public:
    explicit vpnBarracuda(QObject *parent = nullptr);
    void start(const QString &vpnname, vpnClientConnection *conn);

private:
    QString vpnname;
    vpnClientConnection *client_con;

signals:

};

#endif // VPNBARRACUDA_H
