#include "vpnlogger.h"

#include <QDebug>
#include <QDateTime>
#include <QThread>

#include "ticonfmain.h"

vpnLogger::vpnLogger(QObject *parent) : QObject(parent)
{
    logMapperStdout = new QSignalMapper(this);
    loggers = QMap<QString, QProcess*>();
    logfiles = QMap<QString, QFile*>();
    loglocker = QMap<QString, bool>();

    connect(logMapperStdout, SIGNAL(mapped(QString)), this, SLOT(logVPNOutput(QString)));
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
    logMapperStdout->setMapping(proc, name);
}

void vpnLogger::logVPNOutput(const QString &name)
{
    QThread::msleep(200);

    QProcess *proc = loggers[name];

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

void vpnLogger::process()
{

}
