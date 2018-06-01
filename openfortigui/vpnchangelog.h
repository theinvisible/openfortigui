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

#ifndef VPNCHANGELOG_H
#define VPNCHANGELOG_H

#include <QWidget>

namespace Ui {
class vpnChangelog;
}

class vpnChangelog : public QWidget
{
    Q_OBJECT

public:
    explicit vpnChangelog(QWidget *parent = 0);
    ~vpnChangelog();

    void initAfter();

private:
    Ui::vpnChangelog *ui;

    void updateChangelogReadFlag();
    void buildChangelog();

private slots:
    void on_btnOK_clicked();

protected:
    bool eventFilter(QObject *object, QEvent *event);
};

#endif // VPNCHANGELOG_H
