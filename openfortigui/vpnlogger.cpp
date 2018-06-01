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

#include "ticonfmain.h"

vpnLogger::vpnLogger(QObject *parent) : QObject(parent)
{
    logMapperStdout = new QSignalMapper(this);
    logMapperFinished = new QSignalMapper(this);
    loggers = QMap<QString, QProcess*>();
    logfiles = QMap<QString, QFile*>();
    loglocker = QMap<QString, bool>();

    connect(logMapperStdout, SIGNAL(mapped(QString)), this, SLOT(logVPNOutput(QString)));
    connect(logMapperFinished, SIGNAL(mapped(QString)), this, SLOT(procFinished(QString)));
}

vpnLogger::~vpnLogger()
{

}

void vpnLogger::addVPN(const QString &name, QProcess *proc)
{
    qDebug() << "add logger" << tiConfMain::main_config;
    loggers.insert(name, proc);
    loglocker.insert(name, false);
    if(!logfiles.contains(name))
    {
        QFile *file = new QFile(QString("%1/vpn/%2.log").arg(tiConfMain::formatPath(main_settings.getValue("paths/logs").toString()), name));
        file->open(QIODevice::Append | QIODevice::Text);
        logfiles.insert(name, file);
    }

    connect(proc, SIGNAL(readyReadStandardOutput()), logMapperStdout, SLOT(map()));
    connect(proc, SIGNAL(finished(int)), logMapperFinished, SLOT(map()));
    logMapperStdout->setMapping(proc, name);
    logMapperFinished->setMapping(proc, name);
}

void vpnLogger::logVPNOutput(const QString &name)
{
    QThread::msleep(200);

    QProcess *proc = loggers[name];

    if(proc == 0)
        return;

    if(proc->bytesAvailable() == 0 && proc->isReadable())
        return;

    qDebug() << QDateTime::currentMSecsSinceEpoch() << "bytes avail::" << proc->bytesAvailable();

    QByteArray blog;
    blog.append(proc->read(proc->bytesAvailable()));

    QFile *logfile = logfiles[name];
    QTextStream out(logfile);

    QString toLog = QString::fromUtf8(blog);
    if(toLog.contains("Please") ||
       toLog.contains("2factor authentication token:") ||
       toLog.contains("Two-factor authentication"))
    {
        emit OTPRequest(proc);
    }

    out << toLog;
    logfile->flush();
}

void vpnLogger::procFinished(const QString &name)
{
    loggers[name] = 0;
}

void vpnLogger::process()
{

}
