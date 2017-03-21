#ifndef VPNGROUP_H
#define VPNGROUP_H

#include <QObject>

class vpnGroup
{

public:
    explicit vpnGroup();

    QString name;

    QStringList members;
};

#endif // VPNGROUP_H
