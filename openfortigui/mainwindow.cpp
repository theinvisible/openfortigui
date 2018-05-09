#include "mainwindow.h"
#include "ui_mainwindow.h"

#include <QThread>
#include <QProcess>
#include <QTextStream>
#include <QDesktopWidget>
#include <QStandardItemModel>
#include <QMessageBox>
#include <QInputDialog>
#include <QScreen>
#include <QToolButton>
#include <QDesktopServices>
#include <QUrl>

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

vpnManager *MainWindow::vpnmanager = 0;

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
    connect(vpnmanager, SIGNAL(VPNOTPRequest(QProcess*)), this, SLOT(onClientVPNOTPRequest(QProcess*)));

    signalMapper = new QSignalMapper(this);
    connect(signalMapper, SIGNAL(mapped(QString)), this, SLOT(onActionStartVPN(QString)));
    signalMapperGroups = new QSignalMapper(this);
    connect(signalMapperGroups, SIGNAL(mapped(QString)), this, SLOT(onActionStartVPNGroup(QString)));

    // Center window on startup
    QRect geom = QApplication::desktop()->availableGeometry();
    move((geom.width() - width()) / 2, (geom.height() - height()) / 2);

    // Treeview VPNs
    QStringList headers;
    headers << tr("Status") << tr("Name") << tr("Gateway") << tr("User") << tr("Traffic RX/TX");
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
    ui->tvVpnProfiles->header()->resizeSection(1, 150);
    ui->tvVpnProfiles->header()->resizeSection(2, 300);

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
    tray_group_menu = new QMenu(trUtf8("VPN-Groups"));
    connect(tray, SIGNAL(activated(QSystemTrayIcon::ActivationReason)), this, SLOT(onTrayIconActivated(QSystemTrayIcon::ActivationReason)));

    QToolButton *tbtnAdd = new QToolButton();
    QMenu *tmnuAdd = new QMenu(tbtnAdd);
    tmnuAdd->addAction(tr("VPN"), this, SLOT(on_btnAddVPN_clicked()));
    tmnuAdd->addAction(tr("VPN-Group"), this, SLOT(on_btnAddGroup_clicked()));
    tbtnAdd->setPopupMode(QToolButton::InstantPopup);
    tbtnAdd->setToolButtonStyle(Qt::ToolButtonTextUnderIcon);
    tbtnAdd->setMenu(tmnuAdd);
    tbtnAdd->setIcon(QIcon(":/img/add.png"));
    tbtnAdd->setText(tr("Add"));

    ui->tbActions->addAction(QIcon(":/img/connected.png"), trUtf8("Connect"), this, SLOT(onStartVPN()));
    ui->tbActions->addAction(QIcon(":/img/disconnected.png"), trUtf8("Disconnect"), this, SLOT(onStopVPN()));
    ui->tbActions->addSeparator();
    ui->tbActions->addWidget(tbtnAdd);
    ui->tbActions->addAction(QIcon(":/img/edit.png"), trUtf8("Edit"), this, SLOT(onTbActionEdit()));
    ui->tbActions->addAction(QIcon(":/img/copy.png"), trUtf8("Copy"), this, SLOT(onTbActionCopy()));
    QAction *actionSearch = ui->tbActions->addAction(QIcon(":/img/search.png"), trUtf8("Search"), this, SLOT(onTbActionSearch()));
    actionSearch->setCheckable(true);
    ui->tbActions->addSeparator();
    ui->tbActions->addAction(QIcon(":/img/delete.png"), trUtf8("Delete"), this, SLOT(onTbActionDelete()));
    ui->tbActions->addSeparator();
    ui->tbActions->addAction(QIcon(":/img/about.png"), trUtf8("About"), this, SLOT(onActionAbout()));

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
    watcherVpnProfiles = new QFileSystemWatcher(this);
    watcherVpnProfiles->addPath(tiConfMain::formatPath(main_settings.getValue("paths/globalvpnprofiles").toString()));
    watcherVpnProfiles->addPath(tiConfMain::formatPath(main_settings.getValue("paths/localvpnprofiles").toString()));
    watcherVpnProfiles->addPath(tiConfMain::formatPath(main_settings.getValue("paths/localvpngroups").toString()));
    connect(watcherVpnProfiles, SIGNAL(directoryChanged(QString)), this, SLOT(onWatcherVpnProfilesChanged(QString)));

    autostartVPNs();

    if(!main_settings.getValue("main/setupwizard").toBool())
        onSetupWizard();

    if(main_settings.getValue("main/changelogrev_read", 0).toInt() == 0 || openfortigui_config::changelogRev > main_settings.getValue("main/changelogrev_read", 0).toInt())
        onChangelog();

    if(main_settings.getValue("main/show_search").toBool())
    {
        actionSearch->setChecked(true);
        ui->leSearch->show();
    }
}

