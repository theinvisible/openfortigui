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
#include <QFile>
#include <QProcess>
#include <QThread>
#include <QTimer>
#include <QCoreApplication>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonArray>
#include <QMessageBox>
#include <QInputDialog>

vpnManager::vpnManager(QObject *parent) : QObject(parent)
{
    const QString socket_path = vpnApi::socketPath();
    QLocalServer::removeServer(socket_path);
    server = new QLocalServer(this);
    // Credentials travel over this socket in the clear; restrict it to the
    // owning user. The root VPN child is unaffected, root bypasses file modes.
    server->setSocketOptions(QLocalServer::UserAccessOption);
    connect(server, SIGNAL(newConnection()), this, SLOT(onClientConnected()));
    if(!server->listen(socket_path))
        qDebug() << "vpnManager::DiskMain() on apiServer->listen::" << socket_path << "::" << server->errorString();
    else
        qDebug() << "vpnManager:: api socket listening on" << socket_path;

    // Start VPN-Logger Thread
    logger_thread = new QThread;
    logger = new vpnLogger();
    logger->moveToThread(logger_thread);
    //connect(worker, SIGNAL(error(QString)), this, SLOT(errorString(QString)));
    connect(this, SIGNAL(addVPNLogger(QString,QProcess*)), logger, SLOT(addVPN(QString,QProcess*)), Qt::QueuedConnection);
    connect(logger_thread, SIGNAL(started()), logger, SLOT(process()));
    // vpnLogger has no finished() signal -- the two connects that used to be here
    // never did anything but print "No such signal" on every start. The thread is
    // quit in the destructor.
    connect(logger, SIGNAL(PromptRequest(QProcess*,int)), this, SLOT(onPromptRequest(QProcess*,int)), Qt::QueuedConnection);
    connect(logger, SIGNAL(CertificateValidationFailed(QString,QString)), this, SLOT(onCertificateValidationFailed(QString,QString)), Qt::QueuedConnection);
    connect(logger, SIGNAL(SAMLAuthRequest(QString)), this, SLOT(onSAMLAuthRequest(QString)), Qt::QueuedConnection);
    connect(logger, SIGNAL(VPNMessage(QString,vpnMsg)), this, SLOT(onClientVPNMessage(QString,vpnMsg)), Qt::QueuedConnection);
    connect(logger_thread, SIGNAL(finished()), logger_thread, SLOT(deleteLater()));
    logger_thread->start();
}

vpnManager::~vpnManager()
{
    if(logger_thread->isRunning())
        logger_thread->quit();

    for (const QString &name : connections.keys())
    {
        if(connections.contains(name) && connections[name]->status != vpnClientConnection::STATUS_DISCONNECTED)
        {
            stopVPN(name);
        }
    }
}

void vpnManager::startVPN(const QString &name)
{
    if(connections.contains(name))
    {
        qDebug() << "VPN already running with name" << name;
        return;
    }

    // A fresh attempt starts without the errors of the previous one.
    reported_errors.remove(name);

    tiConfVpnProfiles profiles;
    vpnProfile *profile = profiles.getVpnProfileByName(name);
    if(profile == nullptr)
    {
        /*
         * Anything on the api socket may ask for a name that does not exist --
         * the KRunner plugin with a stale entry, a renamed profile, a typo in a
         * script. Dereferencing the null below took the whole GUI down with a
         * segmentation fault.
         */
        vpnMsg msg;
        msg.type = vpnMsg::TYPE_ERROR;
        msg.msg = tr("There is no VPN profile named '%1'.").arg(name);
        emit VPNMessage(name, msg);
        return;
    }

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
        arguments << QCoreApplication::applicationFilePath();
        arguments << "--start-vpn";
        arguments << "--vpn-name";
        arguments << name;
        arguments << "--main-config";
        arguments << tiConfMain::formatPath(QString("%1").arg(tiConfMain::main_config));
        // The child runs as root and cannot derive our runtime location itself.
        arguments << "--api-socket";
        arguments << vpnApi::socketPath();

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
    vpnClientConnection *vpn = nullptr;
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
    for (auto it = connections.cbegin(); it != connections.cend(); ++it)
    {
        if(it.value()->status == vpnClientConnection::STATUS_CONNECTED)
            return true;
    }

    return false;
}

bool vpnManager::isSomeBarracudaConnected()
{
    for (auto it = connections.cbegin(); it != connections.cend(); ++it)
    {
        if(it.value()->status == vpnClientConnection::STATUS_CONNECTED && it.value()->getBarracudaObj() != nullptr)
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
        in.setVersion(QDataStream::Qt_6_0);
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
            for (const QString &member : vpngroup->localMembers)
            {
                startVPN(member);
            }
        }

        if(cmd.action == vpnApi::ACTION_VPNGROUP_STOP)
        {
            tiConfVpnGroups groups;
            vpnGroup *vpngroup = groups.getVpnGroupByName(cmd.objName);
            for (const QString &member : vpngroup->localMembers)
            {
                stopVPN(member);
            }
        }

        //vpnClientConnection *clientConn = new vpnClientConnection(cmd.objName, client);
        if(connections.contains(cmd.objName))
            connections[cmd.objName]->setSocket(client);
        else
            qWarning() << "no socket assigned";
    }
}

