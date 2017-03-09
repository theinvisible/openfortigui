#ifndef VPNWORKER_H
#define VPNWORKER_H

#include <QObject>

class vpnWorker : public QObject
{
    Q_OBJECT
public:
    explicit vpnWorker(QObject *parent = 0);

signals:

public slots:
    void process();
};

#endif // VPNWORKER_H
