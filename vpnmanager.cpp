#include "vpnmanager.h"

#include "config.h"

#include <QDataStream>
#include <QProcess>

void vpnManager::startvpn1()
{
    QStringList arguments;
    arguments << "./openfortigui";
    arguments << "--start-vpn";
    arguments << "--vpn-name";
    arguments << "vpn1";

    QProcess *vpn1 = new QProcess(this);
    qInfo() << "Start vpn2";
    vpn1->start("sudo", arguments);
    vpn1->waitForStarted();
    vpn1->waitForReadyRead();
}

vpnManager::vpnManager(QObject *parent) : QObject(parent)
{
    QLocalServer::removeServer(openfortigui_config::name);
    server = new QLocalServer(this);
    connect(server, SIGNAL(newConnection()), this, SLOT(onAPIConnected()));
    if(!server->listen(openfortigui_config::name))
        qInfo() << "vpnManager::DiskMain() on apiServer->listen::" << server->errorString();
}

void vpnManager::onServerConnected()
{
    qInfo() << "vpnManager::onAPIConnected()";
    if(server->hasPendingConnections())
    {
        QLocalSocket *client = server->nextPendingConnection();
        connect(client, SIGNAL(disconnected()), client, SLOT(deleteLater()));
        client->waitForReadyRead();
        QHash<QString, QString> apiData;
        QDataStream in(client);
        in.setVersion(QDataStream::Qt_5_2);
        in >> apiData;
        qInfo() << "client api command::" << apiData;
        client->flush();
        client->disconnectFromServer();
    }
}
