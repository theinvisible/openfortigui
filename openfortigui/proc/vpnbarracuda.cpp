#include "vpnbarracuda.h"

#include <QTimer>
#include <QDir>

QString vpnBarracuda::conf_template = "BINDIP = \n"
    "CERTFILE = \n"
    "DNSFILEMODE = AUTO\n"
    "DYNSSA = 2\n"
    "HANDSHAKETIMEOUT = 10\n"
    "IPV6TUNNELING = YES\n"
    "KEEPALIVE = 10\n"
    "KEYFILE = \n"
    "MAXRECONNECT = 3\n"
    "OTPMODE = %2\n"
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
    "WRITEDNS = NO\n"
    "\n"
    ".\n"
    "\n";

vpnBarracuda::vpnBarracuda(QObject *parent)
    : QObject{parent}
{
    statsupdater = 0;
}

void vpnBarracuda::start(const QString &vpnname, vpnClientConnection *conn, const QString &otptoken)
{
    this->vpnname = vpnname;
    client_con = conn;

    tiConfVpnProfiles profiles;
    vpn_profile = *profiles.getVpnProfileByName(vpnname);

    QDir logsdir_vpn_path("/tmp/ovpn_gui/ca");
    logsdir_vpn_path.mkpath("/tmp/ovpn_gui/ca");

    QString vpn_conf_file = QString("/tmp/ovpn_gui/barracudavpn.conf");
    QFile file(vpn_conf_file);
    if (!file.open(QIODevice::WriteOnly | QIODevice::Text))
        return;

    QTextStream out(&file);
    out << conf_template.arg(vpn_profile.gateway_host, (vpn_profile.always_ask_otp) ? "STATIC" : "OFF");
    out.flush();

    QString pass = vpn_profile.readPassword();
    QStringList arguments;
    arguments << "--start";
    arguments << "--login";
    arguments << vpn_profile.username;
    arguments << "--serverpwd";
    arguments << pass;
    if(vpn_profile.always_ask_otp) {
        arguments << "--onetimepwd";
        arguments << otptoken;
    }
    arguments << "--config";
    arguments << "/tmp/ovpn_gui/";
    if(vpn_profile.debug)
        arguments << "--verbose";
    pass = "";

    emit VPNStatusChanged(vpn_profile.name, vpnClientConnection::STATUS_CONNECTING);
    client_con->status = vpnClientConnection::STATUS_CONNECTING;
    QProcess *vpnProc = new QProcess(this);
    emit addVPNLogger(vpnname, vpnProc);
    vpnProc->setProcessChannelMode(QProcess::MergedChannels);
    qDebug() << "Start vpn::" << vpn_profile.name;
    connect(vpnProc, QOverload<int, QProcess::ExitStatus>::of(&QProcess::finished), this, [=](int exitCode, QProcess::ExitStatus exitStatus) {
        QString pout = vpnProc->readAllStandardOutput();
        if(pout.contains("failed")) {
            emit VPNStatusChanged(vpn_profile.name, vpnClientConnection::STATUS_DISCONNECTED);
            client_con->status = vpnClientConnection::STATUS_DISCONNECTED;
        } else {
            statsupdater = new QTimer(this);
            connect(statsupdater, &QTimer::timeout, this, &vpnBarracuda::statusCheck);
            statsupdater->start(2000);
        }
    });
    vpnProc->start("barracudavpn", arguments);
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
    if(statsupdater != 0)
        statsupdater->stop();

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
        if(client_con->status != vpnClientConnection::STATUS_CONNECTED) {
            client_con->status = vpnClientConnection::STATUS_CONNECTED;
            emit VPNStatusChanged(vpn_profile.name, vpnClientConnection::STATUS_CONNECTED);
        }

        vpnStats stats = { 0, 0, 0 };

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
        statsupdater->stop();
    }
}
