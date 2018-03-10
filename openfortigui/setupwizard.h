#ifndef SETUPWIZARD_H
#define SETUPWIZARD_H

#include <QWidget>

namespace Ui {
class setupWizard;
}

class setupWizard : public QWidget
{
    Q_OBJECT

public:
    explicit setupWizard(QWidget *parent = 0);
    ~setupWizard();

private:
    Ui::setupWizard *ui;

    void updateButtons();

    void loadAESData();
    bool saveAESData();

    QString randString(int len);

private slots:
    void on_btnBack_clicked();
    void on_btnNext_clicked();
    void on_btnCancel_clicked();

    void on_btnGenKeys_clicked();
    void on_cbUseSystemPasswordStore_toggled(bool checked);
};

#endif // SETUPWIZARD_H
