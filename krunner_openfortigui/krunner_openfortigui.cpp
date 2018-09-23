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

#include <QMessageBox>
#include <QLocalSocket>
#include <QThread>

#include "ticonfmain.h"
#include "vpnhelper.h"
#include "../openfortigui/vpnapi.h"
#include "../openfortigui/config.h"

#include "krunner_openfortigui.h"

#define KB_ASSERT(cond) {if(!(cond)) {qDebug().nospace() << "[" << script << "] Failed on " << #cond; return;}}
#define KB_ASSERT_MSG(cond, msg) {if(!(cond)) {qDebug().nospace() << "[" << script << "] " << msg; return;}}

Krunner_openfortigui::Krunner_openfortigui(QObject* parent, const QVariantList& args)
  : Plasma::AbstractRunner(parent, args)
{
    setSpeed(AbstractRunner::NormalSpeed);
    setPriority(HighestPriority);
    setHasRunOptions(true);

    setDefaultSyntax(Plasma::RunnerSyntax(QString::fromLatin1(":q:"), metadata().comment()));
}

void Krunner_openfortigui::match(Plasma::RunnerContext& ctxt)
{
    if (!ctxt.isValid())
        return;

    QString query = ctxt.query();
    tiConfVpnProfiles vpnProfiles;
    tiConfVpnGroups vpnGroups;
    vpnProfiles.readVpnProfiles();
    vpnGroups.readVpnGroups();

    QList<vpnProfile*> vpns = vpnProfiles.getVpnProfiles();
    for(int i=0; i < vpns.count(); i++)
    {
        vpnProfile *vpn = vpns.at(i);

        if(query == vpn->name.toLower())
        {
            Plasma::QueryMatch match(this);
            match.setText(vpn->name);
            match.setSubtext(vpn->gateway_host);
            match.setData(vpn->name);
            match.setType(Plasma::QueryMatch::ExactMatch);
            match.setRelevance(1.0);
            match.setMatchCategory("VPN");
            match.setIcon(QIcon("/usr/share/pixmaps/openfortigui.png"));
            // Framework >5.24 only
            // match.setIconName("openfortigui");

            ctxt.addMatch(match);
        }
        else if(vpn->name.contains(query, Qt::CaseInsensitive))
        {
            Plasma::QueryMatch match(this);
            match.setText(vpn->name);
            match.setSubtext(vpn->gateway_host);
            match.setData(vpn->name);
            match.setType(Plasma::QueryMatch::CompletionMatch);
            match.setRelevance(0.5);
            match.setMatchCategory("VPN");
            match.setIcon(QIcon("/usr/share/pixmaps/openfortigui.png"));
            // Framework >5.24 only
            // match.setIconName("openfortigui");

            ctxt.addMatch(match);
        }
    }

    QList<vpnGroup*> vpngroups = vpnGroups.getVpnGroups();
    for(int i=0; i < vpngroups.count(); i++)
    {
        vpnGroup *vpngroup = vpngroups.at(i);

        if(query == vpngroup->name.toLower())
        {
            Plasma::QueryMatch match(this);
            match.setText(vpngroup->name);
            match.setData(vpngroup->name);
            match.setType(Plasma::QueryMatch::ExactMatch);
            match.setRelevance(1.0);
            match.setMatchCategory("VPN Group");
            match.setIcon(QIcon("/usr/share/pixmaps/openfortigui.png"));
            // Framework >5.24 only
            // match.setIconName("openfortigui");

            ctxt.addMatch(match);
        }
        else if(vpngroup->name.contains(query, Qt::CaseInsensitive))
        {
            Plasma::QueryMatch match(this);
            match.setText(vpngroup->name);
            match.setData(vpngroup->name);
            match.setType(Plasma::QueryMatch::CompletionMatch);
            match.setRelevance(0.5);
            match.setMatchCategory("VPN Group");
            match.setIcon(QIcon("/usr/share/pixmaps/openfortigui.png"));
            // Framework >5.24 only
            // match.setIconName("openfortigui");

            ctxt.addMatch(match);
        }
    }
}

void Krunner_openfortigui::run(const Plasma::RunnerContext& ctxt, const Plasma::QueryMatch& match)
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
    apiServer.connectToServer(openfortigui_config::name);
    if(apiServer.waitForConnected(1000))
    {
        QByteArray block;
        QDataStream out(&block, QIODevice::WriteOnly);
        out.setVersion(QDataStream::Qt_5_2);
        vpnApi apiData;
        apiData.objName = match.data().toString();
        apiData.action = vpnApi::ACTION_VPN_START;
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

K_EXPORT_PLASMA_RUNNER(krunner_openfortigui, Krunner_openfortigui)

#include "krunner_openfortigui.moc"
