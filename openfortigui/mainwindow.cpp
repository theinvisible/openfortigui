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

#include "mainwindow.h"
#include "ui_mainwindow.h"

#include <QThread>
#include <QProcess>
#include <QTextStream>
#include <QStandardItemModel>
#include <QMessageBox>
#include <QInputDialog>
#include <QScreen>
#include <QToolButton>
#include <QDesktopServices>
#include <QUrl>
#include <QUrlQuery>
#include <QTimer>
#include <QScrollArea>

#include "config.h"
#include "ticonfmain.h"
#include "vpnprofileeditor.h"
#include "vpngroupeditor.h"
#include "vpnsetting.h"
#include "vpnlogin.h"
#include "vpnotplogin.h"
#include "vpnhelper.h"
#include "setupwizard.h"
#include "vpnchangelog.h"

vpnManager *MainWindow::vpnmanager = nullptr;

/*
 * Host window for the editor forms.
 *
 * Every one of these used to be five hand-copied lines ending in
 *   setMinimumSize(QSize(f->width(), f->height()))
 * which pinned the window to the size the form was designed at. The VPN profile
 * editor is 842 px high, so on a 1280x800 screen it could not be made to fit --
 * the buttons at the bottom were simply unreachable (issue #205).
 *
 * The form now lives in a QScrollArea and the window opens at its size hint,
 * capped at 90% of the available screen area. No minimum size: the user decides
 * how small it gets, and the content scrolls.
 */
static QMainWindow *openToolWindow(QWidget *parent, QWidget *content, const QString &title)
{
    auto *window = new QMainWindow(parent, Qt::Dialog);
    window->setWindowModality(Qt::WindowModal);
    window->setWindowTitle(title);

    auto *scroll = new QScrollArea(window);
    scroll->setWidgetResizable(true);
    scroll->setFrameShape(QFrame::NoFrame);
    scroll->setWidget(content);
    window->setCentralWidget(scroll);

    QSize wanted = content->sizeHint();
    if(window->screen() != nullptr)
    {
        // Leave room for panels and window decorations.
        const QSize avail = window->screen()->availableGeometry().size() * 0.9;
        wanted = wanted.boundedTo(avail);
    }
    window->resize(wanted);

    return window;
}

MainWindow::MainWindow(QWidget *parent) :
    QMainWindow(parent),
    ui(new Ui::MainWindow)
{
    ui->setupUi(this);

    installEventFilter(this);

    vpnmanager = new vpnManager(this);
    connect(vpnmanager, SIGNAL(VPNStatusChanged(QString,vpnClientConnection::connectionStatus)), this, SLOT(onClientVPNStatusChanged(QString,vpnClientConnection::connectionStatus)));
    connect(vpnmanager, SIGNAL(VPNCredRequest(QString)), this, SLOT(onClientVPNCredRequest(QString)));
    connect(vpnmanager, SIGNAL(VPNStatsUpdate(QString,vpnStats)), this, SLOT(onClientVPNStatsUpdate(QString,vpnStats)));
    connect(vpnmanager, SIGNAL(VPNPromptRequest(QProcess*,int)), this, SLOT(onClientVPNPromptRequest(QProcess*,int)));
    connect(vpnmanager, SIGNAL(VPNMessage(QString,vpnMsg)), this, SLOT(onClientVPNMessage(QString,vpnMsg)));
    connect(vpnmanager, SIGNAL(VPNCertificateValidationFailed(QString,QString)), this, SLOT(onClientCertValidationFAiled(QString,QString)));
    connect(vpnmanager, SIGNAL(VPNSAMLAuthRequest(QString)), this, SLOT(onClientSAMLAuthRequest(QString)));
    connect(vpnmanager, SIGNAL(VPNShowMainWindowRequest()), this, SLOT(showMainWindow()));

    signalMapper = new QSignalMapper(this);
    connect(signalMapper, SIGNAL(mappedString(QString)), this, SLOT(onActionStartVPN(QString)));
    signalMapperGroups = new QSignalMapper(this);
    connect(signalMapperGroups, SIGNAL(mappedString(QString)), this, SLOT(onActionStartVPNGroup(QString)));

    // Center window on startup
    QRect geom = QGuiApplication::screens()[0]->availableGeometry();
    if(geom.width() > 2560 && geom.height() > 1440)
        resize(geom.width() / 3, geom.height() / 3);
    move((geom.width() - width()) / 2, (geom.height() - height()) / 2);

    // Treeview VPNs
    QStringList headers;
    headers << tr("Status") << tr("Name") << tr("Device") << tr("Gateway") << tr("User") << tr("Traffic RX/TX") << tr("Connected");
    QStandardItemModel *model = new QStandardItemModel(ui->tvVpnProfiles);
    model->setHorizontalHeaderLabels(headers);
    ui->tvVpnProfiles->setModel(model);
    ui->tvVpnProfiles->setRootIsDecorated(true);
    root_local_vpn = new QStandardItem(QIcon(":/img/local.png"), tr("Local VPNs"));
    root_local_vpn->setData(vpnProfile::Origin_LOCAL);
    root_global_vpn = new QStandardItem(QIcon(":/img/global.png"), tr("Global VPNs"));
    root_global_vpn->setData(vpnProfile::Origin_GLOBAL);
    model->setItem(0, 0, root_local_vpn);
    model->setItem(1, 0, root_global_vpn);
    ui->tvVpnProfiles->setExpanded(model->indexFromItem(root_local_vpn), true);
    ui->tvVpnProfiles->setExpanded(model->indexFromItem(root_global_vpn), true);
    ui->tvVpnProfiles->header()->resizeSection(0, 150);
    ui->tvVpnProfiles->header()->resizeSection(1, int(width() * 0.27));
    ui->tvVpnProfiles->header()->resizeSection(3, int(width() * 0.27));

    connect(ui->tvVpnProfiles, SIGNAL(customContextMenuRequested(const QPoint &)), this, SLOT(ontvVpnProfilesCustomContextMenu(const QPoint &)));
    connect(ui->leSearch, SIGNAL(textChanged(QString)), this, SLOT(onvpnSearch(QString)));

    // Treeview VPN-Groups
    QStringList headers2;
    headers2 << tr("Status") << tr("Name") << tr("VPNs");
    QStandardItemModel *model2 = new QStandardItemModel(ui->tvVPNGroups);
    model2->setHorizontalHeaderLabels(headers2);
    ui->tvVPNGroups->setModel(model2);

    tray = new QSystemTrayIcon(this);
    tray->setIcon(QIcon(":/img/app.png"));
    tray->show();
    tray_menu = tray->contextMenu();
    tray_group_menu = new QMenu(tr("VPN-Groups"));
    connect(tray, SIGNAL(activated(QSystemTrayIcon::ActivationReason)), this, SLOT(onTrayIconActivated(QSystemTrayIcon::ActivationReason)));

    QToolButton *tbtnAdd = new QToolButton();
    QMenu *tmnuAdd = new QMenu(tbtnAdd);
    tmnuAdd->addAction(tr("VPN"), this, SLOT(on_btnAddVPN_clicked()));
    tmnuAdd->addAction(tr("VPN-Group"), this, SLOT(on_btnAddGroup_clicked()));
    tbtnAdd->setPopupMode(QToolButton::InstantPopup);
    tbtnAdd->setToolButtonStyle(Qt::ToolButtonTextUnderIcon);
    tbtnAdd->setMenu(tmnuAdd);
    tbtnAdd->setIcon(QIcon::fromTheme("list-add", QIcon(":/img/add.png")));
    tbtnAdd->setText(tr("Add"));

    ui->tbActions->addAction(QIcon(":/img/connected.png"), tr("Connect"), this, SLOT(onStartVPN()));
    ui->tbActions->addAction(QIcon(":/img/disconnected.png"), tr("Disconnect"), this, SLOT(onStopVPN()));
    ui->tbActions->addSeparator();
    ui->tbActions->addWidget(tbtnAdd);
    ui->tbActions->addAction(QIcon::fromTheme("edit-paste", QIcon(":/img/edit.png")), tr("Edit"), this, SLOT(onTbActionEdit()));
    ui->tbActions->addAction(QIcon::fromTheme("edit-copy", QIcon(":/img/copy.png")), tr("Copy"), this, SLOT(onTbActionCopy()));
    QAction *actionSearch = ui->tbActions->addAction(QIcon::fromTheme("system-search", QIcon(":/img/search.png")), tr("Search"), this, SLOT(onTbActionSearch()));
    actionSearch->setCheckable(true);
    ui->tbActions->addSeparator();
    ui->tbActions->addAction(QIcon::fromTheme("edit-delete", QIcon(":/img/delete.png")), tr("Delete"), this, SLOT(onTbActionDelete()));
    ui->tbActions->addAction(QIcon::fromTheme("accessories-text-editor", QIcon(":/img/log.png")), tr("Logs"), this, SLOT(onTbActionLogs()));
    ui->tbActions->addSeparator();
    ui->tbActions->addAction(QIcon::fromTheme("help-about", QIcon(":/img/about.png")), tr("About"), this, SLOT(onActionAbout()));

    ui->leSearch->hide();

    connect(ui->actionMenuExit, SIGNAL(triggered(bool)), this, SLOT(onQuit()));
    connect(ui->actionMenuHide, SIGNAL(triggered(bool)), this, SLOT(hide()));
    connect(ui->actionMenuSettings, SIGNAL(triggered(bool)), this, SLOT(onVPNSettings()));
    connect(ui->actionMenuWizard, SIGNAL(triggered(bool)), this, SLOT(onSetupWizard()));
    connect(ui->actionChangelog, SIGNAL(triggered(bool)), this, SLOT(onChangelog()));
    connect(ui->actionMenuLogs, SIGNAL(triggered(bool)), this, SLOT(onActionLogs()));
    connect(ui->actionMenuConnect, SIGNAL(triggered(bool)), this, SLOT(onStartVPN()));
    connect(ui->actionMenuDisconnect, SIGNAL(triggered(bool)), this, SLOT(onStopVPN()));
    connect(ui->actionMenuAbout, SIGNAL(triggered(bool)), this, SLOT(onActionAbout()));

    refreshVpnProfileList();
    refreshVpnGroupList();

    tiConfMain main_settings;

    /*
     * Coalesce watcher events. Saving a single profile already produces several
     * directoryChanged signals -- QSettings writes through a temporary file and
     * the mode is set afterwards -- and each one used to rebuild both lists and
     * the whole tray menu (issue #210).
     */
    timerVpnProfilesRefresh = new QTimer(this);
    timerVpnProfilesRefresh->setSingleShot(true);
    timerVpnProfilesRefresh->setInterval(300);
    connect(timerVpnProfilesRefresh, SIGNAL(timeout()), this, SLOT(onVpnProfilesRefreshTimeout()));

    watcherVpnProfiles = new QFileSystemWatcher(this);
    setupVpnProfileWatcher();
    connect(watcherVpnProfiles, SIGNAL(directoryChanged(QString)), this, SLOT(onWatcherVpnProfilesChanged(QString)));

    autostartVPNs();

    // Deferred: these window-modal dialogs must not be shown from the
    // constructor. The main window is shown after it, so a dialog mapped here
    // has no visible parent yet, ends up BEHIND the main window and blocks it
    // -- which looks like a frozen application (raise() cannot fix that on
    // Wayland). A queued call runs once the main window is up.
    if(!main_settings.getValue("main/setupwizard").toBool())
        QTimer::singleShot(0, this, &MainWindow::onSetupWizard);

    if(main_settings.getValue("main/changelogrev_read", 0).toInt() > 0 && openfortigui_config::changelogRev > main_settings.getValue("main/changelogrev_read", 0).toInt())
        QTimer::singleShot(0, this, &MainWindow::onChangelog);

    if(main_settings.getValue("main/changelogrev_read", 0).toInt() == 0)
        main_settings.setValue("main/changelogrev_read", openfortigui_config::changelogRev);

    if(main_settings.getValue("main/show_search").toBool())
    {
        actionSearch->setChecked(true);
        ui->leSearch->show();
    }

    if(main_settings.getValue("gui/main_toolbar_location", 0).toInt() != 0)
        addToolBar(static_cast<Qt::ToolBarArea>(main_settings.getValue("gui/main_toolbar_location", 0).toInt()), this->ui->tbActions);
}

