#include "vpnprocess.h"

#include <QDataStream>
#include <QThread>
#include <QCoreApplication>

#include "vpnapi.h"

vpnProcess::vpnProcess(QObject *parent) : QObject(parent)
{

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
        QByteArray block;
        QDataStream out(&block, QIODevice::WriteOnly);
        out.setVersion(QDataStream::Qt_5_2);
        vpnApi apiData;
        apiData.objName = name;
        apiData.action = VPN_HELLO;
        out << apiData;

        apiServer->write(block);
        apiServer->flush();
    }
    else
    {
        qWarning() << apiServer->errorString();
    }

    //apiClient->close();
    //apiClient->disconnect();
}

void vpnProcess::closeProcess()
{
    qInfo() << "shutting down vpn process::" << name;
    QCoreApplication::exit(0);
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
    case VPN_STOP:
        closeProcess();
        break;
    }

    apiServer->flush();
}

