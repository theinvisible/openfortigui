#include "mainwindow.h"

#include "config.h"
#include "ticonfmain.h"
#include "proc/vpnprocess.h"

#include <QApplication>
#include <QCoreApplication>
#include <QTextStream>
#include <QCommandLineParser>
#include <QtDebug>
#include <QFile>
#include <QDateTime>

QFile *openfortiguiLog = 0;

void logMessageOutput(QtMsgType type, const QMessageLogContext &, const QString & str)
{
    const char * msg = str.toStdString().c_str();

    tiConfMain main_settings;

    if(openfortiguiLog == 0)
    {
        openfortiguiLog = new QFile(QString("%1/openfortigui.log").arg(main_settings.getValue("paths/logs").toString()));
        openfortiguiLog->open(QIODevice::Append | QIODevice::Text);
    }

    bool tidebug = main_settings.getValue("main/debug").toBool();

    QTextStream sout(stdout);
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

int main(int argc, char *argv[])
{
    qInstallMessageHandler(logMessageOutput);

    if(argc > 1)
    {
        QCoreApplication a(argc, argv);
        QCoreApplication::setApplicationName(openfortigui_config::name);
        QCoreApplication::setApplicationVersion(openfortigui_config::version);

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

        parser.process(a);

        bool startvpn = parser.isSet(startVpnProcess);
        QString vpnname = parser.value(vpnName);

        if(startvpn && !vpnname.isEmpty())
        {
            qInfo() << QString("start-vpn process::") << vpnname;
            vpnProcess proc;
            proc.run(vpnname);

            return a.exec();
        }
    }
    else
    {
        qInfo() << QString("start-main::");

        QApplication a(argc, argv);
        QApplication::setApplicationName(openfortigui_config::name);
        QApplication::setApplicationVersion(openfortigui_config::version);

        MainWindow w;
        w.show();

        return a.exec();
    }
}