MainWindow::~MainWindow()
{
    delete ui;
}

void MainWindow::on_btnAddVPN_clicked()
{
    vpnProfileEditor *f = new vpnProfileEditor(nullptr, vpnProfileEditorModeNew);
    auto *prefWindow = openToolWindow(this, f, windowTitle() + QObject::tr(" - Add VPN"));

    connect(f, SIGNAL(vpnAdded(vpnProfile)), this, SLOT(onvpnAdded(vpnProfile)));
    prefWindow->show();
}

void MainWindow::on_btnDeleteVPN_clicked()
{
    QStandardItemModel *model = dynamic_cast<QStandardItemModel *>(ui->tvVpnProfiles->model());
    QItemSelectionModel *selmodel = ui->tvVpnProfiles->selectionModel();
    QModelIndexList sellist = selmodel->selectedRows(1);

    if(sellist.count() < 1)
        return;

    if(model->itemFromIndex(sellist.at(0))->parent() == nullptr)
        return;

    if(model->itemFromIndex(sellist.at(0))->parent()->data().toInt() == vpnProfile::Origin_GLOBAL)
    {
        QMessageBox::warning(this, tr("Delete VPN"),
                                        tr("Global VPN-Profiles cannot be deleted."),
                                        QMessageBox::Ok);

        return;
    }

    QString vpnName = model->itemFromIndex(sellist.at(0))->text();
    if(vpnName.isEmpty())
        return;

    vpnClientConnection *cl = vpnmanager->getClientConnection(vpnName);
    if(cl != 0 && cl->status != vpnClientConnection::STATUS_DISCONNECTED)
    {
        QMessageBox::warning(this, tr("Delete VPN"),
                                        tr("The VPN state must be disconnected to perform this action."),
                                        QMessageBox::Ok);

        return;
    }

    qDebug() << "MainWindow::on_btnDeleteVPN_clicked() -> remove vpn with name::" << vpnName;

    int ret = QMessageBox::warning(this, tr("Delete VPN"),
                                tr("Warning, the selected vpn will be deleted, continue?"),
                                QMessageBox::Yes | QMessageBox::No);

    switch(ret)
    {
    case QMessageBox::Yes:
        break;
    case QMessageBox::No:
    default:
        return;
    }

    tiConfVpnProfiles vpnss;
    if(vpnss.removeVpnProfileByName(vpnName))
    {
        refreshVpnProfileList();
    }
    else
    {
        QMessageBox::information(this, tr("Delete VPN"), tr("The selected vpn could not be deleted, an error occurred."));
    }
}

