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

#include "vpnapi.h"

#include "config.h"

#include <QVariant>
#include <QDir>
#include <QFile>
#include <QStandardPaths>

#include <unistd.h>

QString vpnApi::socket_path = QString();

vpnApi::vpnApi()
{

}

void vpnApi::setSocketPath(const QString &path)
{
    vpnApi::socket_path = path;
}

QString vpnApi::socketPath()
{
    if(!vpnApi::socket_path.isEmpty())
        return vpnApi::socket_path;

    QString dir = QStandardPaths::writableLocation(QStandardPaths::RuntimeLocation);
    if(dir.isEmpty())
    {
        // No XDG_RUNTIME_DIR: keep the socket per-uid so users cannot collide.
        dir = QString("/tmp/openfortigui-%1").arg(getuid());
        QDir().mkpath(dir);
        QFile::setPermissions(dir, QFileDevice::ReadOwner | QFileDevice::WriteOwner
                                   | QFileDevice::ExeOwner);
    }

    return QString("%1/%2").arg(dir, openfortigui_config::name);
}


QDataStream &operator<<(QDataStream &ds, const vpnApi &obj)
{
    ds << obj.action;
    ds << obj.objName;
    ds << obj.data;

    return ds;
}

QDataStream &operator>>(QDataStream &ds, vpnApi &obj)
{
    ds >> obj.action;
    ds >> obj.objName;
    ds >> obj.data;

    return ds;
}
