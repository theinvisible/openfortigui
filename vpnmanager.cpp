#include "vpnmanager.h"
#include "config.h"
#include "vpnapi.h"

#include <QDataStream>
#include <QProcess>
#include <QThread>

vpnManager::vpnManager(QObject *parent) : QObject(parent)
{
    QLocalServer::removeServer(openfortigui_config::name);
    server = new QLocalServer(this);
    connect(server, SIGNAL(newConnection()), this, SLOT(onClientConnected()));
    if(!server->listen(openfortigui_config::name))
        qInfo() << "vpnManager::DiskMain() on apiServer->listen::" << server->errorString();
}

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

void vpnManager::onClientConnected()
{
    qInfo() << "vpnManager::onAPIConnected()";
    if(server->hasPendingConnections())
    {
        QLocalSocket *client = server->nextPendingConnection();
        client->waitForReadyRead();
        vpnApi cmd;
        QDataStream in(client);
        in.setVersion(QDataStream::Qt_5_2);
        in >> cmd;
        qInfo() << "client api command::" << cmd.action << "::name::" << cmd.objName;
        client->flush();

        vpnClientConnection *clientConn = new vpnClientConnection(cmd.objName, client);
        connections[cmd.objName] = clientConn;

        //client->disconnectFromServer();
    }
}

vpnClientConnection::vpnClientConnection(const QString &n, QLocalSocket *sock, QObject *parent) : QObject(parent)
{
    name = n;
    socket = sock;

    connect(socket, SIGNAL(disconnected()), this, SLOT(onClientDisconnected()));
    connect(socket, SIGNAL(readyRead()), this, SLOT(onClientReadyRead()));

    /*
    QThread::sleep(5);

    QByteArray block;
    QDataStream out(&block, QIODevice::WriteOnly);
    out.setVersion(QDataStream::Qt_5_2);
    vpnApi apiData;
    apiData.objName = name;
    apiData.action = VPN_STOP;
    out << apiData;

    socket->write(block);
    socket->flush();
    */
}

void vpnClientConnection::onClientReadyRead()
{
    vpnApi cmd;
    QDataStream in(socket);
    in.setVersion(QDataStream::Qt_5_2);
    in >> cmd;
    qInfo() << "client api command2::" << cmd.action << "::name::" << cmd.objName;
    socket->flush();
}

void vpnClientConnection::onClientDisconnected()
{
    qInfo() << "client disconnected::" << name;
    socket->deleteLater();
}
