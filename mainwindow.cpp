#include "mainwindow.h"
#include "ui_mainwindow.h"

#include <QThread>
#include <QProcess>
#include <QTextStream>
#include <QDesktopWidget>
#include <QStandardItemModel>
#include <QMessageBox>

#include "config.h"
#include "ticonfmain.h"
#include "vpnprofileeditor.h"

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

    // Center window on startup
    QRect geom = QApplication::desktop()->availableGeometry();
    move((geom.width() - width()) / 2, (geom.height() - height()) / 2);

    QStringList headers;
    headers << trUtf8("Status") << trUtf8("Name") << trUtf8("Gateway") << trUtf8("Benutzer");

    QStandardItemModel *model = new QStandardItemModel(ui->tvVpnProfiles);
    model->setHorizontalHeaderLabels(headers);

    ui->tvVpnProfiles->setModel(model);

    tray = new QSystemTrayIcon(this);
    tray->setIcon(QIcon(":/img/app.png"));
    tray->show();

    ui->tbActions->addAction(QIcon(":/img/connected.png"), "Connect", this, SLOT(onStartVPN()));
    ui->tbActions->addAction(QIcon(":/img/disconnected.png"), "Disconnect", this, SLOT(onStopVPN()));

    refreshVpnProfileList();
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
    //ui->tvBackupFolders->selectedItems();

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

    QMainWindow *prefWindow = new QMainWindow(this, Qt::Dialog);
    prefWindow->setWindowModality(Qt::WindowModal);

    vpnProfileEditor *f = new vpnProfileEditor(prefWindow, vpnProfileEditorModeEdit);
    f->loadVpnProfile(vpnName);
    prefWindow->setCentralWidget(f);
    prefWindow->setMinimumSize(QSize(f->width(),f->height()));
    prefWindow->setMaximumSize(QSize(f->width(),f->height()));
    prefWindow->setWindowTitle(windowTitle() + QObject::trUtf8(" - Add VPN"));

    connect(f, SIGNAL(vpnEdited(vpnProfile)), this, SLOT(onvpnEdited(vpnProfile)));
    prefWindow->show();
}

void MainWindow::on_tvVpnProfiles_doubleClicked(const QModelIndex &index)
{
    on_btnEditVPN_clicked();
}


void MainWindow::onvpnAdded(const vpnProfile &vpn)
{
    refreshVpnProfileList();
}

void MainWindow::onvpnEdited(const vpnProfile &vpn)
{
    refreshVpnProfileList();
}

void MainWindow::onStartVPN()
{
    qInfo() << "start vpn::";

    QStandardItemModel *model = dynamic_cast<QStandardItemModel *>(ui->tvVpnProfiles->model());
    QItemSelectionModel *selmodel = ui->tvVpnProfiles->selectionModel();
    QModelIndexList sellist = selmodel->selectedRows(1);

    if(sellist.count() < 1)
    {
        return;
    }

    QString vpnName = model->itemFromIndex(sellist.at(0))->text();

    vpnmanager->startVPN(vpnName);
}

void MainWindow::onStartVPN(const QString &vpnname)
{
    qInfo() << "start vpn::" << vpnname;

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

void MainWindow::onStopVPN()
{
    qInfo() << "stop vpn::";

    QStandardItemModel *model = dynamic_cast<QStandardItemModel *>(ui->tvVpnProfiles->model());
    QItemSelectionModel *selmodel = ui->tvVpnProfiles->selectionModel();
    QModelIndexList sellist = selmodel->selectedRows(1);

    if(sellist.count() < 1)
    {
        return;
    }

    QString vpnName = model->itemFromIndex(sellist.at(0))->text();

    vpnmanager->stopVPN(vpnName);
}

void MainWindow::onStopVPN(const QString &vpnname)
{
    vpnmanager->stopVPN(vpnname);
}

void MainWindow::onClientVPNStatusChanged(QString vpnname, vpnClientConnection::connectionStatus status)
{
    refreshVpnProfileList();
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

    QMenu *menu = tray->contextMenu();
    if(menu == 0)
        menu = new QMenu();
    menu->clear();
    menu->addAction("Beenden");
    menu->addAction("Hauptfenster öffnen");
    menu->addSeparator();

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
        QAction *action = menu->addAction(status, vpn->name, signalMapper, SLOT(map()));
        signalMapper->setMapping(action, vpn->name) ;
    }

    ui->tvVpnProfiles->header()->resizeSection(0, 50);
    ui->tvVpnProfiles->header()->resizeSection(1, 150);
    ui->tvVpnProfiles->header()->resizeSection(2, 300);
    ui->tvVpnProfiles->sortByColumn(1);

    tray->setContextMenu(menu);
}

bool MainWindow::eventFilter(QObject *object, QEvent *event)
{
    if(object == this && event->type() == QEvent::Close)
    {
        /*
        int ret = QMessageBox::warning(this, QString::fromUtf8("Fenster schließen"),
                                    QString::fromUtf8("Alle Änderungen gehen verloren. Fortfahren?"),
                                    QMessageBox::Yes | QMessageBox::No);

        switch(ret)
        {
        case QMessageBox::Yes:
            return false;
        case QMessageBox::No:
        default:
            event->ignore();
            return true;
        }
        */

        return false;
    }

    return false;
}
