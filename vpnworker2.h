#ifndef VPNWORKER2_H
#define VPNWORKER2_H

#include <QObject>

class vpnWorker2 : public QObject
{
    Q_OBJECT
public:
    explicit vpnWorker2(QObject *parent = 0);

signals:

public slots:
    void process();
};

#endif // VPNWORKER1_H
