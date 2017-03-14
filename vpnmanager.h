#ifndef VPNMANAGER_H
#define VPNMANAGER_H

#include <QObject>
#include <QLocalSocket>
#include <QLocalServer>

class vpnManager : public QObject
{
    Q_OBJECT
public:

    explicit vpnManager(QObject *parent = 0);

    void startvpn1();

private:

    QLocalServer *server;

signals:

public slots:
    void onServerConnected();
};

#endif // VPNMANAGER_H
