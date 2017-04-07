#include "vpnlogin.h"
#include "ui_vpnlogin.h"

#include "ticonfmain.h"

vpnLogin::vpnLogin(QWidget *parent) :
    QWidget(parent),
    ui(new Ui::vpnLogin)
{
    ui->setupUi(this);

    vpnmanager = 0;
    vpnname = "";
}

vpnLogin::~vpnLogin()
{
    delete ui;
}

void vpnLogin::setData(vpnManager *manager, const QString &name)
{
    vpnmanager = manager;
    vpnname = name;

    tiConfVpnProfiles vpnss;
    vpnProfile *profile = vpnss.getVpnProfileByName(vpnname);
    ui->leUsername->setText(profile->username);
}

void vpnLogin::initAfter()
{
    parentWidget()->installEventFilter(this);
}

void vpnLogin::on_btnSubmit_clicked()
{
    vpnmanager->submitVPNCred(vpnname, ui->leUsername->text(), ui->lePassword->text());
    parentWidget()->hide();
}

void vpnLogin::on_btnCancel_clicked()
{
    vpnmanager->submitVPNCred(vpnname, "", "");
    parentWidget()->close();
}

void vpnLogin::on_leUsername_returnPressed()
{
    on_btnSubmit_clicked();
}

void vpnLogin::on_lePassword_returnPressed()
{
    on_btnSubmit_clicked();
}

bool vpnLogin::eventFilter(QObject *object, QEvent *event)
{
    if(object == parentWidget() && event->type() == QEvent::Close)
    {
        on_btnCancel_clicked();
        event->ignore();
        parentWidget()->hide();

        return true;
    }

    return false;
}
