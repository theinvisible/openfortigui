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

#ifndef VPNMANAGER_H
#define VPNMANAGER_H

#include "vpnapi.h"

#include <QObject>
#include <QLocalSocket>
#include <QLocalServer>
#include <QStandardItem>
#include <QProcess>

#include <vpnlogger.h>

class vpnClientConnection : public QObject
{
    Q_OBJECT
public:

    explicit vpnClientConnection(const QString &n, QObject *parent = 0);

    enum connectionStatus
    {
        STATUS_DISCONNECTED = 0,
        STATUS_CONNECTING,
        STATUS_CONNECTED
    };

    connectionStatus status;
    QProcess *proc;

    void setSocket(QLocalSocket *sock);
    void sendCMD(const vpnApi &cmd);

private:
    QString name;
    QLocalSocket *socket;

    void submitPassStoreCred();

signals:
    void VPNStatusChanged(QString vpnname, vpnClientConnection::connectionStatus status);
    void VPNCredRequest(QString vpnname);
    void VPNStatsUpdate(QString vpnname, vpnStats stats);
    void VPNMessage(QString vpnname, vpnMsg msg);

public slots:
    void onClientReadyRead();
    void onClientDisconnected();
};

class vpnManager : public QObject
{
    Q_OBJECT
public:

    explicit vpnManager(QObject *parent = 0);
    ~vpnManager();

    void startVPN(const QString &name);
    void stopVPN(const QString &name);

    vpnClientConnection *getClientConnection(const QString &name);
    void submitVPNCred(const QString &vpnname, const QString &username, const QString &password);

    void requestStats(const QString &vpnname);
    bool isSomeClientConnected();

private:

    QLocalServer *server;
    QMap<QString, vpnClientConnection*> connections;
    QThread *logger_thread;
    vpnLogger *logger;

signals:
    void VPNStatusChanged(QString vpnname, vpnClientConnection::connectionStatus status);
    void VPNCredRequest(QString vpnname);
    void VPNStatsUpdate(QString vpnname, vpnStats stats);
    void VPNMessage(QString vpnname, vpnMsg msg);
    void VPNOTPRequest(QProcess *proc);
    void VPNCertificateValidationFailed(QString vpnname, QString buffer);
    void VPNShowMainWindowRequest();

    void addVPNLogger(const QString &name, QProcess *proc);

public slots:
    void onClientConnected();
    void onClientVPNStatusChanged(QString vpnname, vpnClientConnection::connectionStatus status);
    void onClientVPNCredRequest(QString vpnname);
    void onClientVPNStatsUpdate(QString vpnname, vpnStats stats);
    void onClientVPNMessage(QString vpnname, vpnMsg msg);
    void onOTPRequest(QProcess *proc);
    void onCertificateValidationFailed(QString vpnname, QString buffer);
};

#endif // VPNMANAGER_H
