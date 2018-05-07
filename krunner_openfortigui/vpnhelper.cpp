#include "vpnhelper.h"

#include <QProcess>
#include <QDebug>

vpnHelper::vpnHelper()
{

}

QString vpnHelper::formatByteUnits(qint64 num)
{
    if(num >= 1024*1024*1024)
        return QString("%1G").arg(QString::number((double)num / (1024*1024*1024), 'f', 2));
    else if(num >= 1024*1024)
        return QString("%1M").arg(QString::number((double)num / (1024*1024), 'f', 2));
    else if(num >= 1024)
        return QString("%1K").arg(QString::number((double)num / 1024, 'f', 2));
    else
        return QString("%1B").arg(num);
}

bool vpnHelper::isOpenFortiGUIRunning()
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
        if(line.contains("openfortigui"))
            count++;
    }
    delete ch;

    return (count > 0) ? true : false;
}
