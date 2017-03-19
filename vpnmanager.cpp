#include "vpnmanager.h"
#include "config.h"
#include "ticonfmain.h"

#include <QDataStream>
#include <QProcess>
#include <QThread>
#include <QCoreApplication>

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

void vpnManager::startVPN(const QString &name)
{
    if(connections.contains(name))
        return;

    QStringList arguments;
    arguments << QCoreApplication::applicationFilePath();
    arguments << "--start-vpn";
    arguments << "--vpn-name";
    arguments << name;
    arguments << "--main-config";
    arguments << QString("'%1'").arg(tiConfMain::main_config);

    QProcess *vpn = new QProcess(this);
    qInfo() << "Start vpn::" << name;
    vpn->start("sudo", arguments);

    vpnClientConnection *clientConn = new vpnClientConnection(name);
    connections[name] = clientConn;
}

void vpnManager::stopVPN(const QString &name)
{
    if(connections.contains(name))
    {
        vpnApi apiData;
        apiData.objName = name;
        apiData.action = vpnApi::ACTION_STOP;

        connections[name]->sendCMD(apiData);
        connections.remove(name);
    }
}

void vpnManager::onClientConnected()
{
    qInfo() << "vpnManager::onClientConnected()";
    if(server->hasPendingConnections())
    {
        QLocalSocket *client = server->nextPendingConnection();
        client->waitForReadyRead();
        vpnApi cmd;
        QDataStream in(client);
        in.setVersion(QDataStream::Qt_5_2);
        in >> cmd;
        qInfo() << "client api helo command::" << cmd.action << "::name::" << cmd.objName;
        client->flush();

        //vpnClientConnection *clientConn = new vpnClientConnection(cmd.objName, client);
        if(connections.contains(cmd.objName))
            connections[cmd.objName]->setSocket(client);
        else
            qInfo() << "no socket assigend";

        //client->disconnectFromServer();
    }
}

vpnClientConnection::vpnClientConnection(const QString &n, QObject *parent) : QObject(parent)
{
    name = n;
    status = STATUS_DISCONNECTED;
}

void vpnClientConnection::setSocket(QLocalSocket *sock)
{
    socket = sock;

    connect(socket, SIGNAL(disconnected()), this, SLOT(onClientDisconnected()));
    connect(socket, SIGNAL(readyRead()), this, SLOT(onClientReadyRead()));
}

void vpnClientConnection::sendCMD(const vpnApi &cmd)
{
    QByteArray block;
    QDataStream out(&block, QIODevice::WriteOnly);
    out.setVersion(QDataStream::Qt_5_2);
    out << cmd;

    if(!socket->isOpen())
    {
        qInfo() << "Socket ist nicht offen";
        return;
    }

    socket->write(block);
    socket->flush();
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
