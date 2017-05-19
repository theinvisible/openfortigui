#include "vpnmanager.h"
#include "config.h"
#include "ticonfmain.h"

#include <QDataStream>
#include <QProcess>
#include <QThread>
#include <QCoreApplication>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonArray>
#include <QMessageBox>

vpnManager::vpnManager(QObject *parent) : QObject(parent)
{
    QLocalServer::removeServer(openfortigui_config::name);
    server = new QLocalServer(this);
    connect(server, SIGNAL(newConnection()), this, SLOT(onClientConnected()));
    if(!server->listen(openfortigui_config::name))
        qInfo() << "vpnManager::DiskMain() on apiServer->listen::" << server->errorString();
}

vpnManager::~vpnManager()
{
    QMapIterator<QString, vpnClientConnection*> i(connections);
    while(i.hasNext())
    {
        i.next();

        if(i.value()->status != vpnClientConnection::STATUS_DISCONNECTED)
        {
            stopVPN(i.key());
        }
    }
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
    // Close read channel to avoid memory leak
    // TODO: Process output later on
    vpn->closeReadChannel(QProcess::StandardOutput);
    vpn->closeReadChannel(QProcess::StandardError);

    vpnClientConnection *clientConn = new vpnClientConnection(name);
    connect(clientConn, SIGNAL(VPNStatusChanged(QString,vpnClientConnection::connectionStatus)), this, SLOT(onClientVPNStatusChanged(QString,vpnClientConnection::connectionStatus)));
    connect(clientConn, SIGNAL(VPNCredRequest(QString)), this, SLOT(onClientVPNCredRequest(QString)), Qt::QueuedConnection);
    connect(clientConn, SIGNAL(VPNStatsUpdate(QString,vpnStats)), this, SLOT(onClientVPNStatsUpdate(QString,vpnStats)), Qt::QueuedConnection);
    connections[name] = clientConn;
}

void vpnManager::stopVPN(const QString &name)
{
    if(connections.contains(name))
    {
        vpnApi apiData;
        apiData.objName = name;
        apiData.action = vpnApi::ACTION_STOP;

        qInfo() << "vpnManager::stopVPN::" << apiData.objName << "::" << apiData.action;

        connections[name]->sendCMD(apiData);
        connections.remove(name);
    }
}

vpnClientConnection *vpnManager::getClientConnection(const QString &name)
{
    vpnClientConnection *vpn = 0;
    if(connections.contains(name))
        vpn = connections[name];

    return vpn;
}

void vpnManager::submitVPNCred(const QString &vpnname, const QString &username, const QString &password)
{
    if(connections.contains(vpnname))
    {
        QJsonDocument json;
        QJsonObject jsTop;
        vpnClientConnection *vpn = connections[vpnname];
        vpnApi data;
        data.action = vpnApi::ACTION_CRED_SUBMIT;
        data.objName = vpnname;

        jsTop["username"] = username;
        jsTop["password"] = password;

        json.setObject(jsTop);
        data.data = json.toJson();

        vpn->sendCMD(data);
    }
}

void vpnManager::requestStats(const QString &vpnname)
{
    qInfo() << "vpnManager::requestStats";

    if(connections.contains(vpnname))
    {
        QJsonDocument json;
        QJsonObject jsTop;
        vpnClientConnection *vpn = connections[vpnname];
        vpnApi data;
        data.action = vpnApi::ACTION_VPNSTATS_REQUEST;
        data.objName = vpnname;

        json.setObject(jsTop);
        data.data = json.toJson();

        vpn->sendCMD(data);
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

void vpnManager::onClientVPNStatusChanged(QString vpnname, vpnClientConnection::connectionStatus status)
{
    qInfo() << "vpnManager::onClientVPNStatusChanged()" << vpnname << "status" << status;

    if(status == vpnClientConnection::STATUS_DISCONNECTED)
        connections.remove(vpnname);

    emit VPNStatusChanged(vpnname, status);
}

void vpnManager::onClientVPNCredRequest(QString vpnname)
{
    emit VPNCredRequest(vpnname);
}

void vpnManager::onClientVPNStatsUpdate(QString vpnname, vpnStats stats)
{
    emit VPNStatsUpdate(vpnname, stats);
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

    qInfo() << "vpnClientConnection::sendCMD::" << cmd.objName << "::" << cmd.action;

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

    QJsonDocument json = QJsonDocument::fromJson(cmd.data);
    QJsonObject jobj = json.object();

    switch(cmd.action)
    {
    case vpnApi::ACTION_VPN_UPDATE_STATUS:
        status = static_cast<vpnClientConnection::connectionStatus>(jobj["status"].toInt());
        emit VPNStatusChanged(name, status);
        break;
    case vpnApi::ACTION_CRED_REQUEST:
        emit VPNCredRequest(name);
        break;
    case vpnApi::ACTION_VPNSTATS_SUBMIT:
        vpnStats stats;
        stats.bytes_read = jobj["bytes_read"].toVariant().toLongLong();
        stats.bytes_written = jobj["bytes_written"].toVariant().toLongLong();
        stats.vpn_start = jobj["vpn_start"].toVariant().toLongLong();
        emit VPNStatsUpdate(name, stats);
        break;
    }

    socket->flush();
}

void vpnClientConnection::onClientDisconnected()
{
    qInfo() << "client disconnected::" << name;
    status = vpnClientConnection::STATUS_DISCONNECTED;
    emit VPNStatusChanged(name, status);
    socket->deleteLater();
}
