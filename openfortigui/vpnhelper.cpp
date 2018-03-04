#include "vpnhelper.h"

#include <QEventLoop>

#include "config.h"
#include <qt5keychain/keychain.h>

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

vpnHelperResult vpnHelper::checkSystemPasswordStoreAvailable()
{
    vpnHelperResult result;
    result.status = false;

    QKeychain::WritePasswordJob job(QLatin1String(openfortigui_config::password_manager_namespace));
    job.setAutoDelete(false);
    job.setKey("testkeystore");
    job.setBinaryData("test");
    QEventLoop loop;
    job.connect(&job, SIGNAL(finished(QKeychain::Job*)), &loop, SLOT(quit()));
    job.start();
    loop.exec();
    if(job.error())
    {
        result.status = false;
        result.msg = job.errorString();
        return result;
    }

    QKeychain::DeletePasswordJob job2(QLatin1String(openfortigui_config::password_manager_namespace));
    job2.setAutoDelete(false);
    job2.setKey("testkeystore");
    QEventLoop loop2;
    job2.connect(&job2, SIGNAL(finished(QKeychain::Job*)), &loop2, SLOT(quit()));
    job2.start();
    loop2.exec();
    if(job2.error())
    {
        result.status = false;
        result.msg = job2.errorString();
        return result;
    }

    result.status = true;
    return result;
}

vpnHelperResult vpnHelper::systemPasswordStoreWrite(const QString &key, const QString &data)
{
    vpnHelperResult result;
    result.status = false;

    QKeychain::WritePasswordJob job(QLatin1String(openfortigui_config::password_manager_namespace));
    job.setAutoDelete(false);
    job.setKey(key);
    job.setBinaryData(data.toUtf8());
    QEventLoop loop;
    job.connect(&job, SIGNAL(finished(QKeychain::Job*)), &loop, SLOT(quit()));
    job.start();
    loop.exec();
    if(job.error())
    {
        result.status = false;
        result.msg = job.errorString();
        return result;
    }

    result.status = true;
    return result;
}

vpnHelperResult vpnHelper::systemPasswordStoreRead(const QString &key)
{
    vpnHelperResult result;
    result.status = false;
    result.data = "";

    QKeychain::ReadPasswordJob job(QLatin1String(openfortigui_config::password_manager_namespace));
    job.setAutoDelete(false);
    job.setKey(key);
    QEventLoop loop;
    job.connect(&job, SIGNAL(finished(QKeychain::Job*)), &loop, SLOT(quit()));
    job.start();
    loop.exec();

    const QString pw = job.textData();
    if(job.error())
    {
        result.status = false;
        result.msg = job.errorString();
        return result;
    }

    result.status = true;
    result.data = pw;
    return result;
}

vpnHelperResult vpnHelper::systemPasswordStoreDelete(const QString &key)
{
    vpnHelperResult result;
    result.status = false;

    QKeychain::DeletePasswordJob job(QLatin1String(openfortigui_config::password_manager_namespace));
    job.setAutoDelete(false);
    job.setKey(key);
    QEventLoop loop;
    job.connect(&job, SIGNAL(finished(QKeychain::Job*)), &loop, SLOT(quit()));
    job.start();
    loop.exec();
    if(job.error())
    {
        result.status = false;
        result.msg = job.errorString();
        return result;
    }

    result.status = true;
    return result;
}

