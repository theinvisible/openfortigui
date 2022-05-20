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

#include "vpnsetting.h"
#include "ui_vpnsetting.h"

#include <QFileDialog>
#include <QMessageBox>

#include "ticonfmain.h"
#include "vpnhelper.h"

vpnSetting::vpnSetting(QWidget *parent) :
    QWidget(parent),
    ui(new Ui::vpnSetting)
{
    ui->setupUi(this);

    tiConfMain confMain;
    ui->cbStartMinimized->setChecked(confMain.getValue("main/start_minimized").toBool());
    ui->cbDebug->setChecked(confMain.getValue("main/debug").toBool());
    ui->cbUseSystemPasswordStore->setChecked(confMain.getValue("main/use_system_password_store").toBool());
    ui->cbSUDOPreserveEnv->setChecked(confMain.getValue("main/sudo_preserve_env").toBool());
    if(confMain.getValue("main/use_system_password_store").toBool())
    {
        ui->leAESKey->setText(vpnHelper::systemPasswordStoreRead("aeskey").data);
        ui->leAESIV->setText(vpnHelper::systemPasswordStoreRead("aesiv").data);
    }
    else
    {
        ui->leAESKey->setText(confMain.getValue("main/aeskey").toString());
        ui->leAESIV->setText(confMain.getValue("main/aesiv").toString());
    }
    ui->cbDisallowUnsecureCertificates->setChecked(confMain.getValue("main/disallow_unsecure_certificates").toBool());	

    ui->leLocalVPNProfiles->setText(confMain.getValue("paths/localvpnprofiles").toString());
    ui->leLocalVPNGroups->setText(confMain.getValue("paths/localvpngroups").toString());
    ui->leGlobalVPNProfiles->setText(confMain.getValue("paths/globalvpnprofiles").toString());
    ui->leLogs->setText(confMain.getValue("paths/logs").toString());
    ui->cbDisableNotifications->setChecked(confMain.getValue("gui/disable_notifications").toBool());
    ui->cbConnectonDblClick->setChecked(confMain.getValue("gui/connect_on_dblclick").toBool());
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
    confMain.setValue("main/use_system_password_store", ui->cbUseSystemPasswordStore->isChecked());
    confMain.setValue("main/sudo_preserve_env", ui->cbSUDOPreserveEnv->isChecked());

    if(ui->cbUseSystemPasswordStore->isChecked()) {
        vpnHelper::systemPasswordStoreWrite("aeskey", ui->leAESKey->text());
        vpnHelper::systemPasswordStoreWrite("aesiv", ui->leAESIV->text());
        ui->leAESKey->setText("");
        ui->leAESIV->setText("");
    } else {
        vpnHelper::systemPasswordStoreDelete("aeskey");
        vpnHelper::systemPasswordStoreDelete("aesiv");
    }

    confMain.setValue("main/aeskey", ui->leAESKey->text());
    confMain.setValue("main/aesiv", ui->leAESIV->text());

    confMain.setValue("main/disallow_unsecure_certificates", ui->cbDisallowUnsecureCertificates->isChecked());

    confMain.setValue("paths/localvpnprofiles", tiConfMain::formatPathReverse(ui->leLocalVPNProfiles->text()));
    confMain.setValue("paths/localvpngroups", tiConfMain::formatPathReverse(ui->leLocalVPNGroups->text()));
    confMain.setValue("paths/globalvpnprofiles", tiConfMain::formatPathReverse(ui->leGlobalVPNProfiles->text()));
    confMain.setValue("paths/logs", tiConfMain::formatPathReverse(ui->leLogs->text()));

    confMain.setValue("gui/disable_notifications", ui->cbDisableNotifications->isChecked());
    confMain.setValue("gui/connect_on_dblclick", ui->cbConnectonDblClick->isChecked());

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

void vpnSetting::on_cbUseSystemPasswordStore_toggled(bool checked)
{
    if(checked == true)
    {
        vpnHelperResult result = vpnHelper::checkSystemPasswordStoreAvailable();
        if(result.status == false)
        {
            QMessageBox::critical(this, tr("System password manager error"), tr("Password manager ist not working, please check the status on your system (GNOME Keyring or KWallet). Error message: %1").arg(result.msg), QMessageBox::Ok);
            ui->cbUseSystemPasswordStore->setChecked(false);
        }
    }
}

void vpnSetting::pathChooser(QLineEdit *widget)
{
    QString startDir = (widget->text().isEmpty()) ? QDir::homePath() : tiConfMain::formatPath(widget->text());

    QString dir = QFileDialog::getExistingDirectory(this, tr("Choose a directory"),
                                                    startDir,
                                                    QFileDialog::ShowDirsOnly
                                                    | QFileDialog::DontResolveSymlinks);

    if(!dir.isEmpty())
        widget->setText(dir);
}
