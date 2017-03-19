#include "vpnprocess.h"

#include <QDataStream>
#include <QThread>
#include <QCoreApplication>

#include "ticonfmain.h"
#include "vpnapi.h"
#include "vpnworker.h"

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
    tiConfVpnProfiles profiles;
    QThread* thread = new QThread;
    vpnWorker* worker = new vpnWorker();
    worker->setConfig(*profiles.getVpnProfileByName(name));
    worker->moveToThread(thread);
    //connect(worker, SIGNAL(error(QString)), this, SLOT(errorString(QString)));
    connect(thread, SIGNAL(started()), worker, SLOT(process()));
    //connect(worker, SIGNAL(finished()), thread, SLOT(quit()));
    //connect(worker, SIGNAL(finished()), worker, SLOT(deleteLater()));
    connect(thread, SIGNAL(finished()), thread, SLOT(deleteLater()));
    //connect(worker, SIGNAL(diskRemoved(DeviceDisk*)), this, SLOT(onDiskRemoved(DeviceDisk*)));
    //connect(worker, SIGNAL(diskAdded(DeviceDisk*)), this, SLOT(onDiskAdded(DeviceDisk*)));
    thread->start();
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

