#include "mainwindow.h"
#include "ui_mainwindow.h"

#include <QThread>
#include <QProcess>
#include <QTextStream>
#include <QDesktopWidget>
#include <QStandardItemModel>
#include <QMessageBox>
#include <QInputDialog>

#include "config.h"
#include "ticonfmain.h"
#include "vpnprofileeditor.h"
#include "vpngroupeditor.h"

vpnManager *MainWindow::vpnmanager = 0;

MainWindow::MainWindow(QWidget *parent) :
    QMainWindow(parent),
    ui(new Ui::MainWindow)
{
    ui->setupUi(this);

    installEventFilter(this);

    vpnmanager = new vpnManager(this);
    connect(vpnmanager, SIGNAL(VPNStatusChanged(QString,vpnClientConnection::connectionStatus)), this, SLOT(onClientVPNStatusChanged(QString,vpnClientConnection::connectionStatus)));

    signalMapper = new QSignalMapper(this);
    connect(signalMapper, SIGNAL(mapped(QString)), this, SLOT(onActionStartVPN(QString)));
    signalMapperGroups = new QSignalMapper(this);
    connect(signalMapperGroups, SIGNAL(mapped(QString)), this, SLOT(onActionStartVPNGroup(QString)));

    // Center window on startup
    QRect geom = QApplication::desktop()->availableGeometry();
    move((geom.width() - width()) / 2, (geom.height() - height()) / 2);

    // Treeview VPNs
    QStringList headers;
    headers << trUtf8("Status") << trUtf8("Name") << trUtf8("Gateway") << trUtf8("Benutzer");
    QStandardItemModel *model = new QStandardItemModel(ui->tvVpnProfiles);
    model->setHorizontalHeaderLabels(headers);
    ui->tvVpnProfiles->setModel(model);

    // Treeview VPN-Groups
    QStringList headers2;
    headers2 << trUtf8("Status") << trUtf8("Name") << trUtf8("VPNs");
    QStandardItemModel *model2 = new QStandardItemModel(ui->tvVPNGroups);
    model2->setHorizontalHeaderLabels(headers2);
    ui->tvVPNGroups->setModel(model2);

    tray = new QSystemTrayIcon(this);
    tray->setIcon(QIcon(":/img/app.png"));
    tray->show();
    tray_menu = tray->contextMenu();
    tray_group_menu = new QMenu("VPN-Groups");

    ui->tbActions->addAction(QIcon(":/img/connected.png"), "Connect", this, SLOT(onStartVPN()));
    ui->tbActions->addAction(QIcon(":/img/disconnected.png"), "Disconnect", this, SLOT(onStopVPN()));

    connect(ui->actionMenuExit, SIGNAL(triggered(bool)), this, SLOT(onQuit()));
    connect(ui->actionMenuHide, SIGNAL(triggered(bool)), this, SLOT(hide()));
    connect(ui->actionMenuConnect, SIGNAL(triggered(bool)), this, SLOT(onStartVPN()));
    connect(ui->actionMenuDisconnect, SIGNAL(triggered(bool)), this, SLOT(onStopVPN()));
    connect(ui->actionMenuAbout, SIGNAL(triggered(bool)), this, SLOT(onActionAbout()));

    refreshVpnProfileList();
    refreshVpnGroupList();
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
    prefWindow->setMaximumSize(QSize(f->width(),f->height()));
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
    {
        return;
    }

    QString vpnName = model->itemFromIndex(sellist.at(0))->text();
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
    vpnClientConnection *cl = vpnmanager->getClientConnection(vpnName);
    if(cl != 0 && cl->status != vpnClientConnection::STATUS_DISCONNECTED)
    {
        QMessageBox::warning(this, trUtf8("Delete VPN"),
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
    prefWindow->setMaximumSize(QSize(f->width(),f->height()));
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
    prefWindow->setMaximumSize(QSize(f->width(),f->height()));
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
        QMessageBox::information(this, trUtf8("Delete VPN-group"), trUtf8("The selected vpn-group could not be deleted, an error occured."));
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
    prefWindow->setMaximumSize(QSize(f->width(),f->height()));
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

void MainWindow::onvpnAdded(const vpnProfile &vpn)
{
    refreshVpnProfileList();
}

void MainWindow::onvpnEdited(const vpnProfile &vpn)
{
    refreshVpnProfileList();
}

void MainWindow::onvpnGroupAdded(const vpnGroup &vpngroup)
{
    refreshVpnGroupList();
}

void MainWindow::onvpnGroupEdited(const vpnGroup &vpngroup)
{
    refreshVpnGroupList();
}

void MainWindow::onStartVPN()
{
    qInfo() << "active-tab::" << ui->tabMain->currentIndex();

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
        QStringListIterator it(group->members);
        while(it.hasNext())
        {
            vpnmanager->startVPN(it.next());
        }
    }
    else
        vpnmanager->startVPN(itemName);
}

void MainWindow::onStartVPN(const QString &vpnname)
{
    qInfo() << "start vpn:" << vpnname << "active-tab::" << ui->tabMain->currentIndex();

    vpnmanager->startVPN(vpnname);
}

void MainWindow::onActionStartVPN(const QString &vpnname)
{
    qInfo() << "action vpn pressed::" << vpnname;

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
    qInfo() << "action vpn-group pressed::" << vpnname;

    tiConfVpnGroups groups;
    vpnClientConnection *conn;
    vpnGroup *vpngroup = groups.getVpnGroupByName(vpnname);
    QStringListIterator it(vpngroup->members);
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

    if(connCount == vpngroup->members.count())
        vpnGroupStatus = vpnClientConnection::STATUS_CONNECTED;
    else if(connCount < vpngroup->members.count() && connCount > 0)
        vpnGroupStatus = vpnClientConnection::STATUS_CONNECTING;
    else
        vpnGroupStatus = vpnClientConnection::STATUS_DISCONNECTED;

    QStringListIterator it2(vpngroup->members);
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
    qInfo() << "stop vpn::" << ui->tabMain->currentIndex();

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
        QStringListIterator it(group->members);
        while(it.hasNext())
        {
            vpnmanager->stopVPN(it.next());
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
    refreshVpnProfileList();
    refreshVpnGroupList();
}

void MainWindow::refreshVpnProfileList()
{
    QStandardItemModel *model = dynamic_cast<QStandardItemModel *>(ui->tvVpnProfiles->model());
    tiConfVpnProfiles vpnss;
    vpnss.readVpnProfiles();

    model->removeRows(0, model->rowCount());

    QStandardItem *item = 0;
    QStandardItem *item2 = 0;
    QStandardItem *item3 = 0;
    QStandardItem *item4 = 0;
    int row = model->rowCount();

    if(tray_menu == 0)
        tray_menu = new QMenu();
    tray_menu->clear();
    tray_menu->addAction(trUtf8("Quit app"), this, SLOT(onQuit()));
    tray_menu->addAction(trUtf8("Show mainwindow"), this, SLOT(show()));
    tray_menu->addSeparator();
    tray_menu->addMenu(tray_group_menu);
    tray_menu->addSeparator();

    QList<vpnProfile*> vpns = vpnss.getVpnProfiles();
    for(int i=0; i < vpns.count(); i++)
    {
        vpnProfile *vpn = vpns.at(i);
        qDebug() << "MainWindow::refreshVpnProfileList() -> vpnprofiles found::" << vpn->name;

        QIcon status;
        vpnClientConnection *conn = vpnmanager->getClientConnection(vpn->name);
        if(conn != 0)
        {
            switch(conn->status)
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
        }
        else
            status = QIcon(":/img/disconnected.png");

        item = new QStandardItem(vpn->name);
        item2 = new QStandardItem(vpn->gateway_host);
        item3 = new QStandardItem(vpn->username);
        item4 = new QStandardItem(status, "");

        row = model->rowCount();
        model->setItem(row, 0, item4);
        model->setItem(row, 1, item);
        model->setItem(row, 2, item2);
        model->setItem(row, 3, item3);

        // Menu
        QAction *action = tray_menu->addAction(status, vpn->name, signalMapper, SLOT(map()));
        signalMapper->setMapping(action, vpn->name) ;
    }

    ui->tvVpnProfiles->header()->resizeSection(0, 50);
    ui->tvVpnProfiles->header()->resizeSection(1, 150);
    ui->tvVpnProfiles->header()->resizeSection(2, 300);
    ui->tvVpnProfiles->sortByColumn(1);

    tray->setContextMenu(tray_menu);
}

void MainWindow::refreshVpnGroupList()
{
    QStandardItemModel *model = dynamic_cast<QStandardItemModel *>(ui->tvVPNGroups->model());
    tiConfVpnGroups vpngroupss;
    vpngroupss.readVpnGroups();

    model->removeRows(0, model->rowCount());

    QStandardItem *item = 0;
    QStandardItem *item2 = 0;
    QStandardItem *item3 = 0;
    int row = model->rowCount();

    tray_group_menu->clear();

    vpnClientConnection *conn;
    QList<vpnGroup*> vpngroups = vpngroupss.getVpnGroups();
    for(int i=0; i < vpngroups.count(); i++)
    {
        vpnGroup *vpngroup = vpngroups.at(i);
        qDebug() << "MainWindow::refreshVpnGroupList() -> vpngroups found::" << vpngroup->name;

        QIcon status;
        QStringListIterator it(vpngroup->members);
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

        if(connCount == vpngroup->members.count())
            vpnGroupStatus = vpnClientConnection::STATUS_CONNECTED;
        else if(connCount < vpngroup->members.count() && connCount > 0)
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
        item2 = new QStandardItem(vpngroup->members.join(", "));
        item3 = new QStandardItem(status, "");

        row = model->rowCount();
        model->setItem(row, 0, item3);
        model->setItem(row, 1, item);
        model->setItem(row, 2, item2);

        // Menu
        QAction *action = tray_group_menu->addAction(status, vpngroup->name, signalMapperGroups, SLOT(map()));
        signalMapperGroups->setMapping(action, vpngroup->name) ;
    }

    ui->tvVPNGroups->header()->resizeSection(0, 50);
    ui->tvVPNGroups->header()->resizeSection(1, 150);
    ui->tvVPNGroups->sortByColumn(1);
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
    QMessageBox::about(this, trUtf8("About openFortiGUI"), trUtf8("<b>openFortiGUI %1</b> <br><br>"
                                                             "Developer: <b>Rene Hadler</b> <br>"
                                                             "eMail: <a href=mailto:'rene@hadler.me'>rene@hadler.me</a> <br>"
                                                             "Website: <a href=https://hadler.me>https://hadler.me</a></p>"
                                                             "<p>This program uses following libs/resources:</p>"
                                                              "QT: <a href='https://www.qt.io'>https://www.qt.io</a> <br>"
                                                              "openfortivpn: <a href='https://github.com/adrienverge/openfortivpn'>https://github.com/adrienverge/openfortivpn</a> <br>"
                                                              "QTinyAes: <a href='https://github.com/Skycoder42/QTinyAes'>https://github.com/Skycoder42/QTinyAes</a> <br>"
                                                              "tiny-AES128-C: <a href='https://github.com/kokke/tiny-AES128-C'>https://github.com/kokke/tiny-AES128-C</a> <br>"
                                                              "Icons8: <a href='https://icons8.com/'>https://icons8.com</a>").arg(openfortigui_config::version));
}
