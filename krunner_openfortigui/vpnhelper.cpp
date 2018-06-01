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

#include "vpnhelper.h"

#include <QProcess>
#include <QDebug>
#include <QLocalSocket>

#include "../openfortigui/vpnapi.h"
#include "../openfortigui/config.h"

vpnHelper::vpnHelper()
{

}

QString vpnHelper::formatByteUnits(qint64 num)
{
    if(num >= 1024*1024*1024)
        return QString("%1G").arg(QString::number((double)num / (1024*1024*1024), 'f', 2));
    else if(num >= 1024*1024)
        return QString("%1M").arg(QString::number((double)num / (1024*1024), 'f', 2));
    else if(num >= 1024)
        return QString("%1K").arg(QString::number((double)num / 1024, 'f', 2));
    else
        return QString("%1B").arg(num);
}

bool vpnHelper::isOpenFortiGUIRunning()
{
    QLocalSocket apiServerTest;
    apiServerTest.connectToServer(openfortigui_config::name);
    if(apiServerTest.waitForConnected(200))
    {
        QByteArray block;
        QDataStream out(&block, QIODevice::WriteOnly);
        out.setVersion(QDataStream::Qt_5_2);
        vpnApi apiData;
        apiData.objName = "ping";
        apiData.action = vpnApi::ACTION_PING;
        out << apiData;

        apiServerTest.write(block);
        apiServerTest.flush();
        apiServerTest.close();

        return true;
    }
    else
    {
        return false;
    }
}
