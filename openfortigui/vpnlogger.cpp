#include "vpnlogger.h"

#include <QDebug>

#include "ticonfmain.h"

vpnLogger::vpnLogger(QObject *parent) : QObject(parent)
{
    logMapperStdout = new QSignalMapper(this);
    logMapperStderr = new QSignalMapper(this);
    loggers = QMap<QString, QProcess*>();
    logfiles = QMap<QString, QFile*>();
    loglocker = QMap<QString, bool>();
    logBuffer = QMap<QString, QByteArray*>();

    connect(logMapperStdout, SIGNAL(mapped(QString)), this, SLOT(logStdout(QString)));
    connect(logMapperStderr, SIGNAL(mapped(QString)), this, SLOT(logStderr(QString)));
}

vpnLogger::~vpnLogger()
{

}

void vpnLogger::addVPN(const QString &name, QProcess *proc)
{
    qDebug() << "add logger" << tiConfMain::main_config;
    loggers.insert(name, proc);
    loglocker.insert(name, false);
    logBuffer.insert(name, new QByteArray());
    if(!logfiles.contains(name))
    {
        QFile *file = new QFile(QString("%1/vpn/%2.log").arg(tiConfMain::formatPath(main_settings.getValue("paths/logs").toString()), name));
        file->open(QIODevice::Append | QIODevice::Text);
        logfiles.insert(name, file);
    }

    connect(proc, SIGNAL(readyReadStandardOutput()), logMapperStdout, SLOT(map()));
    //connect(proc, SIGNAL(readyReadStandardError()), logMapperStderr, SLOT(map()));
    logMapperStdout->setMapping(proc, name);
    logMapperStderr->setMapping(proc, name);
}

void vpnLogger::logStdout(const QString &name)
{
    if(loglocker[name])
        return;

    loglocker[name] = true;

    QProcess *proc = loggers[name];

    qDebug() << "bytes avail::" << proc->bytesAvailable();

    QByteArray blog = proc->readAllStandardOutput();
    if(blog.length() == 0)
    {
        loglocker[name] = false;
        return;
    }

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

    loglocker[name] = false;
}

void vpnLogger::logStderr(const QString &name)
{
    if(loglocker[name])
        return;

    loglocker[name] = true;

    QProcess *proc = loggers[name];

    QByteArray blog = proc->readAllStandardError();
    if(blog.length() == 0)
    {
        loglocker[name] = false;
        return;
    }

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

    loglocker[name] = false;
}

void vpnLogger::process()
{

}
