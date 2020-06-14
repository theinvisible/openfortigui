/*
 *  Copyright (C) 2018 Rene Hadler
 *
 *  This program is free software: you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation, either version 3 of the License, or
 *  (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

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
    if(profile->username.length() > 0)
    {
            ui->lePassword->setFocus();
    }
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
