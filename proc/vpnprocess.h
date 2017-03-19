#ifndef VPNPROCESS_H
#define VPNPROCESS_H

#include <QObject>
#include <QLocalSocket>

#include "config.h"

class vpnProcess : public QObject
{
    Q_OBJECT
public:
    explicit vpnProcess(QObject *parent = 0);
    void run(const QString &vpnname);

private:
    QString name;
    QLocalSocket *apiServer;

    void closeProcess();
    void startVPN();

signals:

public slots:
    void onServerReadyRead();
};

#endif // VPNPROCESS_H
