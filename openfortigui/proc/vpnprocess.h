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

#ifndef VPNPROCESS_H
#define VPNPROCESS_H

#include <QObject>
#include <QLocalSocket>
#include <QThread>

#include "config.h"
#include "vpnworker.h"
#include "vpnapi.h"

class vpnProcess : public QObject
{
    Q_OBJECT
public:
    explicit vpnProcess(QObject *parent = 0);
    void run(const QString &vpnname);

private:
    QString name;
    QLocalSocket *apiServer;
    QThread* thread_vpn;
    vpnWorker *thread_worker;
    QTimer *observer, *observerStats;
    struct tunnel last_tunnel;
    bool init_last_tunnel;

    bool cred_received, passstore_received;
    struct struct_cred_data
    {
        QString username;
        QString password;
    };
    struct_cred_data cred_data;

    vpnStats stats;

    void startVPN();
    void checkVPNSettings(vpnProfile *profile);
    void sendCMD(const vpnApi &cmd);
    void updateStats();

    void requestCred();
    void requestPassStore();
    void submitStats();
    void submitVPNMessage(const QString &msg, int msg_type);

signals:

public slots:
    void onServerReadyRead();
    void onServerDisconnected();
    void onVPNStatusChanged(vpnClientConnection::connectionStatus status);

private slots:
    void onObserverUpdate();
    void onStatsUpdate();
    void closeProcess();
};

#endif // VPNPROCESS_H
