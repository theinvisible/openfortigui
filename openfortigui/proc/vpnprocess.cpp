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
#include <QEventLoop>
#include <QTimer>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonArray>
#include <QFile>
#include <QRegularExpression>
#include <QFileInfo>
#include <QDir>

#include <signal.h>
#include <unistd.h>

#include "ticonfmain.h"

// How long to wait for the worker to tear the tunnel down in an orderly
// fashion before forcing the issue. Generous, because pppd_terminate() and the
// logout at the gateway can run into a network timeout.
#define VPN_SHUTDOWN_TIMEOUT_MS 20000

vpnProcess::vpnProcess(QObject *parent) : QObject(parent)
{
    last_tunnel_state = STATE_DOWN;
    shutting_down = false;
    thread_worker = nullptr;
    thread_vpn = nullptr;
    observer = nullptr;
    observerStats = nullptr;
}

void vpnProcess::setup(const QString &vpnname)
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
        out.setVersion(QDataStream::Qt_6_0);
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

    //startVPN();
}

void vpnProcess::closeProcess()
{
    // Second entry: the worker reacted to the stop and emitted finished(),
    // the tunnel is torn down.
    if(shutting_down)
    {
        finishShutdown();
        return;
    }

    qDebug() << "shutting down vpn process::" << name;
    shutting_down = true;

    if(observer != nullptr)
        observer->stop();
    if(observerStats != nullptr)
        observerStats->stop();

    /*
     * The tunnel belongs to the worker thread -- it is not torn down from here.
     * Instead process() is made to return, which is exactly the path an
     * external SIGTERM takes: io_loop() aborts, process() restores routes and
     * DNS and terminates pppd itself, then emits finished(), which brings us
     * back here. Do not block, or the queued finished() could never arrive.
     */
    vpnWorker::requestStop();

    if(!thread_worker.isNull() && thread_worker->tunnelActive()
       && vpnWorker::stopSignalSafe())
    {
        qDebug() << "requesting tunnel shutdown via SIGTERM::" << name;
        ::kill(::getpid(), SIGTERM);
        QTimer::singleShot(VPN_SHUTDOWN_TIMEOUT_MS, this, SLOT(onShutdownTimeout()));
        return;
    }

    finishShutdown();
}

void vpnProcess::onShutdownTimeout()
{
    qWarning() << "vpn worker did not shut down in time, forcing::" << name;

    // Last resort: the worker is stuck in a blocking call. At least restore
    // the routes, then shut down the hard way.
    if(!thread_worker.isNull())
        thread_worker->end();

    finishShutdown();
}

void vpnProcess::finishShutdown()
{
    if(!thread_vpn.isNull() && thread_vpn->isRunning())
    {
        thread_vpn->quit();
        if(!thread_vpn->wait(2000))
        {
            qWarning() << "vpn thread did not stop, terminating::" << name;
            thread_vpn->terminate();
            thread_vpn->wait(1000);
        }
    }

    QCoreApplication::quit();
}