/*
 * The single most reported cause for a silently failing attempt is a VPN of
 * another vendor already holding the default route (issue #164). By the time a
 * failed attempt is reported our own ppp interface is gone again, so a default
 * route through a ppp/tun/tap/wg interface at that moment belongs to someone
 * else. Only a hint -- parallel VPNs can work -- hence the careful wording.
 */
static QString otherVpnHint()
{
    QFile route(QStringLiteral("/proc/net/route"));
    if(!route.open(QIODevice::ReadOnly | QIODevice::Text))
        return QString();

    const QStringList lines = QString::fromLatin1(route.readAll()).split('\n', Qt::SkipEmptyParts);
    for(int i = 1; i < lines.size(); i++) // line 0 is the header
    {
        const QStringList fields = lines.at(i).split('\t', Qt::SkipEmptyParts);
        if(fields.size() < 2 || fields.at(1) != QLatin1String("00000000"))
            continue;

        const QString iface = fields.at(0);
        if(iface.startsWith("ppp") || iface.startsWith("tun")
           || iface.startsWith("tap") || iface.startsWith("wg"))
            return QObject::tr(" Another VPN connection appears to be active (interface %1) -- "
                               "this can prevent the tunnel from coming up.").arg(iface);
    }

    return QString();
}

void vpnManager::onClientVPNStatusChanged(QString vpnname, vpnClientConnection::connectionStatus status)
{
    qDebug() << "vpnManager::onClientVPNStatusChanged()" << vpnname << "status" << status;

    if(status == vpnClientConnection::STATUS_CONNECTED && connections.contains(vpnname))
        connections[vpnname]->ever_connected = true;

    if(status == vpnClientConnection::STATUS_DISCONNECTED)
    {
        /*
         * A connection that is still in the map was NOT stopped by the user:
         * stopVPN() removes it before the child starts its teardown. If it never
         * reached CONNECTED either, this was a failed connection attempt -- which
         * used to vanish without a word (issue #164): the row flipped back to
         * "Disconnected" while the reason sat only in the log file.
         */
        const bool failed_attempt = connections.contains(vpnname)
                && !connections[vpnname]->ever_connected;

        connections.remove(vpnname);

        if(failed_attempt)
            reportProcessFailure(vpnname, tr("The connection attempt for VPN '%1' failed.").arg(vpnname)
                                          + otherVpnHint());
    }

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
    /*
     * Note that this VPN already has an error on screen. The logger recognises
     * the common failures in the child's output and explains them properly; the
     * generic report from the process handlers must not duplicate that.
     */
    if(msg.type == vpnMsg::TYPE_ERROR)
        reported_errors.insert(vpnname);

    emit VPNMessage(vpnname, msg);
}

void vpnManager::onPromptRequest(QProcess *proc, int type)
{
    qDebug() << "prompt request from vpnmanager, type" << type;
    emit VPNPromptRequest(proc, type);
}

void vpnManager::onCertificateValidationFailed(QString vpnname, QString buffer)
{
    qDebug() << "certificatefailedrequest from vpnmanager";
    emit VPNCertificateValidationFailed(vpnname, buffer);
}

void vpnManager::onSAMLAuthRequest(QString vpnname)
{
    qDebug() << "saml auth request from vpnmanager for" << vpnname;
    emit VPNSAMLAuthRequest(vpnname);
}

/*
 * The last lines of the per-VPN log. The logger writes the child's output there
 * and flushes on every chunk, so by the time the delayed report fires, whatever
 * sudo or the child had to say is in the file. Reading it from there rather than
 * from the process buffer matters: draining the buffer would take the data away
 * from the logger, and the log would lose the very message being complained
 * about.
 */
static QString logTail(const QString &name, int lines = 15)
{
    tiConfMain main_settings;
    const QString path = QString("%1/vpn/%2.log")
            .arg(tiConfMain::formatPath(main_settings.getValue("paths/logs").toString()), name);
    const QString pointer = QObject::tr("Log: %1").arg(path);

    QFile file(path);
    if(!file.open(QIODevice::ReadOnly | QIODevice::Text))
        return pointer;

    // The log grows across runs, only its end is of interest.
    const qint64 window = 8192;
    if(file.size() > window)
        file.seek(file.size() - window);

    QStringList tail = QString::fromUtf8(file.readAll()).split('\n', Qt::SkipEmptyParts);
    if(tail.size() > lines)
        tail = tail.mid(tail.size() - lines);

    if(tail.isEmpty())
        return pointer;

    return QString("%1\n\n%2").arg(tail.join('\n'), pointer);
}

