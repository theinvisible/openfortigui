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

#include "vpngroupeditor.h"
#include "ui_vpngroupeditor.h"

#include "config.h"
#include "ticonfmain.h"

#include <QStandardItemModel>
#include <QMessageBox>

vpnGroupEditor::vpnGroupEditor(QWidget *parent, vpnGroupEditorMode smode) :
    QWidget(parent),
    ui(new Ui::vpnGroupEditor),
    mode(smode),
    config(0)
{
    ui->setupUi(this);

    // Validators
    QRegExp rx(openfortigui_config::validatorName);
    QValidator *validatorName = new QRegExpValidator(rx, this);
    ui->leName->setValidator(validatorName);

    // Treeview VPN-Groups
    QStringList headers;
    headers << tr("Status") << tr("Name") << tr("Origin");
    QStandardItemModel *model = new QStandardItemModel(ui->tvMembers);
    model->setHorizontalHeaderLabels(headers);
    ui->tvMembers->setModel(model);

    tiConfVpnProfiles vpns;
    vpns.readVpnProfiles();

    ui->tvMembers->setSortingEnabled(false);
    model->removeRows(0, model->rowCount());

    QStandardItem *item = 0;
    QStandardItem *item2 = 0;
    QStandardItem *item3 = 0;
    int row = model->rowCount();

    QList<vpnProfile*> vpnss = vpns.getVpnProfiles();
    QString originText = "";
    for(int i=0; i < vpnss.count(); i++)
    {
        vpnProfile *vpn = vpnss.at(i);

        item = new QStandardItem("");
        item2 = new QStandardItem(vpn->name);
        switch(vpn->origin_location)
        {
        case vpnProfile::Origin_LOCAL:
            originText = tr("Local");
            break;
        case vpnProfile::Origin_GLOBAL:
            originText = tr("Global");
            break;
        }
        item3 = new QStandardItem(originText);
        item3->setData(vpn->origin_location);

        item->setCheckable(true);

        row = model->rowCount();
        model->setItem(row, 0, item);
        model->setItem(row, 1, item2);
        model->setItem(row, 2, item3);
    }

    ui->tvMembers->header()->resizeSection(0, 50);
    ui->tvMembers->header()->resizeSection(1, 250);
    ui->tvMembers->setSortingEnabled(true);
    ui->tvMembers->sortByColumn(1, Qt::AscendingOrder);
}

vpnGroupEditor::~vpnGroupEditor()
{
    delete ui;
}

void vpnGroupEditor::loadVpnGroup(const QString &groupname)
{
    tiConfVpnGroups vpngroups;
    QStandardItemModel *model = dynamic_cast<QStandardItemModel *>(ui->tvMembers->model());
    config = vpngroups.getVpnGroupByName(groupname);

    ui->leName->setText(config->name);

    QStandardItem *item = 0, *item2 = 0, *item3 = 0;
    for(int i=0; i<model->rowCount(); i++)
    {
        item = model->item(i, 0);
        item2 = model->item(i, 1);
        item3 = model->item(i, 2);
        vpnProfile::Origin origin = static_cast<vpnProfile::Origin>(item3->data().toInt());

        QStringListIterator it(config->localMembers);
        while(it.hasNext())
        {
            if(item2->text() == it.next() && origin == vpnProfile::Origin_LOCAL)
                item->setCheckState(Qt::Checked);
        }

        QStringListIterator git(config->globalMembers);
        while(git.hasNext())
        {
            if(item2->text() == git.next() && origin == vpnProfile::Origin_GLOBAL)
                item->setCheckState(Qt::Checked);
        }
    }
}

void vpnGroupEditor::on_btnCancel_clicked()
{
    parentWidget()->close();
}

void vpnGroupEditor::on_btnSave_clicked()
{
    if(ui->leName->text().isEmpty())
    {
        QMessageBox::information(this, trUtf8("VPN-Group"), trUtf8("You must set a name for the VPN-group."));
        return;
    }

    tiConfVpnGroups vpngroups;
    vpnGroup vpngroup;
    QStandardItemModel *model = dynamic_cast<QStandardItemModel *>(ui->tvMembers->model());

    if(mode == vpnGroupEditorModeEdit)
    {
        vpngroup.name = ui->leName->text();
        if(vpngroup.name != config->name)
            vpngroups.renameVpnGroup(config->name, vpngroup.name);
    }

    vpngroup.name = ui->leName->text();

    QStandardItem *item = 0, *item2 = 0, *item3 = 0;
    QList<QString> lMembers, gMembers;
    tiConfVpnProfiles vpns;
    vpns.readVpnProfiles();
    vpnProfile *vpnprofile;
    for(int i=0; i<model->rowCount(); i++)
    {
        item = model->item(i, 0);
        item2 = model->item(i, 1);
        item3 = model->item(i, 2);
        vpnProfile::Origin origin = static_cast<vpnProfile::Origin>(item3->data().toInt());

        if(item->checkState() == Qt::Checked)
        {
            vpnprofile = vpns.getVpnProfileByName(item2->text(), origin);
            if(vpnprofile != 0 && (vpnprofile->name.isEmpty() || vpnprofile->password.isEmpty()))
            {
                QMessageBox::warning(this, trUtf8("VPN-Group"), trUtf8("You must set username and password for each group you want to include in a group. "
                                                                       "First missing on VPN: %1").arg(vpnprofile->name));

                return;
            }

            if(origin == vpnProfile::Origin_LOCAL)
                lMembers.append(item2->text());
            else if(origin == vpnProfile::Origin_GLOBAL)
                gMembers.append(item2->text());
        }
    }
    vpngroup.localMembers = lMembers;
    vpngroup.globalMembers = gMembers;

    vpngroups.saveVpnGroup(vpngroup);

    parentWidget()->close();

    if(mode == vpnGroupEditorModeEdit)
        emit vpnGroupEdited(vpngroup);
    else if(mode == vpnGroupEditorModeNew)
        emit vpnGroupAdded(vpngroup);
}
