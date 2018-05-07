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
    vpnProfiles.readVpnProfiles();

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
            match.setIconName("openfortigui");

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
            match.setIconName("openfortigui");

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
        QThread::sleep(1);
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
