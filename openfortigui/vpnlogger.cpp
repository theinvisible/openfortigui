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

#include "vpnlogger.h"

#include <QDebug>
#include <QDateTime>
#include <QThread>
#include <QMetaObject>

#include "ticonfmain.h"

vpnLogger::vpnLogger(QObject *parent) : QObject(parent)
{
    logMapperFinished = new QSignalMapper(this);
    loggers = QMap<QString, QPointer<QProcess>>();
    logfiles = QMap<QString, QFile*>();
    logCertFailedMode = QMap<QString, bool>();
    logCertFailedBuffer = QMap<QString, QString>();
    vpnConfigs = QMap<QString, vpnProfile>();

    connect(logMapperFinished, SIGNAL(mapped(QString)), this, SLOT(procFinished(QString)));
}

vpnLogger::~vpnLogger()
{

}

void vpnLogger::addVPN(const QString &name, QProcess *proc)
{
    qDebug() << "add logger" << tiConfMain::main_config;
    tiConfVpnProfiles profiles;
    vpnProfile *profile = profiles.getVpnProfileByName(name);

    vpnConfigs.insert(name, *profile);
    loggers.insert(name, proc);
    logCertFailedMode.insert(name, false);
    logCertFailedBuffer.insert(name, "");
    if(!logfiles.contains(name))
    {
        QFile *file = new QFile(QString("%1/vpn/%2.log").arg(tiConfMain::formatPath(main_settings.getValue("paths/logs").toString()), name));
        file->open(QIODevice::Append | QIODevice::Text);
        logfiles.insert(name, file);
    }

    connect(proc, &QProcess::readyReadStandardOutput, proc, [this, name, proc]() {
        QByteArray data = proc->readAll();
        if(!data.isEmpty())
            QMetaObject::invokeMethod(this, "logVPNData", Qt::QueuedConnection,
                                      Q_ARG(QString, name), Q_ARG(QByteArray, data));
    });
    connect(proc, SIGNAL(finished(int)), logMapperFinished, SLOT(map()));
    logMapperFinished->setMapping(proc, name);
}

void vpnLogger::logVPNData(const QString &name, const QByteArray &data)
{
    QFile *logfile = logfiles.value(name, nullptr);
    if(logfile == nullptr)
        return;

    QTextStream out(logfile);
    QString toLog = QString::fromUtf8(data);

    if(toLog.contains("Please load the ppp"))
    {
        vpnMsg msg;
        msg.msg = tr("Error: %1").arg(toLog);
        msg.type = vpnMsg::TYPE_ERROR;
        emit VPNMessage(name, msg);

        out << toLog;
        logfile->flush();
        return;
    }

    if(toLog.contains("ERROR:  Could not authenticate to gateway"))
    {
        vpnMsg msg;
        msg.msg = tr("Error: Authentication failed, please check your username/password/cert/otp!");
        msg.detail = tr("Error: %1").arg(toLog);
        msg.type = vpnMsg::TYPE_ERROR;
        emit VPNMessage(name, msg);

        out << toLog;
        logfile->flush();
        return;
    }

    if(toLog.contains("ERROR:  connect"))
    {
        vpnMsg msg;
        msg.msg = tr("Error: Could not connect to VPN-Gateway!");
        msg.detail = tr("Error: %1").arg(toLog);
        msg.type = vpnMsg::TYPE_ERROR;
        emit VPNMessage(name, msg);

        out << toLog;
        logfile->flush();
        return;
    }

    bool needsOtp = false;
    if(vpnConfigs[name].otp_prompt.isEmpty())
    {
        needsOtp = toLog.contains("Please") ||
                   toLog.contains("2factor authentication token:") ||
                   toLog.contains("Two-factor authentication") ||
                   toLog.contains("one-time password");
    } else {
        needsOtp = toLog.contains(vpnConfigs[name].otp_prompt);
    }
    if(needsOtp)
    {
        QPointer<QProcess> proc = loggers.value(name);
        if(!proc.isNull())
            emit OTPRequest(proc);
    }

    if(toLog.contains("Gateway certificate validation failed, and the certificate digest is not in the local whitelist."))
        logCertFailedMode[name] = true;

    if(logCertFailedMode[name])
        logCertFailedBuffer[name].append(toLog);

    if((logCertFailedMode[name] && toLog.contains("Closed connection to gateway.")) || logCertFailedBuffer[name].length() > 10000)
    {
        emit CertificateValidationFailed(name, logCertFailedBuffer[name]);
        logCertFailedBuffer[name] = "";
        logCertFailedMode[name] = false;
    }

    QDateTime currentDate = QDateTime::currentDateTime();
    out << currentDate.toString("MMM d hh:mm:ss").toStdString().c_str() << " " << toLog;
    logfile->flush();
}

void vpnLogger::procFinished(const QString &name)
{
    loggers.remove(name);
    logCertFailedMode.remove(name);
    logCertFailedBuffer.remove(name);
    vpnConfigs.remove(name);
    QFile *logfile = logfiles.value(name, nullptr);
    if(logfile != nullptr)
    {
        logfile->close();
        delete logfile;
    }
    logfiles.remove(name);
}

void vpnLogger::process()
{

}
