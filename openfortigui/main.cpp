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

#include "mainwindow.h"

#include "config.h"
#include "ticonfmain.h"
#include "proc/vpnprocess.h"
#include "vpnmanager.h"

#include <QApplication>
#include <QCoreApplication>
#include <QTextStream>
#include <QCommandLineParser>
#include <QtDebug>
#include <QFile>
#include <QDateTime>
#include <QProcess>
#include <QFileInfo>
#include <QLibraryInfo>
#include <QTranslator>
#include <QMessageBox>
#include <QLocalSocket>

QFile *openfortiguiLog = nullptr;

void logMessageOutput(QtMsgType type, const QMessageLogContext &, const QString & str)
{
    tiConfMain main_settings;
    QTextStream sout(stdout);

    if(openfortiguiLog == nullptr)
    {
        openfortiguiLog = new QFile(QString("%1/openfortigui.log").arg(tiConfMain::formatPath(main_settings.getValue("paths/logs").toString())));
        openfortiguiLog->open(QIODevice::Append | QIODevice::Text);
    }

    bool tidebug = main_settings.getValue("main/debug").toBool();

    QTextStream out(openfortiguiLog);
    QDateTime currentDate = QDateTime::currentDateTime();

    switch (type) {
    case QtDebugMsg:
        if(tidebug == true)
            out << currentDate.toString("MMM d hh:mm:ss") << " openfortiGUI::Debug: " << str << "\n";
        break;
    case QtWarningMsg:
        out << currentDate.toString("MMM d hh:mm:ss") << " openfortiGUI::Warning: " << str << "\n";
        break;
    case QtCriticalMsg:
        out << currentDate.toString("MMM d hh:mm:ss") << " openfortiGUI::Critical: " << str << "\n";
        break;
    case QtInfoMsg:
        out << currentDate.toString("MMM d hh:mm:ss") << " openfortiGUI::Info: " << str << "\n";
        sout << currentDate.toString("MMM d hh:mm:ss") << " openfortiGUI::Info: " << str << "\n";
        break;
    case QtFatalMsg:
        out << currentDate.toString("MMM d hh:mm:ss") << " openfortiGUI::Fatal: " << str << "\n";
        openfortiguiLog->flush();
        abort();
    }

    openfortiguiLog->flush();
}

bool isRunningAlready()
{
    QStringList arguments;
    arguments << "-A";

    QProcess *ch = new QProcess();
    ch->start("ps", arguments);
    ch->waitForFinished(5000);
    ch->waitForReadyRead(5000);
    QString line;
    int count = 0;
    while(!ch->atEnd())
    {
        line = QString::fromLatin1(ch->readLine());
        if(line.contains(QFileInfo(QCoreApplication::applicationFilePath()).fileName()))
            count++;
    }
    delete ch;

    return (count > 1) ? true : false;
}

/*
 * Pull --main-config and --api-socket out of argv before anything else runs.
 *
 * QCommandLineParser needs a QCoreApplication, and by the time we have one the
 * damage is done: the first qDebug() opens the log file and the first tiConfMain
 * constructor calls initMainConf(), both derived from HOME. In the VPN child
 * process HOME is root's, so the log and a stray directory tree used to end up
 * in /root/.openfortigui while the profiles were read from the user's home.
 * Both paths follow main_config, so it has to be set first.
 */
static void applyEarlyArgs(int argc, char *argv[])
{
    auto value = [argc, argv](int i, const char *name) -> QString {
        const QString arg = QString::fromLocal8Bit(argv[i]);
        const QString opt = QString("--%1").arg(name);

        if(arg == opt && i + 1 < argc)
            return QString::fromLocal8Bit(argv[i + 1]);
        if(arg.startsWith(opt + "="))
            return arg.mid(opt.length() + 1);

        return QString();
    };

    for(int i = 1; i < argc; i++)
    {
        const QString mainconfig = value(i, "main-config");
        if(!mainconfig.isEmpty())
            tiConfMain::setMainConfig(mainconfig);

        const QString apisocket = value(i, "api-socket");
        if(!apisocket.isEmpty())
            vpnApi::setSocketPath(apisocket);
    }
}

/*
 * Which of the two modes main() runs in. This used to be "argc > 1", which made
 * any option imply the command line: "openfortigui --main-config /path" exited
 * with 0 without doing anything at all, instead of starting the GUI against that
 * config. Only these four options actually ask for something other than the GUI.
 */
static bool cliMode(int argc, char *argv[])
{
    static const char *cliOptions[] = {
        "--start-vpn", "--kill-vpn-processes", "--help", "-h", "--version", "-v"
    };

    for(int i = 1; i < argc; i++)
    {
        const QString arg = QString::fromLocal8Bit(argv[i]);
        for(const char *opt : cliOptions)
            if(arg == QLatin1String(opt))
                return true;
    }

    return false;
}

