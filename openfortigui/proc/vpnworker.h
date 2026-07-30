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
#include <QMutex>
#include <QString>
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
    explicit vpnWorker(QObject *parent = nullptr);

    void setConfig(vpnProfile c);

    /*
     * The tunnel is a stack variable of process(), so it only lives while the
     * worker thread is inside process(). Everything the main thread needs from
     * it goes through these accessors: they read the pointer under the lock
     * and hand out copies only. process() resets it to nullptr before
     * returning, so nobody can ever get a dangling pointer.
     */
    bool    tunnelActive() const;
    int     tunnelState() const;        // -1 if no tunnel is alive
    QString tunnelPppIface() const;     // empty if no tunnel is alive

    /*
     * Stop signalling. The teardown has to run on the owning thread, so a stop
     * request is not carried out across threads. Instead process() is made to
     * return, exactly like an external SIGTERM would.
     */
    static void requestStop();      // set the flag without a signal
    static bool stopRequested();    // own flag or the io_loop() handler
    static bool stopSignalSafe();   // true if SIGTERM will not kill us outright

private:
    vpnProfile vpnConfig;

    mutable QMutex tunnel_mutex;
    struct tunnel *ptr_tunnel;

    void setTunnel(struct tunnel *tunnel);
    void updateStatus(vpnClientConnection::connectionStatus status);

signals:
    void statusChanged(vpnClientConnection::connectionStatus status);
    void finished();

public slots:
    void process();
    void end();
};

#endif // VPNWORKER_H
