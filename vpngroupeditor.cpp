#include "vpngroupeditor.h"
#include "ui_vpngroupeditor.h"

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

    // Treeview VPN-Groups
    QStringList headers;
    headers << trUtf8("Status") << trUtf8("Name");
    QStandardItemModel *model = new QStandardItemModel(ui->tvMembers);
    model->setHorizontalHeaderLabels(headers);
    ui->tvMembers->setModel(model);

    tiConfVpnProfiles vpns;
    vpns.readVpnProfiles();

    ui->tvMembers->setSortingEnabled(false);
    model->removeRows(0, model->rowCount());

    QStandardItem *item = 0;
    QStandardItem *item2 = 0;
    int row = model->rowCount();

    QList<vpnProfile*> vpnss = vpns.getVpnProfiles();
    for(int i=0; i < vpnss.count(); i++)
    {
        vpnProfile *vpn = vpnss.at(i);

        item = new QStandardItem("");
        item2 = new QStandardItem(vpn->name);

        item->setCheckable(true);

        row = model->rowCount();
        model->setItem(row, 0, item);
        model->setItem(row, 1, item2);
    }

    ui->tvMembers->header()->resizeSection(0, 50);
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

    QStandardItem *item = 0, *item2 = 0;
    for(int i=0; i<model->rowCount(); i++)
    {
        item = model->item(i, 0);
        item2 = model->item(i, 1);

        QStringListIterator it(config->members);
        while(it.hasNext())
        {
            if(item2->text() == it.next())
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

    QStandardItem *item = 0, *item2 = 0;
    QList<QString> members;
    tiConfVpnProfiles vpns;
    vpns.readVpnProfiles();
    vpnProfile *vpnprofile;
    for(int i=0; i<model->rowCount(); i++)
    {
        item = model->item(i, 0);
        item2 = model->item(i, 1);

        if(item->checkState() == Qt::Checked)
        {
            vpnprofile = vpns.getVpnProfileByName(item2->text());
            if(vpnprofile != 0 && (vpnprofile->name.isEmpty() || vpnprofile->password.isEmpty()))
            {
                QMessageBox::warning(this, trUtf8("VPN-Group"), trUtf8("You must set username and password for each group you want to include in a group. "
                                                                       "First missing on VPN: %1").arg(vpnprofile->name));

                return;
            }

            members.append(item2->text());
        }
    }
    vpngroup.members = members;

    vpngroups.saveVpnGroup(vpngroup);

    parentWidget()->close();

    if(mode == vpnGroupEditorModeEdit)
        emit vpnGroupEdited(vpngroup);
    else if(mode == vpnGroupEditorModeNew)
        emit vpnGroupAdded(vpngroup);
}
