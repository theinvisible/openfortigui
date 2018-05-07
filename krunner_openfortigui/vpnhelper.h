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

    static bool isOpenFortiGUIRunning();
};

#endif // VPNHELPER_H
