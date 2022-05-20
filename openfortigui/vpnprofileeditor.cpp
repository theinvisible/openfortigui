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

#include "vpnprofileeditor.h"
#include "ui_vpnprofileeditor.h"

#include <QFileDialog>
#include <QMessageBox>

#include "config.h"
#include "ticonfmain.h"

vpnProfileEditor::vpnProfileEditor(QWidget *parent, vpnProfileEditorMode smode) :
    QWidget(parent),
    ui(new Ui::vpnProfileEditor),
    mode(smode),
    config(0)
{
    ui->setupUi(this);

    // Validators
    QRegExp rx(openfortigui_config::validatorName);
    QValidator *validatorName = new QRegExpValidator(rx, this);
    ui->leName->setValidator(validatorName);

    // Default settings
    ui->cbSetRoutes->setChecked(true);
    ui->cbSetDNS->setChecked(true);
}

vpnProfileEditor::~vpnProfileEditor()
{
    delete ui;
}

void vpnProfileEditor::loadVpnProfile(const QString &profile, vpnProfile::Origin sourceOrigin)
{
    tiConfVpnProfiles vpns;
    config = vpns.getVpnProfileByName(profile, sourceOrigin);

    ui->leName->setText(config->name);
    ui->leGatewayHost->setText(config->gateway_host);
    ui->sBGatewayPort->setValue(config->gateway_port);
    ui->leUsername->setText(config->username);
    ui->lePassword->setText(config->readPassword());
    ui->cbPersistent->setChecked(config->persistent);

    if(!config->ca_file.isEmpty() || !config->user_cert.isEmpty() || !config->user_key.isEmpty())
    {
        ui->gbCertificate->setChecked(true);

        ui->leCAFile->setText(config->ca_file);
        ui->leUserCert->setText(config->user_cert);
        ui->leUserKey->setText(config->user_key);
    }

    ui->leTrustedCert->setText(config->trusted_cert);
    ui->cbTrustAllGwCerts->setChecked(config->trust_all_gw_certs);

    ui->cbSetRoutes->setChecked(config->set_routes);
    ui->cbSetDNS->setChecked(config->set_dns);
    ui->cbInsecureSSL->setChecked(config->insecure_ssl);
    ui->cbDebug->setChecked(config->debug);
    ui->leRealm->setText(config->realm);
    ui->cbAutostart->setChecked(config->autostart);
    ui->cbAlwaysAskOtp->setChecked(config->always_ask_otp);
    ui->leOTPPromptString->setText(config->otp_prompt);
    ui->sbOTPDelay->setValue(config->otp_delay);
    ui->cbHalfInternetRoutes->setChecked(config->half_internet_routers);
    ui->cbSecLevel1->setChecked(config->seclevel1);
    ui->comboMinTLSVersion->setCurrentText(config->min_tls);

    ui->cbPPPDNoPeerDNS->setChecked(config->pppd_no_peerdns);
    ui->lePPPDLogFile->setText(config->pppd_log_file);
    ui->lePPPDPluginFile->setText(config->pppd_plugin_file);
    ui->lePPPDIfname->setText(config->pppd_ifname);
    ui->lePPPDIPParam->setText(config->pppd_ipparam);
    ui->lePPPDCallName->setText(config->pppd_call);

    if(config->origin_location == vpnProfile::Origin_GLOBAL)
    {
        ui->leName->setDisabled(true);
        ui->leGatewayHost->setDisabled(true);
        ui->sBGatewayPort->setDisabled(true);
        ui->leUsername->setDisabled(true);
        ui->lePassword->setDisabled(true);
        ui->cbPersistent->setDisabled(true);
        ui->gbCertificate->setDisabled(true);
        ui->leCAFile->setDisabled(true);
        ui->leUserCert->setDisabled(true);
        ui->leUserKey->setDisabled(true);
        ui->leTrustedCert->setDisabled(true);
        ui->cbSetRoutes->setDisabled(true);
        ui->cbSetDNS->setDisabled(true);
        ui->cbPPPDNoPeerDNS->setDisabled(true);
        ui->cbInsecureSSL->setDisabled(true);
        ui->btnSave->setDisabled(true);
        ui->btnChooseCAFile->setDisabled(true);
        ui->btnChooseUserCert->setDisabled(true);
        ui->btnChooseUserKey->setDisabled(true);
        ui->cbDebug->setDisabled(true);
        ui->leRealm->setDisabled(true);
        ui->cbAutostart->setDisabled(true);
        ui->cbHalfInternetRoutes->setDisabled(true);
        ui->lePPPDLogFile->setDisabled(true);
        ui->lePPPDPluginFile->setDisabled(true);
        ui->lePPPDIfname->setDisabled(true);
        ui->lePPPDIPParam->setDisabled(true);
        ui->lePPPDCallName->setDisabled(true);
        ui->cbAlwaysAskOtp->setDisabled(true);
        ui->leOTPPromptString->setDisabled(true);
        ui->sbOTPDelay->setDisabled(true);
        ui->cbSecLevel1->setDisabled(true);
        ui->comboMinTLSVersion->setDisabled(true);
        ui->cbTrustAllGwCerts->setDisabled(true);
    }

    tiConfMain main_settings;
    bool disallowUnsecureCertificates = main_settings.getValue("main/disallow_unsecure_certificates").toBool();
    if(disallowUnsecureCertificates)
    {
        ui->gbCertificate->setDisabled(true);
        ui->cbTrustAllGwCerts->setDisabled(true);
        ui->leTrustedCert->setDisabled(true);
    }
}

