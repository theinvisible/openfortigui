#include "vpnchangelog.h"
#include "ui_vpnchangelog.h"

#include "config.h"
#include "ticonfmain.h"

vpnChangelog::vpnChangelog(QWidget *parent) :
    QWidget(parent),
    ui(new Ui::vpnChangelog)
{
    ui->setupUi(this);
}

vpnChangelog::~vpnChangelog()
{
    delete ui;
}

void vpnChangelog::initAfter()
{
    parentWidget()->installEventFilter(this);
}

void vpnChangelog::updateChangelogReadFlag()
{
    tiConfMain main_settings;
    main_settings.setValue("main/changelogrev_read", openfortigui_config::changelogRev);
}

void vpnChangelog::on_btnOK_clicked()
{
    updateChangelogReadFlag();
    parentWidget()->close();
}

bool vpnChangelog::eventFilter(QObject *object, QEvent *event)
{
    if(object == parentWidget() && event->type() == QEvent::Close)
    {
        updateChangelogReadFlag();
    }

    return QWidget::eventFilter(object, event);
}