MainWindow::~MainWindow()
{
    delete ui;
}

void MainWindow::on_btnAddVPN_clicked()
{
    QMainWindow *prefWindow = new QMainWindow(this, Qt::Dialog);
    prefWindow->setWindowModality(Qt::WindowModal);

    vpnProfileEditor *f = new vpnProfileEditor(prefWindow, vpnProfileEditorModeNew);
    prefWindow->setCentralWidget(f);
    prefWindow->setMinimumSize(QSize(f->width(),f->height()));
    //prefWindow->setMaximumSize(QSize(f->width(),f->height()));
    prefWindow->setWindowTitle(windowTitle() + QObject::trUtf8(" - Add VPN"));

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

    if(model->itemFromIndex(sellist.at(0))->parent() == 0)
        return;

    if(model->itemFromIndex(sellist.at(0))->parent()->data().toInt() == vpnProfile::Origin_GLOBAL)
    {
        QMessageBox::warning(this, trUtf8("Delete VPN"),
                                        trUtf8("Global VPN-Profiles cannot be deleted."),
                                        QMessageBox::Ok);

        return;
    }

    QString vpnName = model->itemFromIndex(sellist.at(0))->text();
    if(vpnName.isEmpty())
        return;

    vpnClientConnection *cl = vpnmanager->getClientConnection(vpnName);
    if(cl != 0 && cl->status != vpnClientConnection::STATUS_DISCONNECTED)
    {
        QMessageBox::warning(this, trUtf8("Delete VPN"),
                                        trUtf8("The VPN state must be disconnected to perform this action."),
                                        QMessageBox::Ok);

        return;
    }

    qDebug() << "MainWindow::on_btnDeleteVPN_clicked() -> remove vpn with name::" << vpnName;

    int ret = QMessageBox::warning(this, trUtf8("Delete VPN"),
                                trUtf8("Warning, the selected vpn will be deleted, continue?"),
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
        QMessageBox::information(this, trUtf8("Delete VPN"), trUtf8("The selected vpn could not be deleted, an error occured."));
    }
}

void MainWindow::on_btnEditVPN_clicked()
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

    vpnClientConnection *cl = vpnmanager->getClientConnection(vpnName);
    if(cl != 0 && cl->status != vpnClientConnection::STATUS_DISCONNECTED)
    {
        QMessageBox::warning(this, trUtf8("Edit VPN"),
                                        trUtf8("The VPN state must be disconnected to perform this action."),
                                        QMessageBox::Ok);

        return;
    }

    QMainWindow *prefWindow = new QMainWindow(this, Qt::Dialog);
    prefWindow->setWindowModality(Qt::WindowModal);

    vpnProfileEditor *f = new vpnProfileEditor(prefWindow, vpnProfileEditorModeEdit);
    f->loadVpnProfile(vpnName);
    prefWindow->setCentralWidget(f);
    prefWindow->setMinimumSize(QSize(f->width(),f->height()));
    //prefWindow->setMaximumSize(QSize(f->width(),f->height()));
    prefWindow->setWindowTitle(windowTitle() + QObject::trUtf8(" - Edit VPN"));

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
        profiles.copyVpnProfile(vpnName, vpnNameNew);
        refreshVpnProfileList();
    }
}

void MainWindow::on_tvVpnProfiles_doubleClicked(const QModelIndex &index)
{
    on_btnEditVPN_clicked();
}

