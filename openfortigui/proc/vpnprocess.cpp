/*
 *  Copyright (C) 2018 Rene Hadler
 *
 *  This program is free software: you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation, either version 3 of the License, or
 *  (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#include "vpnprocess.h"

#include <QDataStream>
#include <QCoreApplication>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonArray>
#include <QFile>
#include <QRegExp>

#include "ticonfmain.h"

vpnProcess::vpnProcess(QObject *parent) : QObject(parent)
{
    init_last_tunnel = false;
}

void vpnProcess::run(const QString &vpnname)
{
    name = vpnname;

    apiServer = new QLocalSocket(this);
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
}

void vpnProcess::closeProcess()
{
    qDebug() << "shutting down vpn process::" << name;

    thread_worker->end();
    thread_vpn->quit();
    QThread::sleep(2);
    thread_vpn->terminate();
    QCoreApplication::quit();
}

void vpnProcess::startVPN()
{
    qDebug() << "vpnProcess::startVPN::slot";

    tiConfVpnProfiles profiles;
    tiConfMain main_settings;
    bool usePasswordStore = main_settings.getValue("main/use_system_password_store").toBool();
    if(usePasswordStore)
        profiles.setReadProfilePasswords(false);
    vpnProfile *profile = profiles.getVpnProfileByName(name);

    // Try to fetch password from current user password store
    if(usePasswordStore)
    {
        qDebug() << "passstore requested from vpn";
        passstore_received = false;
        requestPassStore();

        // Wait for pass received
        int wmax = 0;
        while(!passstore_received && wmax < 30)
        {
            QThread::sleep(1);
            QCoreApplication::processEvents();
            wmax++;
        }

        if(wmax == 30)
        {
            closeProcess();
            return;
        }

        profile->password = cred_data.password;
        cred_data.password = "";
    }

    // Reset stats
    stats.bytes_read = 0;
    stats.bytes_written = 0;
    stats.vpn_start = 0;

    if(profile->username.isEmpty() || profile->password.isEmpty())
    {
        cred_received = false;
        requestCred();

        // Wait for cred received
        int wmax = 0;
        while(!cred_received && wmax < 30)
        {
            QThread::sleep(1);
            QCoreApplication::processEvents();
            wmax++;
        }

        if(wmax == 30)
        {
            closeProcess();
            return;
        }

        profile->username = cred_data.username;
        profile->password = cred_data.password;
        cred_data.username = "";
        cred_data.password = "";
    }

    thread_vpn = new QThread;
    thread_worker = new vpnWorker();
    thread_worker->setConfig(*profile);
    thread_worker->moveToThread(thread_vpn);
    //connect(worker, SIGNAL(error(QString)), this, SLOT(errorString(QString)));
    connect(thread_vpn, SIGNAL(started()), thread_worker, SLOT(process()));
    connect(thread_worker, SIGNAL(finished()), thread_vpn, SLOT(quit()));
    connect(thread_worker, SIGNAL(finished()), thread_worker, SLOT(deleteLater()));
    connect(thread_worker, SIGNAL(finished()), this, SLOT(closeProcess()));
    connect(thread_vpn, SIGNAL(finished()), thread_vpn, SLOT(deleteLater()));
    connect(thread_worker, SIGNAL(statusChanged(vpnClientConnection::connectionStatus)), this, SLOT(onVPNStatusChanged(vpnClientConnection::connectionStatus)));
    thread_vpn->start();

    observer = new QTimer(this);
    connect(observer, SIGNAL(timeout()), this, SLOT(onObserverUpdate()));
    observer->start(500);

    observerStats = new QTimer(this);
    connect(observerStats, SIGNAL(timeout()), this, SLOT(onStatsUpdate()));
    observerStats->start(2000);
}

void vpnProcess::sendCMD(const vpnApi &cmd)
{
    QByteArray block;
    QDataStream out(&block, QIODevice::WriteOnly);
    out.setVersion(QDataStream::Qt_5_2);
    out << cmd;

    if(!apiServer->isOpen())
    {
        qWarning() << "Socket not open";
        return;
    }

    apiServer->write(block);
    apiServer->flush();
}

void vpnProcess::updateStats()
{
    if(thread_worker->ptr_tunnel != 0)
    {
        QFile file("/proc/net/dev");
        if(!file.open(QIODevice::ReadOnly | QIODevice::Text))
            return;

        QTextStream in(&file);
        QString line = in.readLine();
        QStringList lineParse;
        QRegExp reParse = QRegExp("^\\S{1,}$");
        while (!line.isNull())
        {
            lineParse = line.split(" ").filter(reParse);
            if(lineParse[0].left(lineParse[0].length() - 1) == thread_worker->ptr_tunnel->ppp_iface)
            {
                stats.bytes_read = lineParse[1].toLongLong();
                stats.bytes_written = lineParse[9].toLongLong();
                file.close();
                return;
            }

            line = in.readLine();
        }
        file.close();
    }
}

void vpnProcess::requestCred()
{
    QJsonDocument json;
    QJsonObject jsTop;

    json.setObject(jsTop);

    vpnApi cmd;
    cmd.objName = name;
    cmd.action = vpnApi::ACTION_CRED_REQUEST;
    cmd.data = json.toJson();

    sendCMD(cmd);
}

void vpnProcess::requestPassStore()
{
    QJsonDocument json;
    QJsonObject jsTop;

    json.setObject(jsTop);

    vpnApi cmd;
    cmd.objName = name;
    cmd.action = vpnApi::ACTION_STOREPASS_REQUEST;
    cmd.data = json.toJson();

    sendCMD(cmd);
}

void vpnProcess::submitStats()
{
    QJsonDocument json;
    QJsonObject jsTop;

    jsTop["bytes_read"] = stats.bytes_read;
    jsTop["bytes_written"] = stats.bytes_written;
    jsTop["vpn_start"] = stats.vpn_start;

    json.setObject(jsTop);

    vpnApi cmd;
    cmd.objName = name;
    cmd.action = vpnApi::ACTION_VPNSTATS_SUBMIT;
    cmd.data = json.toJson();

    sendCMD(cmd);
}

void vpnProcess::onServerReadyRead()
{
    qDebug() << "server sent something::";
    vpnApi cmd;
    QDataStream in(apiServer);
    in.setVersion(QDataStream::Qt_5_2);
    in >> cmd;
    qDebug() << "server api command2::" << cmd.action << "::name::" << cmd.objName;

    QJsonDocument json = QJsonDocument::fromJson(cmd.data);
    QJsonObject jobj = json.object();

    switch(cmd.action)
    {
    case vpnApi::ACTION_STOP:
        closeProcess();
        break;
    case vpnApi::ACTION_CRED_SUBMIT:
        cred_data.username = jobj["username"].toString();
        cred_data.password = jobj["password"].toString();
        cred_received = true;
        break;
    case vpnApi::ACTION_STOREPASS_SUBMIT:
        cred_data.password = jobj["password"].toString();
        passstore_received = true;
        break;
    case vpnApi::ACTION_VPNSTATS_REQUEST:
        submitStats();
        break;
    }

    apiServer->flush();
}

void vpnProcess::onServerDisconnected()
{
    qDebug() << "server socket disconnected, exiting";

    closeProcess();
}

void vpnProcess::onVPNStatusChanged(vpnClientConnection::connectionStatus status)
{
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
    if(thread_worker->ptr_tunnel != 0)
    {
        if(!init_last_tunnel)
        {
            last_tunnel = *(thread_worker->ptr_tunnel);
            last_tunnel.state = STATE_DOWN;
            init_last_tunnel = true;
        }

        if(thread_worker->ptr_tunnel->state != last_tunnel.state)
        {
            qDebug() << "vpnProcess::onObserverUpdate::status_update" << name << "state" << thread_worker->ptr_tunnel->state;

            switch(thread_worker->ptr_tunnel->state)
            {
            case STATE_DOWN:
                qDebug() << "vpnProcess::onObserverUpdate::status_update2" << name << "state" << thread_worker->ptr_tunnel->state;
                onVPNStatusChanged(vpnClientConnection::STATUS_DISCONNECTED);
                break;
            case STATE_UP:
                qDebug() << "vpnProcess::onObserverUpdate::status_update2" << name << "state" << thread_worker->ptr_tunnel->state << "ppp-interface::" << thread_worker->ptr_tunnel->ppp_iface;
                onVPNStatusChanged(vpnClientConnection::STATUS_CONNECTED);
                break;
            case STATE_CONNECTING:
                qDebug() << "vpnProcess::onObserverUpdate::status_update2" << name << "state" << thread_worker->ptr_tunnel->state;
                onVPNStatusChanged(vpnClientConnection::STATUS_CONNECTING);
                break;
            }

            last_tunnel.state = thread_worker->ptr_tunnel->state;
        }
    }
}

void vpnProcess::onStatsUpdate()
{
    if(thread_worker->ptr_tunnel->state == STATE_UP)
    {
        updateStats();
        submitStats();
    }
}