void MainWindow::on_btnEditVPN_clicked()
{
    QStandardItemModel *model = dynamic_cast<QStandardItemModel *>(ui->tvVpnProfiles->model());
    QItemSelectionModel *selmodel = ui->tvVpnProfiles->selectionModel();
    QModelIndexList sellist = selmodel->selectedRows(1);

    // Both of these are reached by a double-click when "connect on double-click"
    // is off, and both used to return without a trace -- which made a swallowed
    // double-click indistinguishable from one that arrived (issue #211).
    if(sellist.count() < 1)
    {
        qDebug() << "MainWindow::on_btnEditVPN_clicked() -> nothing selected, ignoring";
        return;
    }

    QString vpnName = model->itemFromIndex(sellist.at(0))->text();
    if(vpnName.isEmpty())
    {
        qDebug() << "MainWindow::on_btnEditVPN_clicked() -> selected row carries no profile name, ignoring";
        return;
    }

    vpnClientConnection *cl = vpnmanager->getClientConnection(vpnName);
    if(cl != 0 && cl->status != vpnClientConnection::STATUS_DISCONNECTED)
    {
        QMessageBox::warning(this, tr("Edit VPN"),
                                        tr("The VPN state must be disconnected to perform this action."),
                                        QMessageBox::Ok);

        return;
    }

    vpnProfileEditor *f = new vpnProfileEditor(nullptr, vpnProfileEditorModeEdit);
    f->loadVpnProfile(vpnName);
    auto *prefWindow = openToolWindow(this, f, windowTitle() + QObject::tr(" - Edit VPN"));

    connect(f, SIGNAL(vpnEdited(vpnProfile)), this, SLOT(onvpnEdited(vpnProfile)));
    prefWindow->show();
}

void MainWindow::on_btnCopyVPN_clicked()
{
    QStandardItemModel *model = dynamic_cast<QStandardItemModel *>(ui->tvVpnProfiles->model());
    QItemSelectionModel *selmodel = ui->tvVpnProfiles->selectionModel();
    QModelIndexList sellist = selmodel->selectedRows(1);

    if(sellist.count() < 1)
    {
        return;
    }

    QString vpnName = model->itemFromIndex(sellist.at(0))->text();
    if(vpnName.isEmpty())
        return;

    bool ok;
    QString vpnNameNew = QInputDialog::getText(this, tr("Copy VPN-profile"),
                                             tr("Enter the new VPN-profile name"), QLineEdit::Normal,
                                             "", &ok);

    if (ok && !vpnNameNew.isEmpty())
    {
        tiConfVpnProfiles profiles;
        if(!profiles.copyVpnProfile(vpnName, vpnNameNew))
            QMessageBox::warning(this, tr("Copy VPN-profile"), tr("The VPN-profile could not be copied."));
        refreshVpnProfileList();
    }
}

void MainWindow::on_tvVpnProfiles_doubleClicked([[maybe_unused]] const QModelIndex &index)
{
    tiConfMain main_settings;

    if(main_settings.getValue("gui/connect_on_dblclick").toBool())
        onStartVPN();
    else
        on_btnEditVPN_clicked();
}

void MainWindow::on_btnAddGroup_clicked()
{
    vpnGroupEditor *f = new vpnGroupEditor(nullptr, vpnGroupEditorModeNew);
    auto *prefWindow = openToolWindow(this, f, windowTitle() + QObject::tr(" - Add VPN-Group"));

    connect(f, SIGNAL(vpnGroupAdded(vpnGroup)), this, SLOT(onvpnGroupAdded(vpnGroup)));
    prefWindow->show();
}

void MainWindow::on_btnDeleteGroup_clicked()
{
    QStandardItemModel *model = dynamic_cast<QStandardItemModel *>(ui->tvVPNGroups->model());
    QItemSelectionModel *selmodel = ui->tvVPNGroups->selectionModel();
    QModelIndexList sellist = selmodel->selectedRows(1);

    if(sellist.count() < 1)
    {
        return;
    }

    QString vpnGroupName = model->itemFromIndex(sellist.at(0))->text();

    qDebug() << "MainWindow::on_btnDeleteGroup_clicked() -> remove vpngroup with name::" << vpnGroupName;

    int ret = QMessageBox::warning(this, tr("Delete VPN-Group"),
                                tr("Warning, the selected vpn-group will be deleted, continue?"),
                                QMessageBox::Yes | QMessageBox::No);

    switch(ret)
    {
    case QMessageBox::Yes:
        break;
    case QMessageBox::No:
    default:
        return;
    }

    tiConfVpnGroups vpngroupss;
    if(vpngroupss.removeVpnGroupByName(vpnGroupName))
    {
        refreshVpnGroupList();
    }
    else
    {
        QMessageBox::information(this, tr("Delete VPN-Group"), tr("The selected vpn-group could not be deleted, an error occurred."));
    }
}

void MainWindow::on_btnEditGroup_clicked()
{
    QStandardItemModel *model = dynamic_cast<QStandardItemModel *>(ui->tvVPNGroups->model());
    QItemSelectionModel *selmodel = ui->tvVPNGroups->selectionModel();
    QModelIndexList sellist = selmodel->selectedRows(1);

    if(sellist.count() < 1)
    {
        return;
    }

    QString vpnGroup = model->itemFromIndex(sellist.at(0))->text();

    vpnGroupEditor *f = new vpnGroupEditor(nullptr, vpnGroupEditorModeEdit);
    f->loadVpnGroup(vpnGroup);
    auto *prefWindow = openToolWindow(this, f, windowTitle() + QObject::tr(" - Edit VPN-Group"));

    connect(f, SIGNAL(vpnGroupEdited(vpnGroup)), this, SLOT(onvpnGroupEdited(vpnGroup)));
    prefWindow->show();
}

void MainWindow::on_btnCopyGroup_clicked()
{
    QStandardItemModel *model = dynamic_cast<QStandardItemModel *>(ui->tvVPNGroups->model());
    QItemSelectionModel *selmodel = ui->tvVPNGroups->selectionModel();
    QModelIndexList sellist = selmodel->selectedRows(1);

    if(sellist.count() < 1)
    {
        return;
    }

    QString vpnGroupName = model->itemFromIndex(sellist.at(0))->text();
    bool ok;
    QString vpnGroupNameNew = QInputDialog::getText(this, tr("Copy Group-profile"),
                                             tr("Enter the new Group-profile name"), QLineEdit::Normal,
                                             "", &ok);

    if (ok && !vpnGroupNameNew.isEmpty())
    {
        tiConfVpnGroups groups;
        groups.copyVpnGroup(vpnGroupName, vpnGroupNameNew);
        refreshVpnGroupList();
    }
}

void MainWindow::on_tvVPNGroups_doubleClicked([[maybe_unused]] const QModelIndex &index)
{
    tiConfMain main_settings;

    if(main_settings.getValue("gui/connect_on_dblclick").toBool())
        onStartVPN();
    else
        on_btnEditGroup_clicked();
}

void MainWindow::onTbActionEdit()
{
    if(ui->tabMain->currentIndex() == 0)
        on_btnEditVPN_clicked();
    else
        on_btnEditGroup_clicked();
}

void MainWindow::onTbActionCopy()
{
    if(ui->tabMain->currentIndex() == 0)
        on_btnCopyVPN_clicked();
    else
        on_btnCopyGroup_clicked();
}

void MainWindow::onTbActionDelete()
{
    if(ui->tabMain->currentIndex() == 0)
        on_btnDeleteVPN_clicked();
    else
        on_btnDeleteGroup_clicked();
}

void MainWindow::onTbActionSearch()
{
    tiConfMain main_settings;
    main_settings.setValue("main/show_search", ui->leSearch->isHidden());
    main_settings.sync();

    if(ui->leSearch->isHidden())
        ui->leSearch->show();
    else
        ui->leSearch->hide();
}

