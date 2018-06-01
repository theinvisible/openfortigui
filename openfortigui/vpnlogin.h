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
