#ifndef VPNMANAGER_H
#define VPNMANAGER_H

#include <QObject>
#include <QLocalSocket>
#include <QLocalServer>

class vpnClientConnection;

class vpnManager : public QObject
{
    Q_OBJECT
public:

    explicit vpnManager(QObject *parent = 0);

    void startvpn1();

private:

    QLocalServer *server;
    QMap<QString, vpnClientConnection*> connections;

signals:

public slots:
    void onClientConnected();
};

class vpnClientConnection : public QObject
{
    Q_OBJECT
public:

    explicit vpnClientConnection(const QString &n, QLocalSocket *sock, QObject *parent = 0);

private:
    QString name;
    QLocalSocket *socket;

signals:

public slots:
    void onClientReadyRead();
    void onClientDisconnected();
};

#endif // VPNMANAGER_H