int main(int argc, char *argv[])
{
    applyEarlyArgs(argc, argv);

    qInstallMessageHandler(logMessageOutput);

    qRegisterMetaType<vpnClientConnection::connectionStatus>("vpnClientConnection::connectionStatus");
    qRegisterMetaType<vpnStats>("vpnStats");
    qRegisterMetaType<vpnMsg>("vpnMsg");

    tiConfMain main_settings;

    QTranslator qtTranslator;
    qtTranslator.load("qt_" + QLocale::system().name(), QLibraryInfo::path(QLibraryInfo::TranslationsPath));

    QTranslator openfortiguiTranslator;
    openfortiguiTranslator.load("openfortigui_" + QLocale::system().name(), ":/translations");

    if(cliMode(argc, argv))
    {
        QCoreApplication a(argc, argv);
        QCoreApplication::setApplicationName(openfortigui_config::name);
        QCoreApplication::setApplicationVersion(openfortigui_config::version);
        a.installTranslator(&qtTranslator);
        a.installTranslator(&openfortiguiTranslator);

        QCommandLineParser parser;
        parser.setApplicationDescription("Help for openfortiGUI options");
        parser.addHelpOption();
        parser.addVersionOption();

        QCommandLineOption startVpnProcess("start-vpn", QCoreApplication::translate("main", "Start vpn-process [must be run as root]"));
        parser.addOption(startVpnProcess);

        QCommandLineOption vpnName("vpn-name",
                    QCoreApplication::translate("main", "Set vpn name <vpnname>"),
                    QCoreApplication::translate("main", "vpnname"));
        parser.addOption(vpnName);

        QCommandLineOption mainConfig("main-config",
                    QCoreApplication::translate("main", "Use <mainconfig> as config file"),
                    QCoreApplication::translate("main", "mainconfig"));
        parser.addOption(mainConfig);

        // Already consumed by applyEarlyArgs(), declared so --help lists it and
        // process() does not reject it as unknown.
        QCommandLineOption apiSocket("api-socket",
                    QCoreApplication::translate("main", "Use <apisocket> as api socket path"),
                    QCoreApplication::translate("main", "apisocket"));
        parser.addOption(apiSocket);

        QCommandLineOption killVPNProcesses("kill-vpn-processes", QCoreApplication::translate("main", "Kills all vpn-processes"));
        parser.addOption(killVPNProcesses);

        parser.process(a);

        bool arg_startvpn = parser.isSet(startVpnProcess);
        bool arg_killvpnprocesses = parser.isSet(killVPNProcesses);
        QString arg_vpnname = parser.value(vpnName);
        QString arg_mainconfig = parser.value(mainConfig);

        if(arg_startvpn && !arg_vpnname.isEmpty())
        {
            qDebug() << QString("start-vpn process::") << arg_vpnname;

            if(!arg_mainconfig.isEmpty())
                tiConfMain::setMainConfig(arg_mainconfig);

            qDebug() << QString("start-vpn process::config_file::") << tiConfMain::main_config;

            vpnProcess proc;
            proc.setup(arg_vpnname);
            QTimer::singleShot(100, &proc, SLOT(startVPN()));

            return a.exec();
        }
        else if(arg_killvpnprocesses)
        {
            qDebug() << QString("kill-vpn-processes executed::");

            QStringList arguments;
            arguments << QFileInfo(QCoreApplication::applicationFilePath()).fileName();

            QProcess *ch = new QProcess();
            ch->start("killall", arguments);
            ch->waitForFinished(5000);
            delete ch;
        }
    }
    else
    {
        qDebug() << QString("start-main::");

        QApplication a(argc, argv);
        QApplication::setApplicationName(openfortigui_config::name);
        QApplication::setApplicationVersion(openfortigui_config::version);
        a.installTranslator(&qtTranslator);
        a.installTranslator(&openfortiguiTranslator);
        a.setQuitOnLastWindowClosed(false);

        if(isRunningAlready())
        {
            // Ask the running instance to show the main window instead of error message
            QLocalSocket apiServer;
            apiServer.connectToServer(vpnApi::socketPath());
            if(apiServer.waitForConnected(1000))
            {
                QByteArray block;
                QDataStream out(&block, QIODevice::WriteOnly);
                out.setVersion(QDataStream::Qt_6_0);
                vpnApi apiData;
                apiData.action = vpnApi::ACTION_SHOW_MAIN;
                out << apiData;

                apiServer.write(block);
                apiServer.flush();
            }
            else
            {
                qWarning() << apiServer.errorString();
            }

            exit(0);
        }

        MainWindow w;

        if(main_settings.getValue("main/start_minimized").toBool() == false)
            w.show();

        return a.exec();
    }
}
