#ifndef VPNPROFILEEDITOR_H
#define VPNPROFILEEDITOR_H

#include <QWidget>

#include "vpnprofile.h"

namespace Ui {
class vpnProfileEditor;
}

enum vpnProfileEditorMode
{
    vpnProfileEditorModeNew = 0,
    vpnProfileEditorModeEdit
};

class vpnProfileEditor : public QWidget
{
    Q_OBJECT

public:
    explicit vpnProfileEditor(QWidget *parent = 0, vpnProfileEditorMode smode = vpnProfileEditorModeNew);
    ~vpnProfileEditor();

private:
    Ui::vpnProfileEditor *ui;

    vpnProfileEditorMode mode;

private slots:
    void on_btnChooseUserCert_clicked();
    void on_btnChooseUserKey_clicked();
    void on_btnChooseCAFile_clicked();

    void on_btnCancel_clicked();
    void on_btnSave_clicked();

signals:
    void vpnAdded(vpnProfile vpn);
    void vpnEdited(vpnProfile vpn);

};

#endif // VPNPROFILEEDITOR_H
