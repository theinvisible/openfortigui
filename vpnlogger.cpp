#include "vpnlogger.h"

#include <QDebug>

#include "ticonfmain.h"

vpnLogger::vpnLogger(QObject *parent) : QObject(parent)
{
    logMapper = new QSignalMapper(this);
    loggers = QMap<QString, QProcess*>();
    logfiles = QMap<QString, QFile*>();
    loglocker = QMap<QString, bool>();

    connect(logMapper, SIGNAL(mapped(QString)), this, SLOT(log(QString)));
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

    connect(proc, SIGNAL(readyReadStandardError()), logMapper, SLOT(map()));
    connect(proc, SIGNAL(readyReadStandardOutput()), logMapper, SLOT(map()));
    logMapper->setMapping(proc, name);
}

void vpnLogger::log(const QString &name)
{
    if(loglocker[name])
        return;

    loglocker[name] = true;

    QProcess *proc = loggers[name];

    QByteArray blog = proc->readAll();
    if(blog.length() == 0)
    {
        return;
    }

    QFile *logfile = logfiles[name];
    QTextStream out(logfile);

    QString toLog = QString::fromUtf8(blog);
    if(toLog.contains("Please") || toLog.contains("2factor authentication token:"))
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
