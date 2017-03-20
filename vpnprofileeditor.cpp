#include "vpnprofileeditor.h"
#include "ui_vpnprofileeditor.h"

#include <QFileDialog>
#include <QMessageBox>

#include "ticonfmain.h"

vpnProfileEditor::vpnProfileEditor(QWidget *parent, vpnProfileEditorMode smode) :
    QWidget(parent),
    ui(new Ui::vpnProfileEditor),
    mode(smode),
    config(0)
{
    ui->setupUi(this);
}

vpnProfileEditor::~vpnProfileEditor()
{
    delete ui;
}

void vpnProfileEditor::loadVpnProfile(const QString &profile)
{
    tiConfVpnProfiles vpns;
    config = vpns.getVpnProfileByName(profile);

    ui->leName->setText(config->name);
    ui->leGatewayHost->setText(config->gateway_host);
    ui->sBGatewayPort->setValue(config->gateway_port);
    ui->leUsername->setText(config->username);
    ui->lePassword->setText(config->password);

    if(!config->ca_file.isEmpty() || !config->user_cert.isEmpty() || !config->user_key.isEmpty() || !config->trusted_cert.isEmpty())
    {
        ui->gbCertificate->setChecked(true);

        ui->leCAFile->setText(config->ca_file);
        ui->leUserCert->setText(config->user_cert);
        ui->leUserKey->setText(config->user_key);
        ui->leTrustedCert->setText(config->trusted_cert);
        ui->cbVerifyCert->setChecked(config->verify_cert);
    }

    ui->leOTP->setText(config->otp);
    ui->cbSetRoutes->setChecked(config->set_routes);
    ui->cbSetDNS->setChecked(config->set_dns);
    ui->cbPPPDUsePeerDNS->setChecked(config->pppd_use_peerdns);
    ui->cbInsecureSSL->setChecked(config->insecure_ssl);
}

void vpnProfileEditor::on_btnChooseUserCert_clicked()
{
    QString startDir = (ui->leUserCert->text().isEmpty()) ? QDir::homePath() : ui->leUserCert->text();

    QString dir = QFileDialog::getOpenFileName(this, trUtf8("Select the user-cert"), startDir);

    if(!dir.isEmpty())
        ui->leUserCert->setText(dir);
}

void vpnProfileEditor::on_btnChooseUserKey_clicked()
{
    QString startDir = (ui->leUserKey->text().isEmpty()) ? QDir::homePath() : ui->leUserKey->text();

    QString dir = QFileDialog::getOpenFileName(this, trUtf8("Select the user-key"), startDir);

    if(!dir.isEmpty())
        ui->leUserKey->setText(dir);
}

void vpnProfileEditor::on_btnChooseCAFile_clicked()
{
    QString startDir = (ui->leCAFile->text().isEmpty()) ? QDir::homePath() : ui->leCAFile->text();

    QString dir = QFileDialog::getOpenFileName(this, trUtf8("Select the CA-file"), startDir);

    if(!dir.isEmpty())
        ui->leCAFile->setText(dir);
}

void vpnProfileEditor::on_btnCancel_clicked()
{
    parentWidget()->close();
}

void vpnProfileEditor::on_btnSave_clicked()
{
    if(ui->leName->text().isEmpty())
    {
        QMessageBox::information(this, QString::fromUtf8("Add VPN"), QString::fromUtf8("You must set a name for the VPN."));
        return;
    }

    if(ui->leGatewayHost->text().isEmpty())
    {
        QMessageBox::information(this, QString::fromUtf8("Add VPN"), QString::fromUtf8("You must set a gateway for the VPN."));
        return;
    }

    if(ui->sBGatewayPort->text().isEmpty())
    {
        QMessageBox::information(this, QString::fromUtf8("Add VPN"), QString::fromUtf8("You must set a gateway-port for the VPN."));
        return;
    }

    if(ui->leUsername->text().isEmpty() || ui->lePassword->text().isEmpty())
    {
        QMessageBox::information(this, QString::fromUtf8("Add VPN"), QString::fromUtf8("You must set a username and password for the VPN."));
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

    if(ui->gbCertificate->isChecked())
    {
        vpn.ca_file = ui->leCAFile->text();
        vpn.user_cert = ui->leUserCert->text();
        vpn.user_key = ui->leUserKey->text();
        vpn.trusted_cert = ui->leTrustedCert->text();
        vpn.verify_cert = ui->cbVerifyCert->isChecked();
    }

    vpn.otp = ui->leOTP->text();
    vpn.set_routes = ui->cbSetRoutes->isChecked();
    vpn.set_dns = ui->cbSetDNS->isChecked();
    vpn.pppd_use_peerdns = ui->cbPPPDUsePeerDNS->isChecked();
    vpn.insecure_ssl = ui->cbInsecureSSL->isChecked();

    vpns.saveVpnProfile(vpn);

    parentWidget()->close();

    if(mode == vpnProfileEditorModeEdit)
        emit vpnEdited(vpn);
    else if(mode == vpnProfileEditorModeNew)
        emit vpnAdded(vpn);
}
