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

#include "vpnmanager.h"
#include "config.h"
#include "ticonfmain.h"
#include "proc/vpnbarracuda.h"

#include <QDataStream>
#include <QProcess>
#include <QThread>
#include <QCoreApplication>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonArray>
#include <QMessageBox>
#include <QInputDialog>

vpnManager::vpnManager(QObject *parent) : QObject(parent)
{
    QLocalServer::removeServer(openfortigui_config::name);
    server = new QLocalServer(this);
    connect(server, SIGNAL(newConnection()), this, SLOT(onClientConnected()));
    if(!server->listen(openfortigui_config::name))
        qDebug() << "vpnManager::DiskMain() on apiServer->listen::" << server->errorString();

    // Start VPN-Logger Thread
    logger_thread = new QThread;
    logger = new vpnLogger();
    logger->moveToThread(logger_thread);
    //connect(worker, SIGNAL(error(QString)), this, SLOT(errorString(QString)));
    connect(this, SIGNAL(addVPNLogger(QString,QProcess*)), logger, SLOT(addVPN(QString,QProcess*)), Qt::QueuedConnection);
    connect(logger_thread, SIGNAL(started()), logger, SLOT(process()));
    connect(logger, SIGNAL(finished()), logger_thread, SLOT(quit()));
    connect(logger, SIGNAL(finished()), logger, SLOT(deleteLater()));
    connect(logger, SIGNAL(OTPRequest(QProcess*)), this, SLOT(onOTPRequest(QProcess*)), Qt::QueuedConnection);
    connect(logger, SIGNAL(CertificateValidationFailed(QString,QString)), this, SLOT(onCertificateValidationFailed(QString,QString)), Qt::QueuedConnection);
    connect(logger, SIGNAL(VPNMessage(QString,vpnMsg)), this, SLOT(onClientVPNMessage(QString,vpnMsg)), Qt::QueuedConnection);
    connect(logger_thread, SIGNAL(finished()), logger_thread, SLOT(deleteLater()));
    logger_thread->start();
}

