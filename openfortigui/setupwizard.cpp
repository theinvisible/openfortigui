#include "setupwizard.h"
#include "ui_setupwizard.h"

#include "qdebug.h"
#include "ticonfmain.h"

#include <QDateTime>
#include <QMessageBox>
#include <QThread>

setupWizard::setupWizard(QWidget *parent) :
    QWidget(parent),
    ui(new Ui::setupWizard)
{
    ui->setupUi(this);
}

setupWizard::~setupWizard()
{
    delete ui;
}

void setupWizard::updateButtons()
{
    int idx = ui->swContent->currentIndex();
    switch(idx)
    {
    case 0:
        ui->btnBack->setDisabled(true);
        ui->btnNext->setDisabled(false);
        break;
    case 1:
        ui->btnBack->setDisabled(false);
        ui->btnNext->setDisabled(false);
        break;
    }
}

void setupWizard::loadAESData()
{
    tiConfMain main_settings;

    ui->leAESKey->setText(main_settings.getValue("main/aeskey").toString());
    ui->leAESIV->setText(main_settings.getValue("main/aesiv").toString());
}

bool setupWizard::saveAESData()
{
    tiConfMain main_settings;

    QString aeskey = ui->leAESKey->text();
    QString aesiv = ui->leAESIV->text();

    if(aeskey.length() != 16 || aesiv.length() != 16)
    {
        QMessageBox::critical(0, tr("AES-Key error"),
                                        tr("Both AES-Key and AES-IV must be 128bit (16 chars) long."),
                                        QMessageBox::Ok);

        return false;
    }

    main_settings.setValue("main/aeskey", ui->leAESKey->text());
    main_settings.setValue("main/aesiv", ui->leAESIV->text());
    main_settings.sync();

    return true;
}

QString setupWizard::randString(int len)
{
    // Avoid using same seed
    QThread::msleep(15);

    QString str;
    int type;
    str.resize(len);
    qsrand(QTime::currentTime().msec());

    for(int s = 0; s < len ; ++s)
    {
        type = qrand() % 3;

        switch(type)
        {
        case 0:
            str[s] = QChar('a' + char(qrand() % ('z' - 'a')));
            break;
        case 1:
            str[s] = QChar('A' + char(qrand() % ('Z' - 'A')));
            break;
        case 2:
            str[s] = QChar('0' + char(qrand() % ('9' - '0')));
            break;
        }
    }

    return str;
}

void setupWizard::on_btnBack_clicked()
{
    int idx = ui->swContent->currentIndex();
    switch(idx)
    {
    case 1:
        if(saveAESData())
        {
            ui->swContent->setCurrentIndex(idx-1);
            ui->btnNext->setText(trUtf8("Next"));
        }
        break;
    }

    updateButtons();
}

void setupWizard::on_btnNext_clicked()
{
    int idx = ui->swContent->currentIndex();
    switch(idx)
    {
    case 0:
        loadAESData();
        ui->swContent->setCurrentIndex(idx+1);
        ui->btnNext->setText(trUtf8("Finish"));
        break;
    case 1:
        if(saveAESData())
        {
            tiConfMain main_settings;
            main_settings.setValue("main/setupwizard", true);
            main_settings.sync();
            parentWidget()->close();
        }
        break;
    }

    updateButtons();
}

void setupWizard::on_btnCancel_clicked()
{
    tiConfMain main_settings;

    if(!main_settings.getValue("main/setupwizard").toBool())
    {
        int result = QMessageBox::question(this, tr("Abort setup wizard"), "You are about to abort the setup wizard. Show again on next program start?", QMessageBox::Yes, QMessageBox::No, QMessageBox::Cancel | QMessageBox::Escape);
        qInfo() << "result::" << result;
        switch(result)
        {
        case QMessageBox::No:
            main_settings.setValue("main/setupwizard", true);
            main_settings.sync();
            break;
        case QMessageBox::Cancel:
            return;
        }
    }

    parentWidget()->close();
}

void setupWizard::on_btnGenKeys_clicked()
{
    ui->leAESKey->setText(randString(16));
    ui->leAESIV->setText(randString(16));
}
