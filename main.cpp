#include "mainwindow.h"
#include <QApplication>
#include <QCoreApplication>

#include <QTextStream>

int main(int argc, char *argv[])
{
    if(argc > 1)
    {
        QCoreApplication a(argc, argv);
        if(a.arguments().at(1) == "-start-vpn")
        {
            QTextStream out(stdout);
            out << QString("start-vpn process");
        }

        return a.exec();
    }
    else
    {
        QApplication a(argc, argv);

        MainWindow w;
        w.show();

        return a.exec();
    }
}
