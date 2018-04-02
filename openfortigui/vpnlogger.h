#ifndef VPNLOGGER_H
#define VPNLOGGER_H

#include <QObject>
#include <QSignalMapper>
#include <QProcess>
#include <QMap>
#include <QFile>

#include "ticonfmain.h"

class vpnLogger : public QObject
{
    Q_OBJECT
public:
    explicit vpnLogger(QObject *parent = nullptr);
    ~vpnLogger();
    void addVPN(const QString &name, QProcess *proc);

private:
    QSignalMapper *logMapperStdout, *logMapperStderr;
    QMap<QString, QProcess*> loggers;
    QMap<QString, QFile*> logfiles;
    QMap<QString, bool> loglocker;
    QMap<QString, QByteArray*> logBuffer;
    tiConfMain main_settings;

private slots:
    void logStdout(const QString &name);
    void logStderr(const QString &name);

signals:
    void OTPRequest(QProcess *proc);

public slots:
    void process();
};

#endif // VPNLOGGER_H
