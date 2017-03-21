#ifndef VPNGROUP_H
#define VPNGROUP_H

#include <QObject>

class vpnGroup
{

public:
    explicit vpnGroup();

    QString name;

    QList<QString> members;
};

#endif // VPNGROUP_H
