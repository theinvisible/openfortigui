#include "vpnprocess.h"

#include <QDataStream>
#include <QCoreApplication>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonArray>

#include "ticonfmain.h"
#include "vpnapi.h"

vpnProcess::vpnProcess(QObject *parent) : QObject(parent)
{
    init_last_tunnel = false;
}

void vpnProcess::run(const QString &vpnname)
{
    name = vpnname;

    apiServer = new QLocalSocket(this);
    //connect(apiClient, SIGNAL(disconnected()), this, SLOT(onManualBackupFinished()));
    connect(apiServer, SIGNAL(readyRead()), this, SLOT(onServerReadyRead()));
    apiServer->connectToServer(openfortigui_config::name);
    if(apiServer->waitForConnected(1000))
    {
        connect(apiServer, SIGNAL(disconnected()), this, SLOT(onServerDisconnected()));

        QByteArray block;
        QDataStream out(&block, QIODevice::WriteOnly);
        out.setVersion(QDataStream::Qt_5_2);
        vpnApi apiData;
        apiData.objName = name;
        apiData.action = vpnApi::ACTION_HELLO;
        out << apiData;

        apiServer->write(block);
        apiServer->flush();
    }
    else
    {
        qWarning() << apiServer->errorString();
    }

    startVPN();

    //apiClient->close();
    //apiClient->disconnect();
}

void vpnProcess::closeProcess()
{
    qInfo() << "shutting down vpn process::" << name;
    QCoreApplication::exit(0);
}

void vpnProcess::startVPN()
{
    qInfo() << "vpnProcess::startVPN::slot";

    tiConfVpnProfiles profiles;
    thread_vpn = new QThread;
    thread_worker = new vpnWorker();
    thread_worker->setConfig(*profiles.getVpnProfileByName(name));
    thread_worker->moveToThread(thread_vpn);
    //connect(worker, SIGNAL(error(QString)), this, SLOT(errorString(QString)));
    connect(thread_vpn, SIGNAL(started()), thread_worker, SLOT(process()));
    connect(thread_worker, SIGNAL(finished()), thread_vpn, SLOT(quit()));
    connect(thread_worker, SIGNAL(finished()), thread_worker, SLOT(deleteLater()));
    connect(thread_vpn, SIGNAL(finished()), thread_vpn, SLOT(deleteLater()));
    connect(thread_worker, SIGNAL(statusChanged(vpnClientConnection::connectionStatus)), this, SLOT(onVPNStatusChanged(vpnClientConnection::connectionStatus)));
    thread_vpn->start();

    observer = new QTimer(this);
    connect(observer, SIGNAL(timeout()), this, SLOT(onObserverUpdate()));
    observer->start(500);
}

void vpnProcess::sendCMD(const vpnApi &cmd)
{
    QByteArray block;
    QDataStream out(&block, QIODevice::WriteOnly);
    out.setVersion(QDataStream::Qt_5_2);
    out << cmd;

    if(!apiServer->isOpen())
    {
        qInfo() << "Socket ist nicht offen";
        return;
    }

    apiServer->write(block);
    apiServer->flush();
}

void vpnProcess::onServerReadyRead()
{
    qInfo() << "server sent something::";
    vpnApi cmd;
    QDataStream in(apiServer);
    in.setVersion(QDataStream::Qt_5_2);
    in >> cmd;
    qInfo() << "server api command2::" << cmd.action << "::name::" << cmd.objName;

    switch(cmd.action)
    {
    case vpnApi::ACTION_STOP:
        closeProcess();
        break;
    }

    apiServer->flush();
}

void vpnProcess::onServerDisconnected()
{
    qInfo() << "server socket disconnected, exiting";

    thread_vpn->quit();
    QThread::sleep(2);
    thread_vpn->terminate();
    QCoreApplication::quit();
}

void vpnProcess::onVPNStatusChanged(vpnClientConnection::connectionStatus status)
{
    qInfo() << "vpn status change::" << status;

    QJsonDocument json;
    QJsonObject jsTop;

    jsTop["status"] = status;

    json.setObject(jsTop);

    vpnApi cmd;
    cmd.objName = name;
    cmd.action = vpnApi::ACTION_VPN_UPDATE_STATUS;
    cmd.data = json.toJson();

    sendCMD(cmd);
}

void vpnProcess::onObserverUpdate()
{
    qInfo() << "vpnWorker::onObserverUpdate::slot";


    if(thread_worker->ptr_tunnel != 0)
    {
        if(!init_last_tunnel)
        {
            last_tunnel = *(thread_worker->ptr_tunnel);
            init_last_tunnel = true;
        }

        qInfo() << "vpnWorker::onObserverUpdate::state" << thread_worker->ptr_tunnel->state;

        if(thread_worker->ptr_tunnel->state != last_tunnel.state)
        {
            switch(thread_worker->ptr_tunnel->state)
            {
            case STATE_DOWN:
                onVPNStatusChanged(vpnClientConnection::STATUS_DISCONNECTED);
                break;
            case STATE_UP:
                onVPNStatusChanged(vpnClientConnection::STATUS_CONNECTED);
                break;
            }

            last_tunnel.state = thread_worker->ptr_tunnel->state;
        }
    }
}
