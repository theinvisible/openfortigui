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

#ifndef VPNLOGGER_H
#define VPNLOGGER_H

#include <QObject>
#include <QSignalMapper>
#include <QProcess>
#include <QMap>
#include <QFile>

#include "ticonfmain.h"
#include "vpnapi.h"

class vpnLogger : public QObject
{
    Q_OBJECT
public:
    explicit vpnLogger(QObject *parent = nullptr);
    ~vpnLogger();

public slots:
    void addVPN(const QString &name, QProcess *proc);

private:
    QSignalMapper *logMapperStdout, *logMapperFinished;
    QMap<QString, QProcess*> loggers;
    QMap<QString, QFile*> logfiles;
    QMap<QString, bool> loglocker;
    QMap<QString, bool> logCertFailedMode;
    QMap<QString, QString> logCertFailedBuffer;
    QMap<QString, vpnProfile> vpnConfigs;
    tiConfMain main_settings;

private slots:
    void logVPNOutput(const QString &name);
    void procFinished(const QString &name);

signals:
    void OTPRequest(QProcess *proc);
    void CertificateValidationFailed(QString name, QString buffer);
    void VPNMessage(QString name, vpnMsg msg);

public slots:
    void process();
};

#endif // VPNLOGGER_H