void MainWindow::on_btnAddGroup_clicked()
{
    QMainWindow *prefWindow = new QMainWindow(this, Qt::Dialog);
    prefWindow->setWindowModality(Qt::WindowModal);

    vpnGroupEditor *f = new vpnGroupEditor(prefWindow, vpnGroupEditorModeNew);
    prefWindow->setCentralWidget(f);
    prefWindow->setMinimumSize(QSize(f->width(),f->height()));
    //prefWindow->setMaximumSize(QSize(f->width(),f->height()));
    prefWindow->setWindowTitle(windowTitle() + QObject::trUtf8(" - Add VPN-Group"));

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

    int ret = QMessageBox::warning(this, trUtf8("Delete VPN-Group"),
                                trUtf8("Warning, the selected vpn-group will be deleted, continue?"),
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
        QMessageBox::information(this, trUtf8("Delete VPN-Group"), trUtf8("The selected vpn-group could not be deleted, an error occured."));
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

    QMainWindow *prefWindow = new QMainWindow(this, Qt::Dialog);
    prefWindow->setWindowModality(Qt::WindowModal);

    vpnGroupEditor *f = new vpnGroupEditor(prefWindow, vpnGroupEditorModeEdit);
    f->loadVpnGroup(vpnGroup);
    prefWindow->setCentralWidget(f);
    prefWindow->setMinimumSize(QSize(f->width(),f->height()));
    //prefWindow->setMaximumSize(QSize(f->width(),f->height()));
    prefWindow->setWindowTitle(windowTitle() + QObject::trUtf8(" - Edit VPN-Group"));

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

void MainWindow::on_tvVPNGroups_doubleClicked(const QModelIndex &index)
{
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

void MainWindow::onvpnAdded(const vpnProfile &vpn)
{
    refreshVpnProfileList();
}

void MainWindow::onvpnEdited(const vpnProfile &vpn)
{
    QStandardItem *item_name = getVpnProfileItem(vpn.name, 1);
    QStandardItem *item_gateway = getVpnProfileItem(vpn.name, 2);
    QStandardItem *item_user = getVpnProfileItem(vpn.name, 3);

    if(item_name != 0 && item_gateway != 0 && item_user != 0)
    {
        item_name->setText(vpn.name);
        item_gateway->setText(vpn.gateway_host);
        item_user->setText(vpn.username);
    }
}

void MainWindow::onvpnGroupAdded(const vpnGroup &vpngroup)
{
    refreshVpnGroupList();
}

void MainWindow::onvpnGroupEdited(const vpnGroup &vpngroup)
{
    refreshVpnGroupList();
}

void MainWindow::onvpnSearch(const QString &searchtext)
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
        return;
    }

    QStandardItem *item = model->itemFromIndex(sellist.at(0));
    QString itemName = item->text();

    if(ui->tabMain->currentIndex() == 1)
    {
        tiConfVpnGroups groups;
        vpnGroup *group = groups.getVpnGroupByName(itemName);
        QStringListIterator it(group->localMembers);
        while(it.hasNext())
        {
            onStartVPN(it.next(), vpnProfile::Origin_LOCAL);
        }
        QStringListIterator git(group->globalMembers);
        while(git.hasNext())
        {
            onStartVPN(git.next(), vpnProfile::Origin_GLOBAL);
        }
    }
    else
    {
        vpnProfile::Origin itemOrigin = static_cast<vpnProfile::Origin>(item->data().toInt());
        onStartVPN(itemName, itemOrigin);
    }
}

void MainWindow::onStartVPN(const QString &vpnname, vpnProfile::Origin origin)
{
    qDebug() << "start vpn:" << vpnname << "active-tab::" << ui->tabMain->currentIndex();

    vpnmanager->startVPN(vpnname);
}

void MainWindow::onActionStartVPN(const QString &vpnname)
{
    qDebug() << "action vpn pressed::" << vpnname;

    vpnClientConnection *conn = vpnmanager->getClientConnection(vpnname);
    if(conn != 0)
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
    QStringListIterator it(vpngroup->localMembers);
    vpnClientConnection::connectionStatus vpnGroupStatus = vpnClientConnection::STATUS_DISCONNECTED;
    int connCount = 0;
    while(it.hasNext())
    {
        conn = vpnmanager->getClientConnection(it.next());
        if(conn != 0)
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

    QStringListIterator it2(vpngroup->localMembers);
    while(it2.hasNext())
    {
        if(vpnGroupStatus == vpnClientConnection::STATUS_DISCONNECTED)
            vpnmanager->startVPN(it2.next());
        else
            vpnmanager->stopVPN(it2.next());
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
        QStringListIterator it(group->localMembers);
        while(it.hasNext())
        {
            vpnmanager->stopVPN(it.next());
        }
        QStringListIterator it2(group->globalMembers);
        while(it2.hasNext())
        {
            vpnmanager->stopVPN(it2.next());
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
    QCoreApplication::quit();
}

void MainWindow::onClientVPNStatusChanged(QString vpnname, vpnClientConnection::connectionStatus status)
{
    qDebug() << "MainWindow::onClientVPNStatusChanged::" << vpnname << "::status::" << status;

    vpnClientConnection *conn = vpnmanager->getClientConnection(vpnname);

    //refreshVpnProfileList();
    QIcon statusicon;
    QStandardItem *statusitem = getVpnProfileItem(vpnname, 0);
    if(statusitem != 0)
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

    if(isHidden())
    {
        switch(status)
        {
        case vpnClientConnection::STATUS_CONNECTED:
            tray->showMessage(tr("VPN-Status"), tr("VPN %1 is connected").arg(vpnname), QSystemTrayIcon::Information, 4000);
            break;
        case vpnClientConnection::STATUS_DISCONNECTED:
            tray->showMessage(tr("VPN-Status"), tr("VPN %1 is disconnected").arg(vpnname), QSystemTrayIcon::Information, 4000);
            break;
        }
    }
}

void MainWindow::onClientVPNCredRequest(QString vpnname)
{
    QMainWindow *prefWindow = new QMainWindow(this, Qt::Dialog);
    prefWindow->setWindowModality(Qt::WindowModal);

    vpnLogin *f = new vpnLogin(prefWindow);
    f->setData(vpnmanager, vpnname);
    prefWindow->setCentralWidget(f);
    prefWindow->setMinimumSize(QSize(f->width(),f->height()));
    //prefWindow->setMaximumSize(QSize(f->width(),f->height()));
    prefWindow->setWindowTitle(windowTitle() + QObject::trUtf8(" - Login"));
    f->initAfter();

    prefWindow->show();
}

void MainWindow::onClientVPNOTPRequest(QProcess *proc)
{
    QMainWindow *prefWindow = new QMainWindow(this, Qt::Dialog);
    prefWindow->setWindowModality(Qt::WindowModal);

    vpnOTPLogin *f = new vpnOTPLogin(prefWindow);
    f->setData(proc);
    prefWindow->setCentralWidget(f);
    prefWindow->setMinimumSize(QSize(f->width(),f->height()));
    //prefWindow->setMaximumSize(QSize(f->width(),f->height()));
    prefWindow->setWindowTitle(windowTitle() + QObject::trUtf8(" - OTP-Login"));
    f->initAfter();

    prefWindow->show();
}

void MainWindow::onClientVPNStatsUpdate(QString vpnname, vpnStats stats)
{
    vpnClientConnection *conn = vpnmanager->getClientConnection(vpnname);
    QStandardItem *item_stats = getVpnProfileItem(vpnname, 4);

    if(conn != 0 && item_stats != 0)
    {
        QString disp = QString("%1 / %2").arg(vpnHelper::formatByteUnits(stats.bytes_read)).arg(vpnHelper::formatByteUnits(stats.bytes_written));
        item_stats->setText(disp);
    }
}

void MainWindow::ontvVpnProfilesCustomContextMenu(const QPoint &point)
{
    QModelIndex index = ui->tvVpnProfiles->indexAt(point);
    QStandardItemModel *model = dynamic_cast<QStandardItemModel *>(ui->tvVpnProfiles->model());

    QStandardItem *item = model->itemFromIndex(index.sibling(index.row(), 1));
    if(item == 0)
        return;

    QString vpnname = item->text();
    if(vpnname.isEmpty())
        return;

    QMenu menu;
    QAction *a_connect = menu.addAction(QIcon(":/img/connected.png"), tr("Connect"));
    QAction *a_disconnect = menu.addAction(QIcon(":/img/disconnected.png"), tr("Disconnect"));
    menu.addSeparator();
    QAction *a_edit = menu.addAction(QIcon(":/img/edit.png"), tr("Edit"));
    QAction *a_copy = menu.addAction(QIcon(":/img/copy.png"), tr("Copy"));
    QAction *a_delete = menu.addAction(QIcon(":/img/delete.png"), tr("Delete"));
    menu.addSeparator();
    QAction *a_viewlogs = menu.addAction(QIcon(":/img/log.png"), tr("View logs"));
    QAction *choosen = menu.exec(ui->tvVpnProfiles->mapToGlobal(point));

    if(choosen == a_viewlogs)
    {
        QDesktopServices::openUrl(QUrl::fromLocalFile(QString("%1/logs/vpn/%2.log").arg(tiConfMain::getAppDir(), vpnname)));
        return;
    }

    if(choosen == a_connect)
    {
        onStartVPN(vpnname);
        return;
    }

    if(choosen == a_disconnect)
    {
        onStopVPN(vpnname);
        return;
    }

    if(choosen == a_edit)
    {
        on_btnEditVPN_clicked();
        return;
    }

    if(choosen == a_copy)
    {
        on_btnCopyVPN_clicked();
        return;
    }

    if(choosen == a_delete)
    {
        on_btnDeleteVPN_clicked();
        return;
    }
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

    QStandardItem *item = 0;
    QStandardItem *item2 = 0;
    QStandardItem *item3 = 0;
    QStandardItem *item4 = 0;
    QStandardItem *item5 = 0;
    int localRow = 0, globalRow = 0;

    if(tray_menu == 0)
        tray_menu = new QMenu();
    tray_menu->clear();
    if(MainWindow::taskbarPosition() == MainWindow::TASKBAR_POSITION_TOP)
    {
        tray_menu->addAction(QIcon(":/img/quit.png"),trUtf8("Quit app"), this, SLOT(onQuit()));
        tray_menu->addAction(QIcon(":/img/settings.png"), trUtf8("Settings"), this, SLOT(onVPNSettings()));
        tray_menu->addAction(QIcon(":/img/show.png"), trUtf8("Show mainwindow"), this, SLOT(show()));
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
        if(conn != 0)
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

        switch(vpn->origin_location)
        {
            case vpnProfile::Origin_LOCAL:
            {
                localRow = root_local_vpn->rowCount();
                root_local_vpn->setChild(localRow, 0, item4);
                root_local_vpn->setChild(localRow, 1, item);
                root_local_vpn->setChild(localRow, 2, item2);
                root_local_vpn->setChild(localRow, 3, item3);
                root_local_vpn->setChild(localRow, 4, item5);
                item->setData(vpnProfile::Origin_LOCAL);
                break;
            }
            case vpnProfile::Origin_GLOBAL:
            {
                globalRow = root_global_vpn->rowCount();
                root_global_vpn->setChild(globalRow, 0, item4);
                root_global_vpn->setChild(globalRow, 1, item);
                root_global_vpn->setChild(globalRow, 2, item2);
                root_global_vpn->setChild(globalRow, 3, item3);
                root_global_vpn->setChild(globalRow, 4, item5);
                item->setData(vpnProfile::Origin_GLOBAL);
                break;
            }
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

    QMapIterator<QString, QAction*> itItems(trayItems);
    while(itItems.hasNext())
    {
        itItems.next();
        tray_menu->insertAction(0, itItems.value());
    }

    if(MainWindow::taskbarPosition() == MainWindow::TASKBAR_POSITION_BOTTOM)
    {
        tray_menu->addSeparator();
        tray_menu->addMenu(tray_group_menu);
        tray_menu->addSeparator();
        tray_menu->addAction(QIcon(":/img/show.png"), trUtf8("Show mainwindow"), this, SLOT(show()));
        tray_menu->addAction(QIcon(":/img/settings.png"), trUtf8("Settings"), this, SLOT(onVPNSettings()));
        tray_menu->addAction(QIcon(":/img/quit.png"), trUtf8("Quit app"), this, SLOT(onQuit()));
    }

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

    QStandardItem *item = 0;
    QStandardItem *item2 = 0;
    QStandardItem *item3 = 0;
    int row = model->rowCount();

    tray_group_menu->clear();

    vpnClientConnection *conn;
    QMap<QString, QAction*> trayItems;
    QList<vpnGroup*> vpngroups = vpngroupss.getVpnGroups();
    for(int i=0; i < vpngroups.count(); i++)
    {
        vpnGroup *vpngroup = vpngroups.at(i);
        qDebug() << "MainWindow::refreshVpnGroupList() -> vpngroups found::" << vpngroup->name;

        QIcon status;
        QStringListIterator it(vpngroup->localMembers);
        QStringListIterator it2(vpngroup->globalMembers);
        vpnClientConnection::connectionStatus vpnGroupStatus = vpnClientConnection::STATUS_DISCONNECTED;
        int connCount = 0;
        while(it.hasNext())
        {
            conn = vpnmanager->getClientConnection(it.next());
            if(conn != 0)
            {
                if(conn->status == vpnClientConnection::STATUS_CONNECTED)
                    connCount+=1;
            }
        }
        while(it2.hasNext())
        {
            conn = vpnmanager->getClientConnection(it2.next());
            if(conn != 0)
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
        QAction *action = new QAction(status, vpngroup->name, tray_group_menu);
        connect(action, SIGNAL(triggered(bool)), signalMapperGroups, SLOT(map()));
        signalMapperGroups->setMapping(action, vpngroup->name);
        trayItems[vpngroup->name] = action;
    }

    QMapIterator<QString, QAction*> itItems(trayItems);
    while(itItems.hasNext())
    {
        itItems.next();
        tray_group_menu->insertAction(0, itItems.value());
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

QStandardItem *MainWindow::getVpnProfileItem(const QString &vpnname, int colum)
{
    QStandardItem *retitem = 0;

    for(int i=0; i < root_local_vpn->rowCount(); i++)
    {
        if(root_local_vpn->child(i, 1)->text() == vpnname)
        {
            return root_local_vpn->child(i, colum);
        }
    }

    if(retitem == 0)
    {
        for(int i=0; i < root_global_vpn->rowCount(); i++)
        {
            if(root_global_vpn->child(i, 1)->text() == vpnname)
            {
                return root_global_vpn->child(i, colum);
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
    QMessageBox::about(this, trUtf8("About openFortiGUI"), trUtf8("<b>openFortiGUI %1</b><br>"
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
            show();
            raise();
            QApplication::setActiveWindow(this);
        }
        else
            hide();
    }
}

void MainWindow::onVPNSettings()
{
    QMainWindow *prefWindow = new QMainWindow(this, Qt::Dialog);
    prefWindow->setWindowModality(Qt::WindowModal);

    vpnSetting *f = new vpnSetting(prefWindow);
    prefWindow->setCentralWidget(f);
    prefWindow->setMinimumSize(QSize(f->width(),f->height()));
    //prefWindow->setMaximumSize(QSize(f->width(),f->height()));
    prefWindow->setWindowTitle(windowTitle() + QObject::trUtf8(" - Settings"));

    prefWindow->show();
}

void MainWindow::onSetupWizard()
{
    QMainWindow *prefWindow = new QMainWindow(this, Qt::Dialog);
    prefWindow->setWindowModality(Qt::WindowModal);

    setupWizard *f = new setupWizard(prefWindow);
    prefWindow->setCentralWidget(f);
    prefWindow->setMinimumSize(QSize(f->width(),f->height()));
    //prefWindow->setMaximumSize(QSize(f->width(),f->height()));
    prefWindow->setWindowTitle(windowTitle() + QObject::trUtf8(" - Setup wizard"));

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
    QMainWindow *changeWindow = new QMainWindow(this, Qt::Dialog);
    changeWindow->setWindowModality(Qt::WindowModal);

    vpnChangelog *f = new vpnChangelog(changeWindow);
    changeWindow->setCentralWidget(f);
    changeWindow->setMinimumSize(QSize(f->width(),f->height()));
    //prefWindow->setMaximumSize(QSize(f->width(),f->height()));
    changeWindow->setWindowTitle(windowTitle() + QObject::trUtf8(" - Changelog"));
    f->initAfter();

    changeWindow->show();
    changeWindow->raise();
    QApplication::setActiveWindow(changeWindow);
}

void MainWindow::onWatcherVpnProfilesChanged(const QString &path)
{
    refreshVpnProfileList();
    refreshVpnGroupList();
}
