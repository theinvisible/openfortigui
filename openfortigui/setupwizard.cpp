#include "setupwizard.h"
#include "ui_setupwizard.h"

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