void MainWindow::onTbActionLogs()
{
    if(ui->tabMain->currentIndex() == 0)
    {
        QStandardItemModel *model = dynamic_cast<QStandardItemModel *>(ui->tvVpnProfiles->model());
        QItemSelectionModel *selmodel = ui->tvVpnProfiles->selectionModel();
        QModelIndexList sellist = selmodel->selectedRows(1);

        if(sellist.count() < 1)
        {
            return;
        }

        QString vpnName = model->itemFromIndex(sellist.at(0))->text();
        if(vpnName.isEmpty())
            return;

        QDesktopServices::openUrl(QUrl::fromLocalFile(QString("%1/logs/vpn/%2.log").arg(tiConfMain::getAppDir(), vpnName)));
    }
}

void MainWindow::onvpnAdded([[maybe_unused]] const vpnProfile &vpn)
{
    refreshVpnProfileList();
}

void MainWindow::onvpnEdited(const vpnProfile &vpn)
{
    QStandardItem *item_name = getVpnProfileItem(vpn.name, 1);
    QStandardItem *item_gateway = getVpnProfileItem(vpn.name, 2);
    QStandardItem *item_user = getVpnProfileItem(vpn.name, 3);

    if(item_name != nullptr && item_gateway != nullptr && item_user != nullptr)
    {
        item_name->setText(vpn.name);
        item_gateway->setText(vpn.gateway_host);
        item_user->setText(vpn.username);
    }
}

void MainWindow::onvpnGroupAdded([[maybe_unused]] const vpnGroup &vpngroup)
{
    refreshVpnGroupList();
}

void MainWindow::onvpnGroupEdited([[maybe_unused]] const vpnGroup &vpngroup)
{
    refreshVpnGroupList();
}

void MainWindow::onvpnSearch([[maybe_unused]] const QString &searchtext)
{
    refreshVpnProfileList();
}

void MainWindow::onStartVPN()
{
    qDebug() << "active-tab::" << ui->tabMain->currentIndex();

    // Tab-ID 0 = VPNs, Tab-ID 1 = VPN-Groups
    QTreeView *tree = (ui->tabMain->currentIndex() == 0) ? ui->tvVpnProfiles : ui->tvVPNGroups;

    QStandardItemModel *model = dynamic_cast<QStandardItemModel *>(tree->model());
    QItemSelectionModel *selmodel = tree->selectionModel();
    QModelIndexList sellist = selmodel->selectedRows(1);

    if(sellist.count() < 1)
    {
        qDebug() << "MainWindow::onStartVPN() -> nothing selected, ignoring";
        return;
    }

    QStandardItem *item = model->itemFromIndex(sellist.at(0));
    QString itemName = item->text();

    // A root row ("Local VPNs" / "Global VPNs") only fills column 0, so column 1
    // is empty here. Without this the empty name travelled all the way into
    // startVPN() and came back as "There is no VPN profile named ''" -- which is
    // how a double-click on a category used to be reported (issue #211).
    if(itemName.isEmpty())
    {
        qDebug() << "MainWindow::onStartVPN() -> selected row carries no profile name, ignoring";
        return;
    }

    if(ui->tabMain->currentIndex() == 1)
    {
        tiConfVpnGroups groups;
        vpnGroup *group = groups.getVpnGroupByName(itemName);
        for (const QString &member : group->localMembers)
        {
            onStartVPN(member, vpnProfile::Origin_LOCAL);
        }
        for (const QString &member : group->globalMembers)
        {
            onStartVPN(member, vpnProfile::Origin_GLOBAL);
        }
    }
    else
    {
        vpnProfile::Origin itemOrigin = static_cast<vpnProfile::Origin>(item->data().toInt());
        onStartVPN(itemName, itemOrigin);
    }
}

void MainWindow::onStartVPN(const QString &vpnname, [[maybe_unused]] vpnProfile::Origin origin)
{
    qDebug() << "start vpn:" << vpnname << "active-tab::" << ui->tabMain->currentIndex();

    vpnmanager->startVPN(vpnname);
}

void MainWindow::onActionStartVPN(const QString &vpnname)
{
    qDebug() << "action vpn pressed::" << vpnname;

    vpnClientConnection *conn = vpnmanager->getClientConnection(vpnname);
    if(conn != nullptr)
    {
        if(conn->status == vpnClientConnection::STATUS_DISCONNECTED)
            onStartVPN(vpnname);
        else
            onStopVPN(vpnname);
    }
    else
        onStartVPN(vpnname);
}

void MainWindow::onActionStartVPNGroup(const QString &vpnname)
{
    qDebug() << "action vpn-group pressed::" << vpnname;

    tiConfVpnGroups groups;
    vpnClientConnection *conn;
    vpnGroup *vpngroup = groups.getVpnGroupByName(vpnname);
    vpnClientConnection::connectionStatus vpnGroupStatus = vpnClientConnection::STATUS_DISCONNECTED;
    int connCount = 0;
    for (const QString &member : vpngroup->localMembers)
    {
        conn = vpnmanager->getClientConnection(member);
        if(conn != nullptr)
        {
            if(conn->status == vpnClientConnection::STATUS_CONNECTED)
                connCount+=1;
        }
    }

    if(connCount == vpngroup->localMembers.count())
        vpnGroupStatus = vpnClientConnection::STATUS_CONNECTED;
    else if(connCount < vpngroup->localMembers.count() && connCount > 0)
        vpnGroupStatus = vpnClientConnection::STATUS_CONNECTING;
    else
        vpnGroupStatus = vpnClientConnection::STATUS_DISCONNECTED;

    for (const QString &member : vpngroup->localMembers)
    {
        if(vpnGroupStatus == vpnClientConnection::STATUS_DISCONNECTED)
            vpnmanager->startVPN(member);
        else
            vpnmanager->stopVPN(member);
    }
}

void MainWindow::onStopVPN()
{
    qDebug() << "stop vpn::" << ui->tabMain->currentIndex();

    // Tab-ID 0 = VPNs, Tab-ID 1 = VPN-Groups
    QTreeView *tree = (ui->tabMain->currentIndex() == 0) ? ui->tvVpnProfiles : ui->tvVPNGroups;

    QStandardItemModel *model = dynamic_cast<QStandardItemModel *>(tree->model());
    QItemSelectionModel *selmodel = tree->selectionModel();
    QModelIndexList sellist = selmodel->selectedRows(1);

    if(sellist.count() < 1)
    {
        return;
    }

    QString itemName = model->itemFromIndex(sellist.at(0))->text();

    if(ui->tabMain->currentIndex() == 1)
    {
        tiConfVpnGroups groups;
        vpnGroup *group = groups.getVpnGroupByName(itemName);
        for (const QString &member : group->localMembers)
        {
            vpnmanager->stopVPN(member);
        }
        for (const QString &member : group->globalMembers)
        {
            vpnmanager->stopVPN(member);
        }
    }
    else
        vpnmanager->stopVPN(itemName);
}

void MainWindow::onStopVPN(const QString &vpnname)
{
    vpnmanager->stopVPN(vpnname);
}

void MainWindow::onQuit()
{
    tiConfMain main_settings;
    main_settings.setValue("gui/main_toolbar_location", this->toolBarArea(this->ui->tbActions));
    main_settings.sync();

    QCoreApplication::quit();
}

