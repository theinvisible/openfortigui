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

#include "vpnotplogin.h"
#include "ui_vpnotplogin.h"

#include <QDebug>

vpnOTPLogin::vpnOTPLogin(QWidget *parent) :
    QWidget(parent),
    ui(new Ui::vpnOTPLogin)
{
    ui->setupUi(this);

    procOTP = nullptr;
}

vpnOTPLogin::~vpnOTPLogin()
{
    delete ui;
}

void vpnOTPLogin::setData(QProcess *otpproc)
{
    procOTP = otpproc;
}

void vpnOTPLogin::setPrompt(const QString &question, const QString &fieldLabel)
{
    ui->label->setText(question);
    ui->label_3->setText(fieldLabel);
}

void vpnOTPLogin::initAfter()
{
    window()->installEventFilter(this);

    // The dialog exists to answer one question -- put the cursor where the
    // answer goes, without the user having to click first.
    ui->leOTP->setFocus();
}

void vpnOTPLogin::on_btnSubmit_clicked()
{
    if(procOTP->isWritable())
    {
        procOTP->write(ui->leOTP->text().append("\n").toLatin1());
        procOTP->waitForBytesWritten();
    }
    else
        qWarning() << "vpnOTPLogin::on_btnSubmit_clicked() :: Process is not writeable";

    window()->hide();
}

void vpnOTPLogin::on_btnCancel_clicked()
{
    if(procOTP->isWritable())
    {
        QByteArray ba("\n");
        procOTP->write(ba);
        procOTP->waitForBytesWritten();
    }
    else
        qWarning() << "vpnOTPLogin::on_btnCancel_clicked() :: Process is not writeable";

    window()->close();
}

void vpnOTPLogin::on_leOTP_returnPressed()
{
    on_btnSubmit_clicked();
}

bool vpnOTPLogin::eventFilter(QObject *object, QEvent *event)
{
    if(object == window() && event->type() == QEvent::Close)
    {
        on_btnCancel_clicked();
        event->ignore();
        window()->hide();

        return true;
    }

    return false;
}
