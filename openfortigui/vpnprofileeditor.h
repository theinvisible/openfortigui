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

    void loadVpnProfile(const QString &profile, vpnProfile::Origin sourceOrigin = vpnProfile::Origin_BOTH);

private:
    Ui::vpnProfileEditor *ui;

    vpnProfileEditorMode mode;
    vpnProfile *config;

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