void MainWindow::onClientVPNStatusChanged(QString vpnname, vpnClientConnection::connectionStatus status)
{
    qDebug() << "MainWindow::onClientVPNStatusChanged::" << vpnname << "::status::" << status;

    //refreshVpnProfileList();
    tiConfMain main_settings;
    QIcon statusicon;
    QStandardItem *statusitem = getVpnProfileItem(vpnname, 0);
    if(statusitem != nullptr)
    {
        switch(status)
        {
        case vpnClientConnection::STATUS_CONNECTED:
            statusicon = QIcon(":/img/connected.png");
            statusitem->setIcon(statusicon);
            statusitem->setText(tr("Connected"));
            break;
        case vpnClientConnection::STATUS_CONNECTING:
            statusicon = QIcon(":/img/connecting.png");
            statusitem->setIcon(statusicon);
            statusitem->setText(tr("Connecting"));
            break;
        case vpnClientConnection::STATUS_DISCONNECTED:
        default:
            statusicon = QIcon(":/img/disconnected.png");
            statusitem->setIcon(statusicon);
            statusitem->setText(tr("Disconnected"));
        }

        if(trayItems.contains(vpnname))
            trayItems[vpnname]->setIcon(statusicon);
    }

    refreshVpnGroupList();

    if(vpnmanager->isSomeClientConnected())
        tray->setIcon(QIcon(":/img/app-enc.png"));
    else
        tray->setIcon(QIcon(":/img/app.png"));

    if(isHidden() && !main_settings.getValue("gui/disable_notifications", false).toBool())
    {
        switch(status)
        {
        case vpnClientConnection::STATUS_CONNECTED:
            tray->showMessage(tr("VPN-Status"), tr("VPN %1 is connected").arg(vpnname), QSystemTrayIcon::Information, 4000);
            break;
        case vpnClientConnection::STATUS_DISCONNECTED:
            tray->showMessage(tr("VPN-Status"), tr("VPN %1 is disconnected").arg(vpnname), QSystemTrayIcon::Information, 4000);
            break;
        case vpnClientConnection::STATUS_CONNECTING:
            break;
        }
    }
}

void MainWindow::onClientVPNCredRequest(QString vpnname)
{
    vpnLogin *f = new vpnLogin(nullptr);
    f->setData(vpnmanager, vpnname);
    auto *prefWindow = openToolWindow(this, f, windowTitle() + QObject::tr(" - Login"));
    f->initAfter();

    prefWindow->show();
    prefWindow->raise();
    QApplication::setActiveWindow(prefWindow);
}

void MainWindow::onClientVPNPromptRequest(QProcess *proc, int type)
{
    // Same one-line-to-stdin dialog for every prompt, only labelled for what is
    // actually being asked -- an OTP field is no help when openfortivpn wants
    // the passphrase for a private key (issue #166).
    QString title, question, fieldLabel;
    switch(type)
    {
    case vpnLogger::PROMPT_PEM_PASSPHRASE:
        title = QObject::tr(" - Certificate passphrase");
        question = tr("Please enter the pass phrase for your private key.");
        fieldLabel = tr("Pass phrase");
        break;
    case vpnLogger::PROMPT_OTP:
    default:
        title = QObject::tr(" - OTP-Login");
        question = tr("Please enter your OTP-login details.");
        fieldLabel = tr("OTP");
        break;
    }

    vpnOTPLogin *f = new vpnOTPLogin(nullptr);
    f->setData(proc);
    f->setPrompt(question, fieldLabel);
    auto *prefWindow = openToolWindow(this, f, windowTitle() + title);
    f->initAfter();

    prefWindow->show();
}

/*
 * The child's SAML listener is up (issue #186): open the browser for the
 * single sign-on. The child runs as root and must not start a browser; the log
 * line is only the trigger -- the URL is rebuilt here from the profile, so
 * nothing scraped out of process output is handed to the browser.
 */
void MainWindow::onClientSAMLAuthRequest(QString vpnname)
{
    tiConfVpnProfiles profiles;
    vpnProfile *profile = profiles.getVpnProfileByName(vpnname);
    if(profile == nullptr || !profile->saml_login)
        return;

    QUrl url;
    url.setScheme("https");
    url.setHost(profile->gateway_host);
    url.setPort(profile->gateway_port);
    url.setPath("/remote/saml/start");
    QUrlQuery query;
    query.addQueryItem("redirect", "1");
    if(!profile->realm.isEmpty())
        query.addQueryItem("realm", profile->realm);
    url.setQuery(query);

    /*
     * Info, not debug: debug logging is off by default, and when a gateway
     * answers the SAML request with an error the first question is always which
     * URL was actually opened -- realm included (issue #186). Nothing secret in
     * it, the child prints the same URL to its own log.
     */
    qInfo() << "SAML login for" << vpnname << "-> opening" << url.toString();

    if(QDesktopServices::openUrl(url))
    {
        tiConfMain main_settings;
        if(!main_settings.getValue("gui/disable_notifications", false).toBool())
            tray->showMessage(tr("VPN-Status"),
                              tr("SAML login for VPN %1: please finish signing in in your browser").arg(vpnname),
                              QSystemTrayIcon::Information, 6000);
    }
    else
    {
        // No browser could be started -- hand the user the URL instead. Not
        // modal: the child keeps waiting meanwhile.
        auto *box = new QMessageBox(QMessageBox::Information,
                                    tr("SAML login"),
                                    tr("Please open this URL in your browser to sign in for VPN %1:\n\n%2")
                                    .arg(vpnname, url.toString()),
                                    QMessageBox::Ok, this);
        box->setTextInteractionFlags(Qt::TextSelectableByMouse);
        box->setAttribute(Qt::WA_DeleteOnClose);
        box->show();
    }
}

void MainWindow::onClientCertValidationFAiled(QString vpnname, QString buffer)
{
    QString hash = "", info = "";
    QRegularExpression reHash("--trusted-cert (.*?)\n");
    QRegularExpressionMatch matchHash = reHash.match(buffer);
    if(matchHash.hasMatch())
        hash = matchHash.captured(1);

    QRegularExpression reInfo("Gateway certificate:\n(.*?)sha256 digest:\n", QRegularExpression::DotMatchesEverythingOption);
    QRegularExpressionMatch matchInfo = reInfo.match(buffer);
    if(matchInfo.hasMatch())
        info = matchInfo.captured(1).replace("ERROR:", "");

    if(hash.isEmpty())
        return;

    info.prepend(tr("Gateway certificate validation failed and the certificate digest is not in the local whitelist nor a valid CA is provided. Certificate details:\n\n"));

    tiConfMain main_settings;
    bool disallowUnsecureCertificates = main_settings.getValue("main/disallow_unsecure_certificates").toBool();
    if(disallowUnsecureCertificates)
    {
        QMessageBox::critical(this, tr("Gateway certificate validation failed"), info);
        return;
    }

    info.append(tr("\n\nAdd certificate to VPN-profile whitelist?"));

    tiConfVpnProfiles profiles;
    profiles.setReadProfilePasswords(true);
    vpnProfile *profile = profiles.getVpnProfileByName(vpnname);
    if(profile == nullptr)
    {
        qWarning() << "onClientCertValidationFAiled:: VPN profile not found:" << vpnname;
        return;
    }

    if(profile->trust_all_gw_certs)
    {
        tiConfMain cmain;
        cmain.saveGwCertCache(vpnname, hash);

        if(!hash.isEmpty())
            restartVPNWhenClosed(vpnname);
    }
    else
    {
        if(QMessageBox::question(this, tr("Gateway certificate validation failed"), info) == QMessageBox::Yes)
        {
            profile->trusted_cert = hash;
            if(!profiles.saveVpnProfile(*profile))
            {
                QMessageBox::warning(this, tr("Gateway certificate validation failed"),
                                     tr("The certificate could not be stored in the VPN-profile. Is the system password store locked?"));
                return;
            }

            /*
             * Reconnect right away. Storing the hash used to be all that
             * happened here, so from the user's side confirming the dialog did
             * nothing at all and the connection had to be started again by hand
             * -- which read as "the certificate is being ignored"
             * (issues #184, #159).
             */
            restartVPNWhenClosed(vpnname);
        }
    }
}

