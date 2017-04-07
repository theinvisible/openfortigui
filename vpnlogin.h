#ifndef VPNLOGIN_H
#define VPNLOGIN_H

#include <QWidget>

#include "vpnmanager.h"

namespace Ui {
class vpnLogin;
}

class vpnLogin : public QWidget
{
    Q_OBJECT

public:
    explicit vpnLogin(QWidget *parent = 0);
    ~vpnLogin();

    void setData(vpnManager *manager, const QString &name);
    void initAfter();

private:
    Ui::vpnLogin *ui;

    vpnManager *vpnmanager;
    QString vpnname;

private slots:
    void on_btnSubmit_clicked();
    void on_btnCancel_clicked();

    void on_leUsername_returnPressed();
    void on_lePassword_returnPressed();

protected:
    bool eventFilter(QObject *object, QEvent *event);
};

#endif // VPNLOGIN_H
