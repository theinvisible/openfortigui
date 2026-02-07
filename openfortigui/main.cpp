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

QFile *openfortiguiLog = 0;

void logMessageOutput(QtMsgType type, const QMessageLogContext &, const QString & str)
{
    tiConfMain main_settings;
    QTextStream sout(stdout);

    if(openfortiguiLog == 0)
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
            out << currentDate.toString("MMM d hh:mm:ss").toStdString().c_str() << " openfortiGUI::Debug: " << str << "\n";
        break;
    case QtWarningMsg:
        out << currentDate.toString("MMM d hh:mm:ss").toStdString().c_str() << " openfortiGUI::Warning: " << str << "\n";
        break;
    case QtCriticalMsg:
        out << currentDate.toString("MMM d hh:mm:ss").toStdString().c_str() << " openfortiGUI::Critical: " << str << "\n";
        break;
    case QtInfoMsg:
        out << currentDate.toString("MMM d hh:mm:ss").toStdString().c_str() << " openfortiGUI::Info: " << str << "\n";
        sout << currentDate.toString("MMM d hh:mm:ss").toStdString().c_str() << " openfortiGUI::Info: " << str << "\n";
        break;
    case QtFatalMsg:
        out << currentDate.toString("MMM d hh:mm:ss").toStdString().c_str() << " openfortiGUI::Fatal: " << str << "\n";
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

int main(int argc, char *argv[])
{
    qInstallMessageHandler(logMessageOutput);

    qRegisterMetaType<vpnClientConnection::connectionStatus>("vpnClientConnection::connectionStatus");
    qRegisterMetaType<vpnStats>("vpnStats");
    qRegisterMetaType<vpnMsg>("vpnMsg");

    tiConfMain main_settings;

    QTranslator qtTranslator;
    qtTranslator.load("qt_" + QLocale::system().name(), QLibraryInfo::path(QLibraryInfo::TranslationsPath));

    QTranslator openfortiguiTranslator;
    openfortiguiTranslator.load("openfortigui_" + QLocale::system().name(), ":/translations");

    if(argc > 1)
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
            apiServer.connectToServer(openfortigui_config::name);
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
