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

QFile *openfortiguiLog = 0;

void logMessageOutput(QtMsgType type, const QMessageLogContext &, const QString & str)
{
    const char * msg = str.toStdString().c_str();

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

    tiConfMain main_settings;

    QTranslator qtTranslator;
    qtTranslator.load("qt_" + QLocale::system().name(), QLibraryInfo::location(QLibraryInfo::TranslationsPath));

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
            qInfo() << QString("start-vpn process::") << arg_vpnname;

            if(!arg_mainconfig.isEmpty())
                tiConfMain::setMainConfig(arg_mainconfig);

            qInfo() << QString("start-vpn process::config_file::") << tiConfMain::main_config;

            vpnProcess proc;
            proc.run(arg_vpnname);

            return a.exec();
        }
        else if(arg_killvpnprocesses)
        {
            qInfo() << QString("kill-vpn-processes executed::");

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
        qInfo() << QString("start-main::");

        QApplication a(argc, argv);
        QApplication::setApplicationName(openfortigui_config::name);
        QApplication::setApplicationVersion(openfortigui_config::version);
        a.installTranslator(&qtTranslator);
        a.installTranslator(&openfortiguiTranslator);
        a.setQuitOnLastWindowClosed(false);

        if(isRunningAlready())
        {
            qInfo() << "This application is already running, exiting now.";
            QMessageBox::critical(0, QApplication::tr("Application error"),
                                            QApplication::tr("This application is already running, exiting now."),
                                            QMessageBox::Ok);
            exit(0);
        }

        MainWindow w;

        if(main_settings.getValue("main/start_minimized").toBool() == false)
            w.show();

        return a.exec();
    }
}
