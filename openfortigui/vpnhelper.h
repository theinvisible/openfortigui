#ifndef VPNHELPER_H
#define VPNHELPER_H

#include <QString>

struct vpnHelperResult
{
    bool status;
    QString msg;
    QString data;
};

class vpnHelper
{
public:
    vpnHelper();

    static QString formatByteUnits(qint64 num);

    static vpnHelperResult checkSystemPasswordStoreAvailable();
    static vpnHelperResult systemPasswordStoreWrite(const QString &key, const QString &data);
    static vpnHelperResult systemPasswordStoreRead(const QString &key);
    static vpnHelperResult systemPasswordStoreDelete(const QString &key);
};

#endif // VPNHELPER_H
