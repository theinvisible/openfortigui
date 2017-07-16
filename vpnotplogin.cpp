#include "vpnotplogin.h"
#include "ui_vpnotplogin.h"

#include <QDebug>

vpnOTPLogin::vpnOTPLogin(QWidget *parent) :
    QWidget(parent),
    ui(new Ui::vpnOTPLogin)
{
    ui->setupUi(this);

    procOTP = 0;
}

vpnOTPLogin::~vpnOTPLogin()
{
    delete ui;
}

void vpnOTPLogin::setData(QProcess *otpproc)
{
    procOTP = otpproc;
}

void vpnOTPLogin::initAfter()
{
    parentWidget()->installEventFilter(this);
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

    parentWidget()->hide();
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

    parentWidget()->close();
}

void vpnOTPLogin::on_leOTP_returnPressed()
{
    on_btnSubmit_clicked();
}

bool vpnOTPLogin::eventFilter(QObject *object, QEvent *event)
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