/*
 * Start a VPN again as soon as its previous connection has gone away. Used after
 * a gateway certificate was accepted: the failed attempt is still shutting down
 * at that point, and startVPN() would refuse while the name is still in
 * vpnManager::connections.
 */
void MainWindow::restartVPNWhenClosed(const QString &vpnname)
{
    int maxwait = 30, curwait = 0;
    QTimer *timer = new QTimer(this);
    connect(timer, &QTimer::timeout, [=]() mutable {
        if(curwait < maxwait)
        {
            curwait += 1;
            if(vpnmanager->getClientConnection(vpnname) == nullptr) {
                vpnmanager->startVPN(vpnname);
                timer->stop();
                timer->deleteLater();
            }
        }
        else
        {
            timer->stop();
            timer->deleteLater();
        }
    });
    timer->start(300);
}

void MainWindow::onClientVPNStatsUpdate(QString vpnname, vpnStats stats)
{
    vpnClientConnection *conn = vpnmanager->getClientConnection(vpnname);
    QStandardItem *item_stats = getVpnProfileItem(vpnname, 5);

    if(conn != nullptr && item_stats != nullptr)
    {
        QString disp = QString("%1 / %2").arg(vpnHelper::formatByteUnits(stats.bytes_read)).arg(vpnHelper::formatByteUnits(stats.bytes_written));
        item_stats->setText(disp);
    }

    // How long the tunnel has been up -- useful where the gateway enforces a
    // maximum session time (issue #185).
    QStandardItem *item_uptime = getVpnProfileItem(vpnname, 6);
    if(conn != nullptr && item_uptime != nullptr)
        item_uptime->setText(vpnHelper::formatDuration(stats.vpn_start));
}

void MainWindow::onClientVPNMessage([[maybe_unused]] QString vpnname, vpnMsg msg)
{
    // Both carry program output, so neither may be interpreted as markup.
    QString stext = QString("<b>%1</b>").arg(msg.msg.toHtmlEscaped());
    if(!msg.detail.isEmpty())
        stext.append("<br><br>Detail:<br><pre>").append(msg.detail.toHtmlEscaped()).append("</pre>");

    switch(msg.type)
    {
    case vpnMsg::TYPE_ERROR:
        QMessageBox::critical(this, tr("Error"), stext);
        break;
    case vpnMsg::TYPE_WARNING:
        QMessageBox::warning(this, tr("Warning"), stext);
        break;
    case vpnMsg::TYPE_INFO:
    default:
        QMessageBox::information(this, tr("Information"), stext);
        break;
    }
}

void MainWindow::ontvVpnProfilesCustomContextMenu(const QPoint &point)
{
    QModelIndex index = ui->tvVpnProfiles->indexAt(point);
    QStandardItemModel *model = dynamic_cast<QStandardItemModel *>(ui->tvVpnProfiles->model());

    QStandardItem *item = model->itemFromIndex(index.sibling(index.row(), 1));
    if(item == nullptr)
        return;

    QString vpnname = item->text();
    if(vpnname.isEmpty())
        return;

    QMenu menu;
    QAction *a_connect = menu.addAction(QIcon(":/img/connected.png"), tr("Connect"));
    QAction *a_disconnect = menu.addAction(QIcon(":/img/disconnected.png"), tr("Disconnect"));
    menu.addSeparator();
    QAction *a_edit = menu.addAction(QIcon::fromTheme("edit-paste", QIcon(":/img/edit.png")), tr("Edit"));
    QAction *a_copy = menu.addAction(QIcon::fromTheme("edit-copy", QIcon(":/img/copy.png")), tr("Copy"));
    QAction *a_delete = menu.addAction(QIcon::fromTheme("edit-delete", QIcon(":/img/delete.png")), tr("Delete"));
    menu.addSeparator();
    QAction *a_viewlogs = menu.addAction(QIcon::fromTheme("accessories-text-editor", QIcon(":/img/log.png")), tr("View logs"));
    QAction *chosen = menu.exec(ui->tvVpnProfiles->mapToGlobal(point));

    if(chosen == a_viewlogs)
    {
        QDesktopServices::openUrl(QUrl::fromLocalFile(QString("%1/logs/vpn/%2.log").arg(tiConfMain::getAppDir(), vpnname)));
        return;
    }

    if(chosen == a_connect)
    {
        onStartVPN(vpnname);
        return;
    }

    if(chosen == a_disconnect)
    {
        onStopVPN(vpnname);
        return;
    }

    if(chosen == a_edit)
    {
        on_btnEditVPN_clicked();
        return;
    }

    if(chosen == a_copy)
    {
        on_btnCopyVPN_clicked();
        return;
    }

    if(chosen == a_delete)
    {
        on_btnDeleteVPN_clicked();
        return;
    }
}

void MainWindow::showMainWindow()
{
    show();
    raise();
    // activateWindow() rather than QApplication::setActiveWindow(), which is
    // deprecated as of Qt 6.7. Identical behaviour. Note that under Wayland a
    // client cannot raise or focus itself without an activation token, so both
    // of these are no-ops there.
    activateWindow();
}

MainWindow::TASKBAR_POSITION MainWindow::taskbarPosition()
{
    QScreen *screen = QGuiApplication::primaryScreen();
    QRect geo = screen->geometry();
    QRect geoAvail = screen->availableGeometry();

    return (geoAvail.top() > geo.top()) ? MainWindow::TASKBAR_POSITION_TOP : MainWindow::TASKBAR_POSITION_BOTTOM;
}

