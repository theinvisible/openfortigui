#include "vpnsetting.h"
#include "ui_vpnsetting.h"

#include <QFileDialog>

#include "ticonfmain.h"

vpnSetting::vpnSetting(QWidget *parent) :
    QWidget(parent),
    ui(new Ui::vpnSetting)
{
    ui->setupUi(this);

    tiConfMain confMain;
    ui->cbStartMinimized->setChecked(confMain.getValue("main/start_minimized").toBool());
    ui->cbDebug->setChecked(confMain.getValue("main/debug").toBool());
    ui->leAESKey->setText(confMain.getValue("main/aeskey").toString());
    ui->leAESIV->setText(confMain.getValue("main/aesiv").toString());

    ui->leLocalVPNProfiles->setText(confMain.getValue("paths/localvpnprofiles").toString());
    ui->leLocalVPNGroups->setText(confMain.getValue("paths/localvpngroups").toString());
    ui->leGlobalVPNProfiles->setText(confMain.getValue("paths/globalvpnprofiles").toString());
    ui->leLogs->setText(confMain.getValue("paths/logs").toString());
}

vpnSetting::~vpnSetting()
{
    delete ui;
}

void vpnSetting::on_btnCancel_clicked()
{
    parentWidget()->close();
}

void vpnSetting::on_btnSave_clicked()
{
    tiConfMain confMain;
    confMain.setValue("main/start_minimized", ui->cbStartMinimized->isChecked());
    confMain.setValue("main/debug", ui->cbDebug->isChecked());
    confMain.setValue("main/aeskey", ui->leAESKey->text());
    confMain.setValue("main/aesiv", ui->leAESIV->text());

    confMain.setValue("paths/localvpnprofiles", tiConfMain::formatPathReverse(ui->leLocalVPNProfiles->text()));
    confMain.setValue("paths/localvpngroups", tiConfMain::formatPathReverse(ui->leLocalVPNGroups->text()));
    confMain.setValue("paths/globalvpnprofiles", tiConfMain::formatPathReverse(ui->leGlobalVPNProfiles->text()));
    confMain.setValue("paths/logs", tiConfMain::formatPathReverse(ui->leLogs->text()));

    confMain.sync();

    parentWidget()->close();
}

void vpnSetting::on_btnChooseLocalVPNProfiles_clicked()
{
    pathChooser(ui->leLocalVPNProfiles);
}

void vpnSetting::on_btnChooseLocalVPNGroups_clicked()
{
    pathChooser(ui->leLocalVPNGroups);
}

void vpnSetting::on_btnChooseGlobalVPNProfiles_clicked()
{
    pathChooser(ui->leGlobalVPNProfiles);
}

void vpnSetting::on_btnChooseLogs_clicked()
{
    pathChooser(ui->leLogs);
}

void vpnSetting::pathChooser(QLineEdit *widget)
{
    QString startDir = (widget->text().isEmpty()) ? QDir::homePath() : tiConfMain::formatPath(widget->text());

    QString dir = QFileDialog::getExistingDirectory(this, trUtf8("Choose a directory"),
                                                    startDir,
                                                    QFileDialog::ShowDirsOnly
                                                    | QFileDialog::DontResolveSymlinks);

    if(!dir.isEmpty())
        widget->setText(dir);
}