void vpnProfileEditor::on_btnChooseUserCert_clicked()
{
    QString startDir = (ui->leUserCert->text().isEmpty()) ? QDir::homePath() : ui->leUserCert->text();

    QString dir = QFileDialog::getOpenFileName(this, tr("Select the user-cert"), startDir);

    if(!dir.isEmpty())
        ui->leUserCert->setText(dir);
}

void vpnProfileEditor::on_btnChooseUserKey_clicked()
{
    QString startDir = (ui->leUserKey->text().isEmpty()) ? QDir::homePath() : ui->leUserKey->text();

    QString dir = QFileDialog::getOpenFileName(this, tr("Select the user-key"), startDir);

    if(!dir.isEmpty())
        ui->leUserKey->setText(dir);
}

void vpnProfileEditor::on_btnChooseCAFile_clicked()
{
    QString startDir = (ui->leCAFile->text().isEmpty()) ? QDir::homePath() : ui->leCAFile->text();

    QString dir = QFileDialog::getOpenFileName(this, tr("Select the CA-file"), startDir);

    if(!dir.isEmpty())
        ui->leCAFile->setText(dir);
}

void vpnProfileEditor::on_btnCancel_clicked()
{
    parentWidget()->close();
}

void vpnProfileEditor::on_btnSave_clicked()
{
    if(ui->leName->text().isEmpty() || !ui->leName->hasAcceptableInput())
    {
        QMessageBox::information(this, tr("VPN"), tr("You must set a valid name for the VPN."));
        return;
    }

    if(ui->leGatewayHost->text().isEmpty())
    {
        QMessageBox::information(this, tr("VPN"), tr("You must set a gateway for the VPN."));
        return;
    }

    if(ui->sBGatewayPort->text().isEmpty())
    {
        QMessageBox::information(this, tr("VPN"), tr("You must set a gateway-port for the VPN."));
        return;
    }

    tiConfVpnProfiles vpns;
    vpnProfile vpn;

    if(mode == vpnProfileEditorModeEdit)
    {
        vpn.name = ui->leName->text();
        if(vpn.name != config->name)
            vpns.renameVpnProfile(config->name, vpn.name);

        if(!ui->gbCertificate->isChecked())
        {
            vpn.ca_file = "";
            vpn.user_cert = "";
            vpn.user_key = "";
            vpn.verify_cert = false;
        }
    }

    vpn.name = ui->leName->text();
    vpn.gateway_host = ui->leGatewayHost->text();
    vpn.gateway_port = ui->sBGatewayPort->text().toInt();
    vpn.username = ui->leUsername->text();
    vpn.password = ui->lePassword->text();
    vpn.persistent = ui->cbPersistent->isChecked();

    if(ui->gbCertificate->isChecked())
    {
        vpn.ca_file = ui->leCAFile->text();
        vpn.user_cert = ui->leUserCert->text();
        vpn.user_key = ui->leUserKey->text();
    }

    vpn.trusted_cert = ui->leTrustedCert->text();
    vpn.trust_all_gw_certs = ui->cbTrustAllGwCerts->isChecked();

    vpn.set_routes = ui->cbSetRoutes->isChecked();
    vpn.set_dns = ui->cbSetDNS->isChecked();
    vpn.pppd_no_peerdns = ui->cbPPPDNoPeerDNS->isChecked();
    vpn.insecure_ssl = ui->cbInsecureSSL->isChecked();
    vpn.debug = ui->cbDebug->isChecked();
    vpn.realm = ui->leRealm->text();
    vpn.autostart = ui->cbAutostart->isChecked();
    vpn.always_ask_otp = ui->cbAlwaysAskOtp->isChecked();
    vpn.half_internet_routers = ui->cbHalfInternetRoutes->isChecked();
    vpn.otp_prompt = ui->leOTPPromptString->text();
    vpn.otp_delay = ui->sbOTPDelay->text().toInt();
    vpn.seclevel1 = ui->cbSecLevel1->isChecked();
    vpn.min_tls = ui->comboMinTLSVersion->currentText();

    vpn.pppd_log_file = ui->lePPPDLogFile->text();
    vpn.pppd_plugin_file = ui->lePPPDPluginFile->text();
    vpn.pppd_ifname = ui->lePPPDIfname->text();
    vpn.pppd_ipparam = ui->lePPPDIPParam->text();
    vpn.pppd_call = ui->lePPPDCallName->text();

    vpns.saveVpnProfile(vpn);

    parentWidget()->close();

    if(mode == vpnProfileEditorModeEdit)
        emit vpnEdited(vpn);
    else if(mode == vpnProfileEditorModeNew)
        emit vpnAdded(vpn);
}