void MainWindow::refreshVpnProfileList()
{
    tiConfVpnProfiles vpnss;
    vpnss.readVpnProfiles();

    // Get current selected item
    QString curSelectedItem = "";
    QStandardItemModel *model = dynamic_cast<QStandardItemModel *>(ui->tvVpnProfiles->model());
    QItemSelectionModel *selmodel = ui->tvVpnProfiles->selectionModel();
    QModelIndexList sellist = selmodel->selectedRows(1);

    if(sellist.count() < 1) {
        curSelectedItem = "";
    } else {
        curSelectedItem = model->itemFromIndex(sellist.at(0))->text();
    }

    root_local_vpn->removeRows(0, root_local_vpn->rowCount());
    root_global_vpn->removeRows(0, root_global_vpn->rowCount());

    QStandardItem *item = nullptr;
    QStandardItem *item2 = nullptr;
    QStandardItem *item3 = nullptr;
    QStandardItem *item4 = nullptr;
    QStandardItem *item5 = nullptr;
    QStandardItem *item6 = nullptr;
    int localRow = 0, globalRow = 0;

    if(tray_menu == nullptr)
        tray_menu = new QMenu();
    tray_menu->clear();
    if(MainWindow::taskbarPosition() == MainWindow::TASKBAR_POSITION_TOP)
    {
        tray_menu->addAction(QIcon::fromTheme("application-exit", QIcon(":/img/quit.png")), tr("Quit OpenFortiGUI"), this, SLOT(onQuit()));
        tray_menu->addAction(QIcon::fromTheme("preferences-system", QIcon(":/img/settings.png")), tr("Settings"), this, SLOT(onVPNSettings()));
        // Same entry point as the tray left-click and as the ACTION_SHOW_MAIN
        // request of a second instance. A plain show() neither raises nor
        // activates, so the entry looked dead whenever the main window was
        // merely behind another one.
        tray_menu->addAction(QIcon::fromTheme("window-new", QIcon(":/img/show.png")), tr("Show mainwindow"), this, SLOT(showMainWindow()));
        tray_menu->addSeparator();
        tray_menu->addMenu(tray_group_menu);
        tray_menu->addSeparator();
    }

    ui->tvVpnProfiles->setSortingEnabled(false);

    QString filter = ui->leSearch->text();
    trayItems.clear();
    QList<vpnProfile*> vpns = vpnss.getVpnProfiles();
    bool isVPNConnected = false;
    for(int i=0; i < vpns.count(); i++)
    {
        vpnProfile *vpn = vpns.at(i);
        qDebug() << "MainWindow::refreshVpnProfileList() -> vpnprofiles found::" << vpn->name;

        if(!filter.isEmpty())
        {
            if(!vpn->name.contains(filter, Qt::CaseInsensitive) && !vpn->gateway_host.contains(filter, Qt::CaseInsensitive))
                continue;
        }

        QIcon status;
        vpnClientConnection *conn = vpnmanager->getClientConnection(vpn->name);
        if(conn != nullptr)
        {
            switch(conn->status)
            {
            case vpnClientConnection::STATUS_CONNECTED:
                status = QIcon(":/img/connected.png");
                item4 = new QStandardItem(status, tr("Connected"));
                isVPNConnected = true;
                break;
            case vpnClientConnection::STATUS_CONNECTING:
                status = QIcon(":/img/connecting.png");
                item4 = new QStandardItem(status, tr("Connecting"));
                break;
            case vpnClientConnection::STATUS_DISCONNECTED:
            default:
                status = QIcon(":/img/disconnected.png");
                item4 = new QStandardItem(status, tr("Disconnected"));
            }
        }
        else
        {
            status = QIcon(":/img/disconnected.png");
            item4 = new QStandardItem(status, tr("Disconnected"));
        }

        item = new QStandardItem(vpn->name);
        item2 = new QStandardItem(vpn->gateway_host);
        item3 = new QStandardItem(vpn->username);
        item5 = new QStandardItem();
        item6 = new QStandardItem();
        switch(vpn->device_type)
        {
        case vpnProfile::Device_Barracuda:
            item6->setText("Barracuda");
            break;
        case vpnProfile::Device_Fortigate:
        default:
            item6->setText("Fortigate");
            break;
        }

        switch(vpn->origin_location)
        {
            case vpnProfile::Origin_LOCAL:
            {
                localRow = root_local_vpn->rowCount();
                root_local_vpn->setChild(localRow, 0, item4);
                root_local_vpn->setChild(localRow, 1, item);
                root_local_vpn->setChild(localRow, 2, item6);
                root_local_vpn->setChild(localRow, 3, item2);
                root_local_vpn->setChild(localRow, 4, item3);
                root_local_vpn->setChild(localRow, 5, item5);
                item->setData(vpnProfile::Origin_LOCAL);
                break;
            }
            case vpnProfile::Origin_GLOBAL:
            {
                globalRow = root_global_vpn->rowCount();
                root_global_vpn->setChild(globalRow, 0, item4);
                root_global_vpn->setChild(globalRow, 1, item);
                root_global_vpn->setChild(globalRow, 2, item6);
                root_global_vpn->setChild(globalRow, 3, item2);
                root_global_vpn->setChild(globalRow, 4, item3);
                root_global_vpn->setChild(globalRow, 5, item5);
                item->setData(vpnProfile::Origin_GLOBAL);
                break;
            }
        case vpnProfile::Origin_BOTH:
            break;
        }
        // Update tray
        if(isVPNConnected)
            tray->setIcon(QIcon(":/img/app-enc.png"));
        else
            tray->setIcon(QIcon(":/img/app.png"));

        // Menu
        QAction *action = new QAction(status, vpn->name, tray_menu);
        connect(action, SIGNAL(triggered(bool)), signalMapper, SLOT(map()));
        signalMapper->setMapping(action, vpn->name);
        trayItems[vpn->name] = action;
    }

    for (auto it = trayItems.cbegin(); it != trayItems.cend(); ++it)
    {
        tray_menu->insertAction(nullptr, it.value());
    }

    if(MainWindow::taskbarPosition() == MainWindow::TASKBAR_POSITION_BOTTOM)
    {
        tray_menu->addSeparator();
        tray_menu->addMenu(tray_group_menu);
        tray_menu->addSeparator();
        tray_menu->addAction(QIcon::fromTheme("window-new", QIcon(":/img/show.png")), tr("Show mainwindow"), this, SLOT(showMainWindow()));
        tray_menu->addAction(QIcon::fromTheme("preferences-system", QIcon(":/img/settings.png")), tr("Settings"), this, SLOT(onVPNSettings()));
        tray_menu->addAction(QIcon::fromTheme("application-exit", QIcon(":/img/quit.png")), tr("Quit OpenFortiGUI"), this, SLOT(onQuit()));
    }

    // Always the same menu object. Re-setting it makes the tray host tear the
    // exported menu down and build it up again, which is visible as a flicker
    // on the D-Bus based trays (issue #210).
    if(tray->contextMenu() != tray_menu)
        tray->setContextMenu(tray_menu);

    ui->tvVpnProfiles->setSortingEnabled(true);
    ui->tvVpnProfiles->sortByColumn(1, Qt::AscendingOrder);

    if(!curSelectedItem.isEmpty())
    {
        QModelIndex qmi = model->indexFromItem(getVpnProfileItem(curSelectedItem, 1));
        selmodel->clear();
        selmodel->setCurrentIndex(qmi, QItemSelectionModel::SelectionFlag::Rows | QItemSelectionModel::SelectionFlag::Select);
    }
}

void MainWindow::refreshVpnGroupList()
{
    QStandardItemModel *model = dynamic_cast<QStandardItemModel *>(ui->tvVPNGroups->model());
    tiConfVpnGroups vpngroupss;
    vpngroupss.readVpnGroups();

    model->removeRows(0, model->rowCount());
    ui->tvVPNGroups->setSortingEnabled(false);

    QStandardItem *item = nullptr;
    QStandardItem *item2 = nullptr;
    QStandardItem *item3 = nullptr;
    int row = model->rowCount();

    vpnClientConnection *conn;
    QList<vpnGroup*> vpngroups = vpngroupss.getVpnGroups();

    /*
     * Only rebuild the submenu when the groups themselves changed. This runs on
     * every VPN status change (onClientVPNStatusChanged), and clear() deletes
     * the actions -- including the one the user is about to click, so the
     * submenu fell apart under the cursor while a connection was coming up
     * (issue #210). Otherwise just patch the status icons of what is there.
     *
     * QMap::keys() comes back sorted by key, hence the sort() on the names.
     */
    QStringList groupNames;
    for(vpnGroup *vpngroup : vpngroups)
        groupNames << vpngroup->name;
    groupNames.sort();

    const bool rebuildMenu = (groupNames != trayGroupItems.keys());
    if(rebuildMenu)
    {
        tray_group_menu->clear();
        trayGroupItems.clear();
    }

    for(int i=0; i < vpngroups.count(); i++)
    {
        vpnGroup *vpngroup = vpngroups.at(i);
        qDebug() << "MainWindow::refreshVpnGroupList() -> vpngroups found::" << vpngroup->name;

        QIcon status;
        vpnClientConnection::connectionStatus vpnGroupStatus = vpnClientConnection::STATUS_DISCONNECTED;
        int connCount = 0;
        for (const QString &member : vpngroup->localMembers)
        {
            conn = vpnmanager->getClientConnection(member);
            if(conn != nullptr)
            {
                if(conn->status == vpnClientConnection::STATUS_CONNECTED)
                    connCount+=1;
            }
        }
        for (const QString &member : vpngroup->globalMembers)
        {
            conn = vpnmanager->getClientConnection(member);
            if(conn != nullptr)
            {
                if(conn->status == vpnClientConnection::STATUS_CONNECTED)
                    connCount+=1;
            }
        }

        int totalCount = vpngroup->localMembers.count() + vpngroup->globalMembers.count();

        if(connCount == totalCount)
            vpnGroupStatus = vpnClientConnection::STATUS_CONNECTED;
        else if(connCount < totalCount && connCount > 0)
            vpnGroupStatus = vpnClientConnection::STATUS_CONNECTING;
        else
            vpnGroupStatus = vpnClientConnection::STATUS_DISCONNECTED;

        switch(vpnGroupStatus)
        {
        case vpnClientConnection::STATUS_CONNECTED:
            status = QIcon(":/img/connected.png");
            break;
        case vpnClientConnection::STATUS_CONNECTING:
            status = QIcon(":/img/connecting.png");
            break;
        case vpnClientConnection::STATUS_DISCONNECTED:
        default:
            status = QIcon(":/img/disconnected.png");
        }

        item = new QStandardItem(vpngroup->name);
        item2 = new QStandardItem(vpngroup->localMembers.join(", ") + ", " + vpngroup->globalMembers.join(", "));
        item3 = new QStandardItem(status, "");

        row = model->rowCount();
        model->setItem(row, 0, item3);
        model->setItem(row, 1, item);
        model->setItem(row, 2, item2);

        // Menu
        if(rebuildMenu)
        {
            QAction *action = new QAction(status, vpngroup->name, tray_group_menu);
            connect(action, SIGNAL(triggered(bool)), signalMapperGroups, SLOT(map()));
            signalMapperGroups->setMapping(action, vpngroup->name);
            trayGroupItems[vpngroup->name] = action;
        }
        else
        {
            trayGroupItems[vpngroup->name]->setIcon(status);
        }
    }

    if(rebuildMenu)
    {
        for (auto it = trayGroupItems.cbegin(); it != trayGroupItems.cend(); ++it)
        {
            tray_group_menu->insertAction(nullptr, it.value());
        }
    }

    ui->tvVPNGroups->header()->resizeSection(0, 50);
    ui->tvVPNGroups->header()->resizeSection(1, 150);
    ui->tvVPNGroups->setSortingEnabled(true);
    ui->tvVPNGroups->sortByColumn(1, Qt::AscendingOrder);
}

