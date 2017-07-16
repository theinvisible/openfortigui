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
