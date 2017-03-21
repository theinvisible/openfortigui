#include "vpngroupeditor.h"
#include "ui_vpngroupeditor.h"

vpnGroupEditor::vpnGroupEditor(QWidget *parent, vpnGroupEditorMode smode) :
    QWidget(parent),
    ui(new Ui::vpnGroupEditor),
    mode(smode),
    config(0)
{
    ui->setupUi(this);
}

vpnGroupEditor::~vpnGroupEditor()
{
    delete ui;
}
