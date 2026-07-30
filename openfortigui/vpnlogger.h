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
#include <QProcess>
#include <QPointer>
#include <QMap>
#include <QFile>
#include <QTimer>

#include "ticonfmain.h"
#include "vpnapi.h"

class vpnLogger : public QObject
{
    Q_OBJECT
public:
    explicit vpnLogger(QObject *parent = nullptr);
    ~vpnLogger();

    /*
     * What the child process is waiting for on stdin. Recognised from its
     * output, because that is the only channel there is.
     */
    enum promptType
    {
        PROMPT_OTP = 0,
        PROMPT_PEM_PASSPHRASE
    };

public slots:
    void addVPN(const QString &name, QProcess *proc);

private:
    QMap<QString, QPointer<QProcess>> loggers;
    QMap<QString, QFile*> logfiles;
    QMap<QString, bool> logCertFailedMode;
    QMap<QString, QString> logCertFailedBuffer;
    QMap<QString, vpnProfile> vpnConfigs;
    tiConfMain main_settings;

    // Output that has arrived but is not processed yet, see logVPNData().
    QMap<QString, QByteArray> pending;
    QTimer *flushTimer;

    void processOutput(const QString &name, const QByteArray &data);

private slots:
    void logVPNData(const QString &name, const QByteArray &data);
    void flushPending();
    void procFinished(const QString &name);

signals:
    void PromptRequest(QProcess *proc, int type);
    void CertificateValidationFailed(QString name, QString buffer);
    void VPNMessage(QString name, vpnMsg msg);

public slots:
    void process();
};

#endif // VPNLOGGER_H
