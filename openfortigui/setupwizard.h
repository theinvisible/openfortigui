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

#ifndef SETUPWIZARD_H
#define SETUPWIZARD_H

#include <QWidget>

namespace Ui {
class setupWizard;
}

class setupWizard : public QWidget
{
    Q_OBJECT

public:
    explicit setupWizard(QWidget *parent = 0);
    ~setupWizard();

private:
    Ui::setupWizard *ui;

    void updateButtons();

    void loadAESData();
    bool saveAESData();

    QString randString(int len);

private slots:
    void on_btnBack_clicked();
    void on_btnNext_clicked();
    void on_btnCancel_clicked();

    void on_btnGenKeys_clicked();
    void on_cbUseSystemPasswordStore_toggled(bool checked);
};

#endif // SETUPWIZARD_H