vpnManager::~vpnManager()
{
    if(logger_thread->isRunning())
        logger_thread->quit();

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
    tiConfMain main_settings;

    if(connections.contains(name))
    {
        qDebug() << "VPN already running with name" << name;
        return;
    }

    tiConfVpnProfiles profiles;
    vpnProfile *profile = profiles.getVpnProfileByName(name);

    switch(profile->device_type)
    {
    case vpnProfile::Device_Barracuda:
    {
        if(isSomeBarracudaConnected()) {
            vpnMsg msg;
            msg.msg = tr("Only one running barracuda VPN allowed!");
            msg.type = vpnMsg::TYPE_ERROR;
            emit VPNMessage(name, msg);
            return;
        }

        if(profile->username.isEmpty() || profile->readPassword().isEmpty()) {
            vpnMsg msg;
            msg.msg = tr("Please provide username and password in your VPN profile!");
            msg.type = vpnMsg::TYPE_ERROR;
            emit VPNMessage(name, msg);
            return;
        }

        QString otptoken = "";
        if(profile->always_ask_otp) {
            bool ok;
            otptoken = QInputDialog::getText(static_cast<QWidget *>(this->parent()), tr("OTP-Token required"),
                                             tr("Please provide your OTP-Token:"), QLineEdit::Normal,
                                             "", &ok);
        }

        vpnClientConnection *clientConn = new vpnClientConnection(name);
        vpnBarracuda *vpn = new vpnBarracuda(clientConn);
        clientConn->setBarracudaObj(vpn);
        connect(vpn, SIGNAL(VPNStatusChanged(QString,vpnClientConnection::connectionStatus)), this, SLOT(onClientVPNStatusChanged(QString,vpnClientConnection::connectionStatus)));
        connect(vpn, SIGNAL(VPNCredRequest(QString)), this, SLOT(onClientVPNCredRequest(QString)), Qt::QueuedConnection);
        connect(vpn, SIGNAL(VPNStatsUpdate(QString,vpnStats)), this, SLOT(onClientVPNStatsUpdate(QString,vpnStats)), Qt::QueuedConnection);
        connect(vpn, SIGNAL(VPNMessage(QString,vpnMsg)), this, SLOT(onClientVPNMessage(QString,vpnMsg)), Qt::QueuedConnection);
        connect(vpn, QOverload<QString, QProcess*>::of(&vpnBarracuda::addVPNLogger), this, [=](QString n, QProcess *p) { emit addVPNLogger(n, p); });
        vpn->start(name, clientConn, otptoken);
        connections[name] = clientConn;
        break;
    }
    case vpnProfile::Device_Fortigate:
    default:
    {
        QStringList arguments;
        if(main_settings.getValue("main/sudo_preserve_env").toBool())
            arguments << "-E";
        arguments << QCoreApplication::applicationFilePath();
        arguments << "--start-vpn";
        arguments << "--vpn-name";
        arguments << name;
        arguments << "--main-config";
        arguments << tiConfMain::formatPath(QString("%1").arg(tiConfMain::main_config));

        QProcess *vpnProc = new QProcess(this);
        vpnProc->setProcessChannelMode(QProcess::MergedChannels);
    #if QT_VERSION > QT_VERSION_CHECK(5, 7, 0)
        connect(vpnProc, QOverload<int, QProcess::ExitStatus>::of(&QProcess::finished), this, [=](int exitCode, QProcess::ExitStatus exitStatus){ onVPNProcessFinished(name, exitCode, exitStatus); }, Qt::QueuedConnection);
        connect(vpnProc, &QProcess::errorOccurred, this, [=](QProcess::ProcessError error){ onVPNProcessErrorOccurred(name, error); }, Qt::QueuedConnection);
    #endif
        emit addVPNLogger(name, vpnProc);
        qDebug() << "Start vpn::" << name;
        vpnProc->start("sudo", arguments);
        // Close read channel to avoid memory leak
        // TODO: Process output later on
        vpnProc->waitForStarted();
        //vpnProc->closeReadChannel(QProcess::StandardOutput);
        //vpnProc->closeReadChannel(QProcess::StandardError);

        vpnClientConnection *clientConn = new vpnClientConnection(name);
        clientConn->proc = vpnProc;
        connect(clientConn, SIGNAL(VPNStatusChanged(QString,vpnClientConnection::connectionStatus)), this, SLOT(onClientVPNStatusChanged(QString,vpnClientConnection::connectionStatus)));
        connect(clientConn, SIGNAL(VPNCredRequest(QString)), this, SLOT(onClientVPNCredRequest(QString)), Qt::QueuedConnection);
        connect(clientConn, SIGNAL(VPNStatsUpdate(QString,vpnStats)), this, SLOT(onClientVPNStatsUpdate(QString,vpnStats)), Qt::QueuedConnection);
        connect(clientConn, SIGNAL(VPNMessage(QString,vpnMsg)), this, SLOT(onClientVPNMessage(QString,vpnMsg)), Qt::QueuedConnection);
        connections[name] = clientConn;
    }
    }

    //logger->addVPN(name, vpnProc);

}

void vpnManager::stopVPN(const QString &name)
{
    if(connections.contains(name))
    {
        tiConfVpnProfiles profiles;
        vpnProfile *profile = profiles.getVpnProfileByName(name);

        switch(profile->device_type)
        {
        case vpnProfile::Device_Barracuda:
        {
            if(connections[name]->status == vpnClientConnection::STATUS_CONNECTING)
                return;

            connections[name]->stop();
            connections.remove(name);
            break;
        }
        case vpnProfile::Device_Fortigate:
        default:
        {
            vpnApi apiData;
            apiData.objName = name;
            apiData.action = vpnApi::ACTION_STOP;

            qDebug() << "vpnManager::stopVPN::" << apiData.objName << "::" << apiData.action;

            connections[name]->sendCMD(apiData);
            connections.remove(name);
        }
        }
    }
}

