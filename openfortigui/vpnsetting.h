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

#ifndef VPNSETTING_H
#define VPNSETTING_H

#include <QWidget>
#include <QLineEdit>

namespace Ui {
class vpnSetting;
}

class vpnSetting : public QWidget
{
    Q_OBJECT

public:
    explicit vpnSetting(QWidget *parent = 0);
    ~vpnSetting();

public slots:
    void on_btnCancel_clicked();
    void on_btnSave_clicked();

    void on_btnChooseLocalVPNProfiles_clicked();
    void on_btnChooseLocalVPNGroups_clicked();
    void on_btnChooseGlobalVPNProfiles_clicked();
    void on_btnChooseLogs_clicked();
    void on_cbUseSystemPasswordStore_toggled(bool checked);

private:
    Ui::vpnSetting *ui;

    void pathChooser(QLineEdit *widget);
};

#endif // VPNSETTING_H