void MainWindow::autostartVPNs()
{
    tiConfVpnProfiles vpnss;
    vpnss.readVpnProfiles();
    QList<vpnProfile*> vpns = vpnss.getVpnProfiles();
    for(int i=0; i < vpns.count(); i++)
    {
        vpnProfile *vpn = vpns.at(i);

        if(vpn->autostart)
            vpnmanager->startVPN(vpn->name);
    }
}

QStandardItem *MainWindow::getVpnProfileItem(const QString &vpnname, int column)
{
    QStandardItem *retitem = nullptr;

    for(int i=0; i < root_local_vpn->rowCount(); i++)
    {
        if(root_local_vpn->child(i, 1)->text() == vpnname)
        {
            return root_local_vpn->child(i, column);
        }
    }

    if(retitem == nullptr)
    {
        for(int i=0; i < root_global_vpn->rowCount(); i++)
        {
            if(root_global_vpn->child(i, 1)->text() == vpnname)
            {
                return root_global_vpn->child(i, column);
            }
        }
    }

    return retitem;
}

bool MainWindow::eventFilter(QObject *object, QEvent *event)
{
    if(object == this && event->type() == QEvent::Close)
    {
        hide();

        event->ignore();
        return true;
    }

    return false;
}

void MainWindow::onActionAbout()
{
    QMessageBox::about(this, tr("About openFortiGUI"), tr("<b>openFortiGUI %1</b><br>"
                                                             "<table><tr><td width='150'>Developer:</td> <td><b>Rene Hadler</b></td></tr>"
                                                             "<tr><td>eMail:</td> <td> <a href=mailto:'rene@hadler.me'>rene@hadler.me</a></td></tr>"
                                                             "<tr><td>Website:</td> <td> <a href=https://hadler.me>https://hadler.me</a></td></tr></table>"
                                                             "<p>This program uses following libs/resources:</p>"
                                                              "<table><tr><td width='150'>GCC %2:</td> <td> <a href='https://gcc.gnu.org/'>https://gcc.gnu.org</a></td></tr>"
                                                              "<tr><td>QT %3:</td> <td> <a href='https://www.qt.io'>https://www.qt.io</a></td></tr>"
                                                              "<tr><td>openfortivpn:</td> <td> <a href='https://github.com/adrienverge/openfortivpn'>https://github.com/adrienverge/openfortivpn</a></td></tr>"
                                                              "<tr><td>Icons8:</td> <td> <a href='https://icons8.com/'>https://icons8.com</a></td></tr>"
                                                              "<tr><td>App-Icon:</td> <td> <a href='https://deepdoc.at/'>https://deepdoc.at</a></td></tr></table>").arg(openfortigui_config::version, __VERSION__, QT_VERSION_STR));
}

void MainWindow::onTrayIconActivated(QSystemTrayIcon::ActivationReason reason)
{
    if(reason == QSystemTrayIcon::Trigger)
    {
        if(isHidden())
        {
            showMainWindow();
        }
        else
            hide();
    }
}

void MainWindow::onVPNSettings()
{
    vpnSetting *f = new vpnSetting(nullptr);
    auto *prefWindow = openToolWindow(this, f, windowTitle() + QObject::tr(" - Settings"));

    prefWindow->show();
}

void MainWindow::onSetupWizard()
{
    setupWizard *f = new setupWizard(nullptr);
    auto *prefWindow = openToolWindow(this, f, windowTitle() + QObject::tr(" - Setup wizard"));

    prefWindow->show();
    prefWindow->raise();
    QApplication::setActiveWindow(prefWindow);
}

void MainWindow::onActionLogs()
{
    QDesktopServices::openUrl(QUrl::fromLocalFile(QString("%1/logs/").arg(tiConfMain::getAppDir())));
}

void MainWindow::onChangelog()
{
    vpnChangelog *f = new vpnChangelog(nullptr);
    auto *changeWindow = openToolWindow(this, f, windowTitle() + QObject::tr(" - Changelog"));
    f->initAfter();

    changeWindow->show();
    changeWindow->raise();
    QApplication::setActiveWindow(changeWindow);
}

void MainWindow::onWatcherVpnProfilesChanged([[maybe_unused]] const QString &path)
{
    // start() on a running timer restarts it, so a burst of events collapses
    // into a single refresh 300 ms after the last of them (issue #210).
    timerVpnProfilesRefresh->start();
}

void MainWindow::onVpnProfilesRefreshTimeout()
{
    setupVpnProfileWatcher();

    refreshVpnProfileList();
    refreshVpnGroupList();
}

/*
 * Watch the three profile directories, adding only what is not watched yet.
 *
 * Qt drops a watch when the watched directory itself disappears, so a profile
 * directory replaced wholesale -- by a restore or a sync tool -- would never be
 * reported again. initMainConf() recreates the directories, so re-adding what is
 * missing after every event is enough. addPath() on a path that is still watched
 * only logs a warning, hence the containment check.
 */
void MainWindow::setupVpnProfileWatcher()
{
    tiConfMain main_settings;
    const QStringList paths = {
        tiConfMain::formatPath(main_settings.getValue("paths/globalvpnprofiles").toString()),
        tiConfMain::formatPath(main_settings.getValue("paths/localvpnprofiles").toString()),
        tiConfMain::formatPath(main_settings.getValue("paths/localvpngroups").toString())
    };

    const QStringList watched = watcherVpnProfiles->directories();
    for(const QString &path : paths)
        if(!watched.contains(path))
            watcherVpnProfiles->addPath(path);
}
