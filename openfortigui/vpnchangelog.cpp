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
    buildChangelog();
}

void vpnChangelog::updateChangelogReadFlag()
{
    tiConfMain main_settings;
    main_settings.setValue("main/changelogrev_read", openfortigui_config::changelogRev);
}

void vpnChangelog::buildChangelog()
{
    QString changelog;

    changelog += "<!DOCTYPE HTML PUBLIC \"-//W3C//DTD HTML 4.0//EN\" \"http://www.w3.org/TR/REC-html40/strict.dtd\"> \
            <html><head><meta name=\"qrichtext\" content=\"1\" /><style type=\"text/css\"> \
            p, li { white-space: pre-wrap; } \
            </style></head><body style=\" font-family:'Noto Sans'; font-size:10pt; font-weight:400; font-style:normal;\">";

    // Version 0.6.1
    changelog += tr("<p style=\" margin-top:0px; margin-bottom:0px; margin-left:0px; margin-right:0px; -qt-block-indent:0; text-indent:0px;\"><span style=\" font-size:14pt; font-weight:600; color:#00851b;\">Version 0.6.1</span></p> \
            <p style=\"-qt-paragraph-type:empty; margin-top:0px; margin-bottom:0px; margin-left:0px; margin-right:0px; -qt-block-indent:0; text-indent:0px; font-size:14pt; font-weight:600; color:#00851b;\"><br /></p> \
            <p style=\" margin-top:0px; margin-bottom:0px; margin-left:0px; margin-right:0px; -qt-block-indent:0; text-indent:0px;\"><span style=\" font-weight:600;\">Major changes:</span></p> \
            <p style=\" margin-top:0px; margin-bottom:0px; margin-left:0px; margin-right:0px; -qt-block-indent:0; text-indent:0px;\"><span style=\" font-weight:600;\"> </span> - <span style=\" color:#ff0000;\">Added catalan language (thanks wagafo@github), Ubuntu 18.04 builds</span></p> \
            <p style=\"-qt-paragraph-type:empty; margin-top:0px; margin-bottom:0px; margin-left:0px; margin-right:0px; -qt-block-indent:0; text-indent:0px; font-weight:600;\"><br /></p> \
            <p style=\" margin-top:0px; margin-bottom:0px; margin-left:0px; margin-right:0px; -qt-block-indent:0; text-indent:0px;\"><span style=\" font-weight:600;\">Other changes:</span></p> \
            <p style=\" margin-top:0px; margin-bottom:0px; margin-left:0px; margin-right:0px; -qt-block-indent:0; text-indent:0px;\">  - Various Bugfixes (see git logs)</p> \
            <p style=\"-qt-paragraph-type:empty; margin-top:0px; margin-bottom:0px; margin-left:0px; margin-right:0px; -qt-block-indent:0; text-indent:0px; font-size:14pt; font-weight:600; color:#00851b;\"><br /></p>");

    // Version 0.6.0
    changelog += tr("<p style=\" margin-top:0px; margin-bottom:0px; margin-left:0px; margin-right:0px; -qt-block-indent:0; text-indent:0px;\"><span style=\" font-size:14pt; font-weight:600; color:#00851b;\">Version 0.6.0</span></p> \
            <p style=\"-qt-paragraph-type:empty; margin-top:0px; margin-bottom:0px; margin-left:0px; margin-right:0px; -qt-block-indent:0; text-indent:0px; font-weight:600;\"><br /></p> \
            <p style=\" margin-top:0px; margin-bottom:0px; margin-left:0px; margin-right:0px; -qt-block-indent:0; text-indent:0px;\"><span style=\" font-weight:600;\">Major changes:</span></p> \
            <p style=\" margin-top:0px; margin-bottom:0px; margin-left:0px; margin-right:0px; -qt-block-indent:0; text-indent:0px;\"><span style=\" font-weight:600;\"> </span> - <span style=\" color:#ff0000;\">Switched AES library to OpenSSL. Due to padding differences to former library you MUST reset all your passwords for your VPN profiles</span></p> \
            <p style=\"-qt-paragraph-type:empty; margin-top:0px; margin-bottom:0px; margin-left:0px; margin-right:0px; -qt-block-indent:0; text-indent:0px; font-weight:600;\"><br /></p> \
            <p style=\" margin-top:0px; margin-bottom:0px; margin-left:0px; margin-right:0px; -qt-block-indent:0; text-indent:0px;\"><span style=\" font-weight:600;\">Other changes:</span></p> \
            <p style=\" margin-top:0px; margin-bottom:0px; margin-left:0px; margin-right:0px; -qt-block-indent:0; text-indent:0px;\">  - Added Changelog dialog</p> \
            <p style=\" margin-top:0px; margin-bottom:0px; margin-left:0px; margin-right:0px; -qt-block-indent:0; text-indent:0px;\">  - Smaller bug fixes</p> \
            <p style=\" margin-top:0px; margin-bottom:0px; margin-left:0px; margin-right:0px; -qt-block-indent:0; text-indent:0px;\">  - Added &quot;half internet routes&quot; Option to VPN profile</p>");

    changelog += "</body></html>";

    ui->textBrowser->setHtml(changelog);
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
