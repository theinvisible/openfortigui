#include "vpnbarracuda.h"

#include <QTimer>

QString vpnBarracuda::conf_template = "BINDIP = \n"
    "CERTFILE = \n"
    "DNSFILEMODE = AUTO\n"
    "DYNSSA = 2\n"
    "HANDSHAKETIMEOUT = 10\n"
    "IPV6TUNNELING = YES\n"
    "KEEPALIVE = 10\n"
    "KEYFILE = \n"
    "MAXRECONNECT = 3\n"
    "OTPMODE = OFF\n"
    "PROXYADDR = \n"
    "PROXYPORT = 8080\n"
    "PROXYTYPE = NO PROXY\n"
    "PROXYUSER = \n"
    "SERVER = %1\n"
    "SERVERPORT = 691\n"
    "SPECIAL = NONE\n"
    "TAP = /dev/net/tun\n"
    "TUNNELENC = AES128-MD5\n"
    "TUNNELMODE = UDP\n"
    "TUNNELREKEY = 20\n"
    "WRITEDNS = MERGE\n"
    "\n"
    ".\n"
    "\n";

vpnBarracuda::vpnBarracuda(QObject *parent)
    : QObject{parent}
{
    statsupdater = 0;
}

void vpnBarracuda::start(const QString &vpnname, vpnClientConnection *conn)
{
    this->vpnname = vpnname;
    client_con = conn;

    tiConfVpnProfiles profiles;
    vpn_profile = *profiles.getVpnProfileByName(vpnname);

    QString vpn_conf_file = QString("/tmp/%1").arg(vpn_profile.name);
    QFile file(vpn_conf_file);
    if (!file.open(QIODevice::WriteOnly | QIODevice::Text))
        return;

    QTextStream out(&file);
    out << conf_template.arg(vpn_profile.gateway_host);

    QString pass = vpn_profile.readPassword();
    QStringList arguments;
    arguments << "--start";
    arguments << "--login";
    arguments << vpn_profile.username;
    arguments << "--serverpwd";
    arguments << pass;
    arguments << "--config";
    arguments << vpn_conf_file;
    pass = "";

    emit VPNStatusChanged(vpn_profile.name, vpnClientConnection::STATUS_CONNECTING);
    QProcess *vpnProc = new QProcess(this);
    vpnProc->setProcessChannelMode(QProcess::MergedChannels);
    qDebug() << "Start vpn::" << vpn_profile.name;
    vpnProc->start("barracudavpn", arguments);
    vpnProc->waitForStarted();
    vpnProc->waitForFinished();
    QString pout = vpnProc->readAllStandardOutput();
    qDebug() << pout;
    if(pout.contains("failed")) {
        qInfo() << "failed";
        emit VPNStatusChanged(vpn_profile.name, vpnClientConnection::STATUS_DISCONNECTED);
        client_con->status = vpnClientConnection::STATUS_DISCONNECTED;
    } else {
        qInfo() << "success";

        statsupdater = new QTimer(this);
        connect(statsupdater, &QTimer::timeout, this, &vpnBarracuda::statusCheck);
        statsupdater->start(2000);
    }
}

void vpnBarracuda::stop()
{
    QStringList arguments;
    arguments << "--stop";

    QProcess *vpnProc = new QProcess(this);
    qDebug() << "Stop vpn::" << vpn_profile.name;
    vpnProc->start("barracudavpn", arguments);
    vpnProc->waitForStarted();
    vpnProc->waitForFinished();

    emit VPNStatusChanged(vpn_profile.name, vpnClientConnection::STATUS_DISCONNECTED);
    client_con->status = vpnClientConnection::STATUS_DISCONNECTED;
}

void vpnBarracuda::statusCheck()
{
    QStringList arguments;
    arguments << "--status";

    QProcess *vpnProc = new QProcess(this);
    vpnProc->setProcessChannelMode(QProcess::MergedChannels);
    vpnProc->start("barracudavpn", arguments);
    vpnProc->waitForStarted();
    vpnProc->waitForFinished();
    QString pout = vpnProc->readAllStandardOutput();
    if(pout.contains("Status:      CONNECTED")) {
        emit VPNStatusChanged(vpn_profile.name, vpnClientConnection::STATUS_CONNECTED);
        client_con->status = vpnClientConnection::STATUS_CONNECTED;

        vpnStats stats;

        QFile file("/proc/net/dev");
        if(!file.open(QIODevice::ReadOnly | QIODevice::Text))
            return;

        QTextStream in(&file);
        QString line = in.readLine();
        QStringList lineParse;
        QRegExp reParse = QRegExp("^\\S{1,}$");
        while (!line.isNull())
        {
            lineParse = line.split(" ").filter(reParse);
            if(lineParse[0].left(lineParse[0].length() - 1).contains("tun"))
            {
                stats.bytes_read = lineParse[1].toLongLong();
                stats.bytes_written = lineParse[9].toLongLong();
                file.close();
            }

            line = in.readLine();
        }
        file.close();
        emit VPNStatsUpdate(vpn_profile.name, stats);
    } else {
        emit VPNStatusChanged(vpn_profile.name, vpnClientConnection::STATUS_DISCONNECTED);
        client_con->status = vpnClientConnection::STATUS_DISCONNECTED;
    }
}