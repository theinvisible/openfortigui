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

#ifndef VPNWORKER_H
#define VPNWORKER_H

#include <QObject>
#include <QTimer>
#include "vpnprofile.h"
#include "vpnmanager.h"

extern "C"  {
#include "openfortivpn/src/config.h"
#include "openfortivpn/src/log.h"
#include "openfortivpn/src/tunnel.h"
}

class vpnWorker : public QObject
{
    Q_OBJECT
public:
    explicit vpnWorker(QObject *parent = 0);

    struct tunnel *ptr_tunnel;
    void setConfig(vpnProfile c);

private:
    vpnProfile vpnConfig;

    void updateStatus(vpnClientConnection::connectionStatus status);

signals:
    void statusChanged(vpnClientConnection::connectionStatus status);
    void finished();

public slots:
    void process();
    void end();
};

#endif // VPNWORKER_H
