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

#include <iostream>
#include <QtCore/QJsonDocument>
#include <QtCore/QJsonArray>
#include <QtCore/QDebug>
#include <QtCore/QDir>

#include <QLocalSocket>
#include <QThread>

#include "ticonfmain.h"
#include "vpnhelper.h"
#include "vpnapi.h"
#include "config.h"

#include "krunner_openfortigui.h"

#define KB_ASSERT(cond) {if(!(cond)) {qDebug().nospace() << "[" << script << "] Failed on " << #cond; return;}}
#define KB_ASSERT_MSG(cond, msg) {if(!(cond)) {qDebug().nospace() << "[" << script << "] " << msg; return;}}

Krunner_openfortigui::Krunner_openfortigui(QObject *parent, const KPluginMetaData &metaData)
  : KRunner::AbstractRunner(parent, metaData)
{
    addSyntax(QStringLiteral(":q:"), metaData.description());
}

void Krunner_openfortigui::match(KRunner::RunnerContext &ctxt)
{
    if (!ctxt.isValid())
        return;

    QString query = ctxt.query();
    tiConfVpnProfiles vpnProfiles;
    tiConfVpnGroups vpnGroups;
    vpnProfiles.readVpnProfiles();
    vpnGroups.readVpnGroups();
    QMap<QString, QVariant> matchData;

    QList<vpnProfile*> vpns = vpnProfiles.getVpnProfiles();
    for(int i=0; i < vpns.count(); i++)
    {
        vpnProfile *vpn = vpns.at(i);

        if(query == vpn->name.toLower())
        {
            KRunner::QueryMatch match(this);
            match.setText(vpn->name);
            match.setSubtext(vpn->gateway_host);
            matchData["type"] = Krunner_openfortigui::DATA_TYPE_VPN;
            matchData["data"] = vpn->name;
            match.setData(QVariant(matchData));
            match.setCategoryRelevance(KRunner::QueryMatch::CategoryRelevance::Highest);
            match.setRelevance(1.0);
            match.setMatchCategory("VPN");
            match.setIconName(QStringLiteral("openfortigui"));

            ctxt.addMatch(match);
        }
        else if(vpn->name.contains(query, Qt::CaseInsensitive))
        {
            KRunner::QueryMatch match(this);
            match.setText(vpn->name);
            match.setSubtext(vpn->gateway_host);
            matchData["type"] = Krunner_openfortigui::DATA_TYPE_VPN;
            matchData["data"] = vpn->name;
            match.setData(matchData);
            match.setCategoryRelevance(KRunner::QueryMatch::CategoryRelevance::Moderate);
            match.setRelevance(0.5);
            match.setMatchCategory("VPN");
            match.setIconName(QStringLiteral("openfortigui"));

            ctxt.addMatch(match);
        }
    }

    QList<vpnGroup*> vpngroups = vpnGroups.getVpnGroups();
    for(int i=0; i < vpngroups.count(); i++)
    {
        vpnGroup *vpngroup = vpngroups.at(i);

        if(query == vpngroup->name.toLower())
        {
            KRunner::QueryMatch match(this);
            match.setText(vpngroup->name);
            //vpnGroup *vpngroupi = vpnGroups.getVpnGroupByName(vpngroup->name);
            //qWarning() << vpngroupi->localMembers.join(", ");
            matchData["type"] = Krunner_openfortigui::DATA_TYPE_VPNGROUP;
            matchData["data"] = vpngroup->name;
            match.setData(matchData);
            match.setCategoryRelevance(KRunner::QueryMatch::CategoryRelevance::Highest);
            match.setRelevance(1.0);
            match.setMatchCategory("VPN Group");
            match.setIconName(QStringLiteral("openfortigui"));

            ctxt.addMatch(match);
        }
        else if(vpngroup->name.contains(query, Qt::CaseInsensitive))
        {
            KRunner::QueryMatch match(this);
            match.setText(vpngroup->name);
            //vpnGroup *vpngroupi = vpnGroups.getVpnGroupByName(vpngroup->name);
            //qWarning() << vpngroupi->localMembers.join(", ");
            matchData["type"] = Krunner_openfortigui::DATA_TYPE_VPNGROUP;
            matchData["data"] = vpngroup->name;
            match.setData(matchData);
            match.setCategoryRelevance(KRunner::QueryMatch::CategoryRelevance::Moderate);
            match.setRelevance(0.5);
            match.setMatchCategory("VPN Group");
            match.setIconName(QStringLiteral("openfortigui"));

            ctxt.addMatch(match);
        }
    }
}

void Krunner_openfortigui::run(const KRunner::RunnerContext &ctxt, const KRunner::QueryMatch &match)
{
    Q_UNUSED(ctxt)

    if(!vpnHelper::isOpenFortiGUIRunning())
    {
        QProcess::startDetached("openfortigui");

        bool sockConnected = false;
        int maxwait = 5000, curwait = 0;
        while(!sockConnected && curwait < maxwait)
        {
            QLocalSocket apiServerTest(this);
            apiServerTest.connectToServer(vpnApi::socketPath());
            if(apiServerTest.waitForConnected(200))
            {
                QByteArray block;
                QDataStream out(&block, QIODevice::WriteOnly);
                out.setVersion(QDataStream::Qt_6_0);
                vpnApi apiData;
                apiData.objName = "ping";
                apiData.action = vpnApi::ACTION_PING;
                out << apiData;

                apiServerTest.write(block);
                apiServerTest.flush();
                apiServerTest.close();

                sockConnected = true;
                continue;
            }
            else
            {
                curwait += 200;
                QThread::msleep(200);
            }

        }
    }

    QLocalSocket apiServer(this);
    apiServer.connectToServer(vpnApi::socketPath());
    if(apiServer.waitForConnected(1000))
    {
        QByteArray block;
        QDataStream out(&block, QIODevice::WriteOnly);
        out.setVersion(QDataStream::Qt_6_0);
        vpnApi apiData;
        switch(match.data().toMap()["type"].toInt())
        {
        case Krunner_openfortigui::DATA_TYPE_VPN:
            apiData.objName = match.data().toMap()["data"].toString();
            apiData.action = vpnApi::ACTION_VPN_START;
            break;

        case Krunner_openfortigui::DATA_TYPE_VPNGROUP:
            apiData.objName = match.data().toMap()["data"].toString();
            apiData.action = vpnApi::ACTION_VPNGROUP_START;
            break;

        default:
            return;
        }

        out << apiData;

        apiServer.write(block);
        apiServer.flush();
        apiServer.close();
    }
    else
    {
        qWarning() << apiServer.errorString();
    }

}

K_PLUGIN_CLASS_WITH_JSON(Krunner_openfortigui, "krunner_openfortigui.json")

#include "krunner_openfortigui.moc"