vpnClientConnection *vpnManager::getClientConnection(const QString &name)
{
    vpnClientConnection *vpn = 0;
    if(connections.contains(name))
        vpn = connections[name];

    return vpn;
}

void vpnManager::submitVPNCred(const QString &vpnname, const QString &username, const QString &password, const QString &otp)
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
        jsTop["otp"] = otp;

        json.setObject(jsTop);
        data.data = json.toJson();

        vpn->sendCMD(data);
    }
}

void vpnManager::requestStats(const QString &vpnname)
{
    qDebug() << "vpnManager::requestStats";

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

bool vpnManager::isSomeClientConnected()
{
    QMapIterator<QString, vpnClientConnection*> i(connections);
    while(i.hasNext())
    {
        i.next();

        if(i.value()->status == vpnClientConnection::STATUS_CONNECTED)
            return true;
    }

    return false;
}

bool vpnManager::isSomeBarracudaConnected()
{
    QMapIterator<QString, vpnClientConnection*> i(connections);
    while(i.hasNext())
    {
        i.next();

        if(i.value()->status == vpnClientConnection::STATUS_CONNECTED && i.value()->getBarracudaObj() != 0)
            return true;
    }

    return false;
}

void vpnManager::onClientConnected()
{
    qDebug() << "vpnManager::onClientConnected()";
    if(server->hasPendingConnections())
    {
        QLocalSocket *client = server->nextPendingConnection();
        client->waitForReadyRead();
        vpnApi cmd;
        QDataStream in(client);
        in.setVersion(QDataStream::Qt_5_2);
        in >> cmd;
        qDebug() << "client api helo command::" << cmd.action << "::name::" << cmd.objName;
        client->flush();

        if(cmd.action == vpnApi::ACTION_PING)
        {
            client->close();
            return;
        }

        // If we get ACTION_VPN_START/ACTION_VPN_STOP here we drop the connection after action
        if(cmd.action == vpnApi::ACTION_VPN_START)
        {
            startVPN(cmd.objName);
            client->close();
            return;
        }

        if(cmd.action == vpnApi::ACTION_VPN_STOP)
        {
            stopVPN(cmd.objName);
            client->close();
            return;
        }

        if(cmd.action == vpnApi::ACTION_SHOW_MAIN)
        {
            emit VPNShowMainWindowRequest();
            client->close();
            return;
        }

        if(cmd.action == vpnApi::ACTION_VPNGROUP_START)
        {
            tiConfVpnGroups groups;
            vpnGroup *vpngroup = groups.getVpnGroupByName(cmd.objName);
            QStringListIterator it(vpngroup->localMembers);
            while(it.hasNext())
            {
                startVPN(it.next());
            }
        }

        if(cmd.action == vpnApi::ACTION_VPNGROUP_STOP)
        {
            tiConfVpnGroups groups;
            vpnGroup *vpngroup = groups.getVpnGroupByName(cmd.objName);
            QStringListIterator it(vpngroup->localMembers);
            while(it.hasNext())
            {
                stopVPN(it.next());
            }
        }

        //vpnClientConnection *clientConn = new vpnClientConnection(cmd.objName, client);
        if(connections.contains(cmd.objName))
            connections[cmd.objName]->setSocket(client);
        else
            qWarning() << "no socket assigned";
    }
}

void vpnManager::onClientVPNStatusChanged(QString vpnname, vpnClientConnection::connectionStatus status)
{
    qDebug() << "vpnManager::onClientVPNStatusChanged()" << vpnname << "status" << status;

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

void vpnManager::onClientVPNMessage(QString vpnname, vpnMsg msg)
{
    emit VPNMessage(vpnname, msg);
}

void vpnManager::onOTPRequest(QProcess *proc)
{
    qDebug() << "otprequest from vpnmanager";
    emit VPNOTPRequest(proc);
}

void vpnManager::onCertificateValidationFailed(QString vpnname, QString buffer)
{
    qDebug() << "certificatefailedrequest from vpnmanager";
    emit VPNCertificateValidationFailed(vpnname, buffer);
}

void vpnManager::onVPNProcessFinished(QString name, __attribute__ ((unused)) int exitCode, __attribute__ ((unused)) QProcess::ExitStatus exitStatus)
{
    qDebug() << "VPN process " << name << " finished!";
    if(connections.contains(name))
    {
        connections.remove(name);
    }
}

void vpnManager::onVPNProcessErrorOccurred(QString name, __attribute__ ((unused)) QProcess::ProcessError error)
{
    qDebug() << "VPN process " << name << " error occurred!";
    if(connections.contains(name))
    {
        connections.remove(name);
    }
}

vpnClientConnection::vpnClientConnection(const QString &n, QObject *parent) : QObject(parent)
{
    name = n;
    status = STATUS_DISCONNECTED;
    barracuda_obj = 0;
}

void vpnClientConnection::setSocket(QLocalSocket *sock)
{
    socket = sock;

    connect(socket, SIGNAL(disconnected()), this, SLOT(onClientDisconnected()));
    connect(socket, SIGNAL(readyRead()), this, SLOT(onClientReadyRead()));
}

void vpnClientConnection::setBarracudaObj(vpnBarracuda *bar)
{
    barracuda_obj = bar;
}

vpnBarracuda *vpnClientConnection::getBarracudaObj()
{
    return barracuda_obj;
}

void vpnClientConnection::sendCMD(const vpnApi &cmd)
{
    QByteArray block;
    QDataStream out(&block, QIODevice::WriteOnly);
    out.setVersion(QDataStream::Qt_5_2);
    out << cmd;

    qDebug() << "vpnClientConnection::sendCMD::" << cmd.objName << "::" << cmd.action;

    if(!socket->isOpen())
    {
        qWarning() << "Socket ist nicht offen";
        return;
    }

    socket->write(block);
    socket->flush();
}

void vpnClientConnection::stop()
{
    tiConfVpnProfiles profiles;
    vpnProfile *profile = profiles.getVpnProfileByName(name);
    switch(profile->device_type)
    {
    case vpnProfile::Device_Barracuda:
        barracuda_obj->stop();
        barracuda_obj->deleteLater();
        break;
    case vpnProfile::Device_Fortigate:
        break;
    }
}

void vpnClientConnection::submitPassStoreCred()
{
    QJsonDocument json;
    QJsonObject jsTop;
    vpnApi data;
    QString password;
    data.action = vpnApi::ACTION_STOREPASS_SUBMIT;
    data.objName = name;

    tiConfVpnProfiles profiles;
    vpnProfile *profile = profiles.getVpnProfileByName(name);
    password = profile->readPassword();

    // Ask for otp if needed
    /*
    if (profile->always_ask_otp)
    {
        QString otp = QInputDialog::getText(nullptr, tr("Enter OTP"), tr("Enter OTP token"));
        if (!otp.isEmpty())
            password = QString("%1,%2").arg(password).arg(otp);
    }
    */

    jsTop["password"] = password;

    json.setObject(jsTop);
    data.data = json.toJson();

    sendCMD(data);
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
    case vpnApi::ACTION_STOREPASS_REQUEST:
        socket->flush();
        submitPassStoreCred();
        break;
    case vpnApi::ACTION_VPNSTATS_SUBMIT:
        vpnStats stats;
        stats.bytes_read = jobj["bytes_read"].toVariant().toLongLong();
        stats.bytes_written = jobj["bytes_written"].toVariant().toLongLong();
        stats.vpn_start = jobj["vpn_start"].toVariant().toLongLong();
        emit VPNStatsUpdate(name, stats);
        break;
    case vpnApi::ACTION_VPN_MSG:
        vpnMsg msg;
        msg.msg = jobj["msg"].toVariant().toString();
        msg.type = jobj["msg_type"].toVariant().toInt();
        emit VPNMessage(name, msg);
        break;
    }

    socket->flush();
}

void vpnClientConnection::onClientDisconnected()
{
    qDebug() << "client disconnected::" << name;
    status = vpnClientConnection::STATUS_DISCONNECTED;
    emit VPNStatusChanged(name, status);
    socket->deleteLater();
}
