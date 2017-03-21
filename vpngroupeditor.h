#ifndef VPNGROUPEDITOR_H
#define VPNGROUPEDITOR_H

#include <QWidget>

#include <vpngroup.h>

namespace Ui {
class vpnGroupEditor;
}

enum vpnGroupEditorMode
{
    vpnGroupEditorModeNew = 0,
    vpnGroupEditorModeEdit
};

class vpnGroupEditor : public QWidget
{
    Q_OBJECT

public:
    explicit vpnGroupEditor(QWidget *parent = 0, vpnGroupEditorMode smode = vpnGroupEditorModeNew);
    ~vpnGroupEditor();

private:
    Ui::vpnGroupEditor *ui;

    vpnGroupEditorMode mode;
    vpnGroup *config;
};

#endif // VPNGROUPEDITOR_H