void vpnProcess::startVPN()
{
    tiConfVpnProfiles profiles;
    tiConfMain main_settings;
    bool usePasswordStore = main_settings.getValue("main/use_system_password_store").toBool();
    if(usePasswordStore)
        profiles.setReadProfilePasswords(false);
    vpnProfile *profile = profiles.getVpnProfileByName(name);
    if(profile == nullptr)
    {
        qWarning() << "VPN profile not found:" << name;
        submitVPNMessage(tr("VPN profile '%1' not found!").arg(name), vpnMsg::TYPE_ERROR);
        closeProcess();
        return;
    }
    if(!checkVPNSettings(profile))
    {
        qDebug() << "VPN settings check failed, exiting!";
        closeProcess();
        return;
    }

    // Try to fetch password from current user password store
    if(usePasswordStore)
    {
        qDebug() << "passstore requested from vpn";
        passstore_received = false;
        requestPassStore();

        // Wait for pass received
        if(!passstore_received)
        {
            QEventLoop waitLoop;
            QTimer timeoutTimer;
            timeoutTimer.setSingleShot(true);
            timeoutTimer.setInterval(30000);
            connect(this, &vpnProcess::passtoreReceived, &waitLoop, &QEventLoop::quit);
            connect(&timeoutTimer, &QTimer::timeout, &waitLoop, &QEventLoop::quit);
            timeoutTimer.start();
            waitLoop.exec();
        }

        if(!passstore_received)
        {
            submitVPNMessage(tr("Timeout for password request from passstore!"), vpnMsg::TYPE_ERROR);
            closeProcess();
            return;
        }

        profile->password = cred_data.password;
        cred_data.password = "";
    } else {
        profile->password = profile->readPassword();
    }

    // Reset stats
    stats.bytes_read = 0;
    stats.bytes_written = 0;
    stats.vpn_start = 0;

    if((!profile->username.isEmpty() && profile->password.isEmpty()) || profile->always_ask_otp)
    {
        cred_received = false;
        requestCred();

        // Wait for cred received
        if(!cred_received)
        {
            QEventLoop waitLoop;
            QTimer timeoutTimer;
            timeoutTimer.setSingleShot(true);
            timeoutTimer.setInterval(30000);
            connect(this, &vpnProcess::credReceived, &waitLoop, &QEventLoop::quit);
            connect(&timeoutTimer, &QTimer::timeout, &waitLoop, &QEventLoop::quit);
            timeoutTimer.start();
            waitLoop.exec();
        }

        if(!cred_received)
        {
            submitVPNMessage(tr("Timeout for user/password request, try again!"), vpnMsg::TYPE_ERROR);
            closeProcess();
            return;
        }

        profile->username = cred_data.username;
        if(!cred_data.password.isEmpty())
            profile->password = cred_data.password;
        profile->otp = cred_data.otp;
        cred_data.username = "";
        cred_data.password = "";
        cred_data.otp = "";

        if(profile->always_ask_otp && !profile->otp.isEmpty())
            profile->password = QString("%1,%2").arg(profile->password).arg(profile->otp);
    }

    if(profile->trust_all_gw_certs)
    {
        qDebug() << "read gw_cert_cache for " << profile->name;
        QString hash = main_settings.readGwCertCache(profile->name);
        if(!hash.isEmpty())
            profile->trusted_cert = hash;
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

bool vpnProcess::checkVPNSettings(vpnProfile *profile)
{
    bool ret = true;

    QFileInfo ltest(profile->pppd_log_file);
    QDir ldtest;
    if(!profile->pppd_log_file.isEmpty() && !ldtest.exists(ltest.absolutePath()))
    {
        submitVPNMessage(tr("PPPD log file dir %1 does not exist!").arg(profile->pppd_log_file), vpnMsg::TYPE_ERROR);
        ret = false;
    }

    return ret;
}

void vpnProcess::sendCMD(const vpnApi &cmd)
{
    QByteArray block;
    QDataStream out(&block, QIODevice::WriteOnly);
    out.setVersion(QDataStream::Qt_6_0);
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
    if(thread_worker.isNull())
        return;

    const QString ppp_iface = thread_worker->tunnelPppIface();
    if(ppp_iface.isEmpty())
        return;

    QFile file("/proc/net/dev");
    if(!file.open(QIODevice::ReadOnly | QIODevice::Text))
        return;

    QTextStream in(&file);
    QString line = in.readLine();
    QStringList lineParse;
    QRegularExpression reParse("^\\S{1,}$");
    while (!line.isNull())
    {
        lineParse = line.split(" ").filter(reParse);
        if(lineParse[0].left(lineParse[0].length() - 1) == ppp_iface)
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

void vpnProcess::submitVPNMessage(const QString &msg, int msg_type)
{
    QJsonDocument json;
    QJsonObject jsTop;

    jsTop["msg"] = msg;
    jsTop["msg_type"] = msg_type;

    json.setObject(jsTop);

    vpnApi cmd;
    cmd.objName = name;
    cmd.action = vpnApi::ACTION_VPN_MSG;
    cmd.data = json.toJson();

    sendCMD(cmd);
}

void vpnProcess::onServerReadyRead()
{
    qDebug() << "server sent something::";
    vpnApi cmd;
    QDataStream in(apiServer);
    in.setVersion(QDataStream::Qt_6_0);
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
        cred_data.otp = jobj["otp"].toString();
        cred_received = true;
        emit credReceived();
        break;
    case vpnApi::ACTION_STOREPASS_SUBMIT:
        cred_data.password = jobj["password"].toString();
        passstore_received = true;
        emit passtoreReceived();
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
    if(thread_worker.isNull())
        return;

    const int state = thread_worker->tunnelState();
    if(state < 0 || state == last_tunnel_state)
        return;

    qDebug() << "vpnProcess::onObserverUpdate::status_update" << name << "state" << state;

    switch(state)
    {
    case STATE_DOWN:
        onVPNStatusChanged(vpnClientConnection::STATUS_DISCONNECTED);
        break;
    case STATE_UP:
        qDebug() << "vpnProcess::onObserverUpdate::status_update2" << name
                 << "ppp-interface::" << thread_worker->tunnelPppIface();
        onVPNStatusChanged(vpnClientConnection::STATUS_CONNECTED);
        break;
    case STATE_CONNECTING:
        onVPNStatusChanged(vpnClientConnection::STATUS_CONNECTING);
        break;
    case STATE_DISCONNECTING:
        break;
    }

    last_tunnel_state = state;
}

void vpnProcess::onStatsUpdate()
{
    if(thread_worker.isNull())
        return;

    if(thread_worker->tunnelState() == STATE_UP)
    {
        updateStats();
        submitStats();
    }
}