/*
 * Report a failure of the child process -- but not right away.
 *
 * The logger recognises the frequent causes in the child's output and explains
 * them far better than an exit code can (sudo asking for a password, for
 * instance). It needs a moment to read that output, so the generic message waits
 * and is dropped if a specific one arrived meanwhile: one failure, one dialog.
 */
void vpnManager::reportProcessFailure(const QString &name, const QString &text)
{
    QTimer::singleShot(2000, this, [this, name, text]() {
        if(reported_errors.contains(name))
            return;

        // Started again in the meantime -- no point in reporting the old attempt.
        if(connections.contains(name))
            return;

        vpnMsg msg;
        msg.type = vpnMsg::TYPE_ERROR;
        msg.msg = text;
        msg.detail = logTail(name);
        emit VPNMessage(name, msg);
    });
}

/*
 * Both handlers used to drop the process without a word: when sudo refused, the
 * entry simply disappeared from the list again. The logger covers the causes it
 * has a pattern for; everything else -- "user is not allowed to execute", a
 * syntactically broken sudoers file, a missing sudo, a crash -- left no trace the
 * user would ever look at.
 *
 * Only an abnormal end gets a dialog. Stopping a VPN ends in
 * QCoreApplication::quit() and therefore in exit code 0, and a connection
 * attempt that fails reports itself over the api, also with 0. A crash is left to
 * onVPNProcessErrorOccurred(), which Qt emits before finished().
 */
void vpnManager::onVPNProcessFinished(QString name, int exitCode, QProcess::ExitStatus exitStatus)
{
    qDebug() << "VPN process " << name << " finished::" << exitCode << exitStatus;

    if(!connections.contains(name))
        return;

    const bool ever_connected = connections[name]->ever_connected;
    connections.remove(name);

    if(exitStatus == QProcess::NormalExit && exitCode != 0)
        reportProcessFailure(name, tr("VPN '%1' could not be started, sudo ended with exit code %2. "
                                      "Please check the rule in /etc/sudoers.d/openfortigui.")
                                   .arg(name).arg(exitCode));
    /*
     * Exit code 0, but the attempt never reached CONNECTED and no DISCONNECTED
     * status arrived either (that removes the map entry in
     * onClientVPNStatusChanged): the child died before it ever talked on the
     * api socket. Same silent failure class as issue #164.
     */
    else if(exitStatus == QProcess::NormalExit && !ever_connected)
        reportProcessFailure(name, tr("The connection attempt for VPN '%1' failed.").arg(name)
                                   + otherVpnHint());
}

void vpnManager::onVPNProcessErrorOccurred(QString name, QProcess::ProcessError error)
{
    qDebug() << "VPN process " << name << " error occurred::" << error;

    if(!connections.contains(name))
        return;

    QProcess *proc = connections[name]->proc;
    connections.remove(name);

    QString text;
    switch(error)
    {
    case QProcess::FailedToStart:
        text = tr("VPN '%1' could not be started: '%2' was not found or is not executable.")
               .arg(name, (proc != nullptr) ? proc->program() : QStringLiteral("sudo"));
        break;
    case QProcess::Crashed:
        text = tr("The VPN process of '%1' ended unexpectedly.").arg(name);
        break;
    case QProcess::Timedout:
        text = tr("Timeout while waiting for the VPN process of '%1'.").arg(name);
        break;
    case QProcess::WriteError:
        text = tr("Could not write to the VPN process of '%1'.").arg(name);
        break;
    case QProcess::ReadError:
        text = tr("Could not read from the VPN process of '%1'.").arg(name);
        break;
    case QProcess::UnknownError:
    default:
        text = tr("Unknown error in the VPN process of '%1' (code %2).")
               .arg(name).arg(static_cast<int>(error));
        break;
    }

    reportProcessFailure(name, text);
}

vpnClientConnection::vpnClientConnection(const QString &n, QObject *parent) : QObject(parent)
{
    name = n;
    status = STATUS_DISCONNECTED;
    proc = nullptr;
    socket = nullptr;
    barracuda_obj = nullptr;
    ever_connected = false;
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
    out.setVersion(QDataStream::Qt_6_0);
    out << cmd;

    qDebug() << "vpnClientConnection::sendCMD::" << cmd.objName << "::" << cmd.action;

    // The socket only exists once the child has connected -- stopping a VPN
    // whose attempt is still in the sudo/startup phase must not crash here.
    if(socket == nullptr || !socket->isOpen())
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
    // With the system password store the child cannot decrypt the profile
    // itself (key and IV live in the keychain), so the cookie travels along.
    jsTop["cookie"] = profile->readCookie();

    json.setObject(jsTop);
    data.data = json.toJson();

    sendCMD(data);
}

void vpnClientConnection::onClientReadyRead()
{
    vpnApi cmd;
    QDataStream in(socket);
    in.setVersion(QDataStream::Qt_6_0);
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
