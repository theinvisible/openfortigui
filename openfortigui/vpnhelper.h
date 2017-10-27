#ifndef VPNHELPER_H
#define VPNHELPER_H

#include <QString>

class vpnHelper
{
public:
    vpnHelper();

    static QString formatByteUnits(qint64 num);
};

#endif // VPNHELPER_H
