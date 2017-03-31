#ifndef VPNSETTING_H
#define VPNSETTING_H

#include <QWidget>
#include <QLineEdit>

namespace Ui {
class vpnSetting;
}

class vpnSetting : public QWidget
{
    Q_OBJECT

public:
    explicit vpnSetting(QWidget *parent = 0);
    ~vpnSetting();

public slots:
    void on_btnCancel_clicked();
    void on_btnSave_clicked();

    void on_btnChooseLocalVPNProfiles_clicked();
    void on_btnChooseLocalVPNGroups_clicked();
    void on_btnChooseGlobalVPNProfiles_clicked();
    void on_btnChooseLogs_clicked();

private:
    Ui::vpnSetting *ui;

    void pathChooser(QLineEdit *widget);
};

#endif // VPNSETTING_H
