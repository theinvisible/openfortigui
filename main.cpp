#include "mainwindow.h"

#include "config.h"

#include <QApplication>
#include <QCoreApplication>
#include <QTextStream>
#include <QCommandLineParser>

int main(int argc, char *argv[])
{
    QTextStream out(stdout);

    if(argc > 1)
    {
        QCoreApplication a(argc, argv);
        QCoreApplication::setApplicationName(openfortigui_config::name);
        QCoreApplication::setApplicationVersion(openfortigui_config::version);

        QCommandLineParser parser;
        parser.setApplicationDescription("Help for openfortiGUI options");
        parser.addHelpOption();
        parser.addVersionOption();

        QCommandLineOption startVpnProcess("start-vpn", QCoreApplication::translate("main", "Start vpn-process"));
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
            out << QString("start-vpn process::") << vpnname;
            out.flush();

            return a.exec();
        }
    }
    else
    {
        out << QString("start-main::");
        out.flush();

        QApplication a(argc, argv);
        QApplication::setApplicationName(openfortigui_config::name);
        QApplication::setApplicationVersion(openfortigui_config::version);

        MainWindow w;
        w.show();

        return a.exec();
    }
}
