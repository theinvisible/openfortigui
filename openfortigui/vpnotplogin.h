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

#ifndef VPNOTPLOGIN_H
#define VPNOTPLOGIN_H

#include <QWidget>
#include <QProcess>

namespace Ui {
class vpnOTPLogin;
}

class vpnOTPLogin : public QWidget
{
    Q_OBJECT

public:
    explicit vpnOTPLogin(QWidget *parent = 0);
    ~vpnOTPLogin();

    void setData(QProcess *otpproc);
    void initAfter();

private:
    Ui::vpnOTPLogin *ui;

    QProcess *procOTP;

private slots:
    void on_btnSubmit_clicked();
    void on_btnCancel_clicked();

    void on_leOTP_returnPressed();

protected:
    bool eventFilter(QObject *object, QEvent *event);
};

#endif // VPNOTPLOGIN_H
