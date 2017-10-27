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

    void loadVpnGroup(const QString &groupname);

private:
    Ui::vpnGroupEditor *ui;

    vpnGroupEditorMode mode;
    vpnGroup *config;

private slots:
    void on_btnCancel_clicked();
    void on_btnSave_clicked();

signals:
    void vpnGroupAdded(vpnGroup vpn);
    void vpnGroupEdited(vpnGroup vpn);
};

#endif // VPNGROUPEDITOR_H
