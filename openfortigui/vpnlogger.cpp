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

#include "ticonfmain.h"

vpnLogger::vpnLogger(QObject *parent) : QObject(parent)
{
    flushTimer = nullptr;
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

    /*
     * The process belongs to the main thread, so it is read there and only the
     * bytes travel to this one. Reading it from the logger thread is what a
     * QProcess must never be subjected to: while this thread walked the ring
     * buffer, the main thread's socket notifier appended to it, and under load --
     * a chatty child, a big download -- the buffer got corrupted and the GUI
     * died in QRingBuffer::read(). Reported as PR #207.
     *
     * The lambda has the process as its context object, which is what puts it in
     * the main thread; the hop back here is a queued invocation.
     */
    connect(proc, &QProcess::readyReadStandardOutput, proc, [this, name, proc]() {
        const QByteArray data = proc->readAllStandardOutput();
        if(data.isEmpty())
            return;
        QMetaObject::invokeMethod(this, [this, name, data]() { logVPNData(name, data); },
                                  Qt::QueuedConnection);
    });
    connect(proc, &QProcess::finished, proc, [this, name]() {
        QMetaObject::invokeMethod(this, [this, name]() { procFinished(name); },
                                  Qt::QueuedConnection);
    });
}

/*
 * Collect what arrives and process it in one piece a moment later.
 *
 * The patterns below are matched with contains() on a chunk of output, so a line
 * split across two reads would match nothing -- that is how the passphrase prompt
 * used to be missed (issue #166). The old code coalesced by sleeping 200 ms in
 * the logger thread before reading; the timer does the same without blocking a
 * thread and without holding a lock on someone else's ring buffer.
 *
 * The timer is not restarted while it is running, so continuous output cannot
 * starve the buffer: it is emptied at least every 150 ms. Prompts matter here --
 * the child writes them without a trailing newline and then waits on stdin, so
 * waiting for a complete line would wait forever.
 */
void vpnLogger::logVPNData(const QString &name, const QByteArray &data)
{
    pending[name].append(data);

    /*
     * A child that outruns the logger must not make the chunks arbitrarily large:
     * each one is turned into a QString and scanned by a dozen contains(), so the
     * work per flush should stay bounded. This is not backpressure -- there never
     * was any, QProcess buffers without a limit unless setReadBufferSize() says
     * otherwise -- it just keeps one round of work a round of work.
     */
    if(pending[name].size() >= 1024 * 1024)
    {
        processOutput(name, pending.take(name));
        return;
    }

    if(flushTimer == nullptr)
    {
        // Created here so it belongs to the logger thread, not to the one that
        // constructed this object.
        flushTimer = new QTimer(this);
        flushTimer->setSingleShot(true);
        connect(flushTimer, &QTimer::timeout, this, &vpnLogger::flushPending);
    }

    if(!flushTimer->isActive())
        flushTimer->start(150);
}

void vpnLogger::flushPending()
{
    const QStringList names = pending.keys();
    for(const QString &name : names)
    {
        const QByteArray data = pending.take(name);
        if(!data.isEmpty())
            processOutput(name, data);
    }
}

void vpnLogger::processOutput(const QString &name, const QByteArray &data)
{
    QFile *logfile = logfiles.value(name, nullptr);
    if(logfile == nullptr)
        return;

    QTextStream out(logfile);

    QString toLog = QString::fromUtf8(data);

    /*
     * sudo could not run us without asking for a password, which means the
     * NOPASSWD rule did not apply -- usually because the user is not in the
     * group the rule names. This was the single most reported problem
     * (issues #133, #167, #193, #203) and always needed a maintainer to
     * explain it, because the raw sudo message says nothing about openfortiGUI.
     */
    if(toLog.contains("a terminal is required to read the password")
       || toLog.contains("a password is required"))
    {
        vpnMsg msg;
        msg.msg = tr("Error: sudo asked for a password, so the VPN process could not be started.");
        msg.detail = tr("openfortiGUI needs a sudo rule that allows starting the VPN process "
                        "without a password. Check that /etc/sudoers.d/openfortigui exists, that "
                        "/etc/sudoers includes that directory, and that your user is a member of "
                        "the group the rule names (\"sudo\" by default -- domain/AD users usually "
                        "are not, so the rule has to name their group instead).\n\n%1").arg(toLog);
        msg.type = vpnMsg::TYPE_ERROR;
        emit VPNMessage(name, msg);

        out << toLog;
        logfile->flush();
        return;
    }

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

    /*
     * The gateway's prompts are only visible as text on the child's stdout, so
     * they are recognised by pattern. openfortivpn asks for the private key
     * passphrase the same way (pem_passphrase_cb in tunnel.c), which is why
     * profiles with an encrypted client key used to hang forever: no pattern
     * matched, no dialog appeared, and the process waited on stdin (issue #166).
     */
    int prompt = -1;
    if(toLog.contains("PEM pass phrase"))
    {
        prompt = vpnLogger::PROMPT_PEM_PASSPHRASE;
    }
    else if(vpnConfigs[name].otp_prompt.isEmpty())
    {
        if(toLog.contains("Please") ||
           toLog.contains("2factor authentication token:") ||
           toLog.contains("Two-factor authentication") ||
           toLog.contains("one-time password"))
        {
            prompt = vpnLogger::PROMPT_OTP;
        }
    } else {
        if(toLog.contains(vpnConfigs[name].otp_prompt))
        {
            prompt = vpnLogger::PROMPT_OTP;
        }
    }

    if(prompt >= 0)
    {
        /*
         * The answer is written to the process by the dialog, in the main thread.
         * Only the pointer travels; a process that has ended in the meantime
         * leaves a null QPointer instead of a dangling one.
         */
        QPointer<QProcess> proc = loggers.value(name);
        if(!proc.isNull())
            emit PromptRequest(proc, prompt);
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
    out << currentDate.toString("MMM d hh:mm:ss") << " " << toLog;
    logfile->flush();
}

/*
 * Everything belonging to this VPN goes away with the process. The old version
 * only set the entry to null, so the log file stayed open and every map kept its
 * row -- one more of each per connection, for the lifetime of the GUI.
 */
void vpnLogger::procFinished(const QString &name)
{
    // Whatever arrived last still belongs in the log.
    if(pending.contains(name))
        processOutput(name, pending.take(name));

    loggers.remove(name);
    logCertFailedMode.remove(name);
    logCertFailedBuffer.remove(name);
    vpnConfigs.remove(name);

    QFile *logfile = logfiles.take(name);
    if(logfile != nullptr)
    {
        logfile->close();
        delete logfile;
    }
}

void vpnLogger::process()
{

}
