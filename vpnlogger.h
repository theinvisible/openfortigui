#ifndef VPNLOGGER_H
#define VPNLOGGER_H

#include <QObject>
#include <QSignalMapper>
#include <QProcess>
#include <QMap>

#include "ticonfmain.h"

class vpnLogger : public QObject
{
    Q_OBJECT
public:
    explicit vpnLogger(QObject *parent = nullptr);
    void addVPN(const QString &name, QProcess *proc);

private:
    QSignalMapper *logMapper;
    QMap<QString, QProcess*> loggers;
    tiConfMain main_settings;

private slots:
    void log(const QString &name);

signals:

public slots:
    void process();
};

#endif // VPNLOGGER_H
