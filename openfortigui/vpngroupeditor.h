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
