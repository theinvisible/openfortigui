#include "mainwindow.h"
#include "ui_mainwindow.h"

#include <QThread>
#include <QProcess>
#include <QTextStream>
#include <QDesktopWidget>
#include <QStandardItemModel>
#include <QMessageBox>

extern "C"  {
#include "openfortivpn/src/config.h"
#include "openfortivpn/src/log.h"
#include "openfortivpn/src/tunnel.h"
}

#include "config.h"
#include "ticonfmain.h"
#include "vpnprofileeditor.h"
#include "qtinyaes/QTinyAes/qtinyaes.h"

vpnManager *MainWindow::vpnmanager = 0;

MainWindow::MainWindow(QWidget *parent) :
    QMainWindow(parent),
    ui(new Ui::MainWindow)
{
    ui->setupUi(this);

    installEventFilter(this);

    vpnmanager = new vpnManager(this);

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

    ui->tbActions->addAction(QIcon(":/img/connected.png"), "Connect");
    ui->tbActions->addAction(QIcon(":/img/disconnected.png"), "Disconnect");

    refreshVpnProfileList();

    QTinyAes aes(QTinyAes::CBC, openfortigui_config::aeskey, "random_iv_128bit");

    QString n = "Rene";
    qInfo() << "orig::" << n;
    QByteArray cipher = aes.encrypt(n.toUtf8());
    QString cstring = QString::fromUtf8(cipher.toBase64());
    qInfo() << "ciper::" << cstring;
    QString res = QString::fromUtf8(aes.decrypt(QByteArray::fromBase64(cstring.toUtf8())));
    qInfo() << "result::" << res;

    //vpnmanager = new vpnManager(this);
    //vpnmanager->startvpn1();

    /*
    QTextStream out(stdout);
    QStringList arguments;
    arguments << "./openfortigui";
    arguments << "--start-vpn";
    arguments << "--vpn-name";
    arguments << "vpn1";

    QStringList arguments2;
    arguments2 << "./openfortigui";
    arguments2 << "--start-vpn";
    arguments2 << "--vpn-name";
    arguments2 << "vpn2";

    QProcess *vpn1 = new QProcess(this);
    out << "Start vpn";
    vpn1->start("sudo", arguments);
    vpn1->waitForStarted();
    vpn1->waitForReadyRead();
    out << "Start read";
    out << vpn1->readAll();

    QProcess *vpn2 = new QProcess(this);
    out << "Start vpn2";
    vpn2->start("sudo", arguments2);
    vpn2->waitForStarted();
    vpn2->waitForReadyRead();
    out << "Start read";
    out << vpn2->readAll();
    */


    //QProcess *vpn2 = new QProcess(this);
    //vpn2->start("openfortigui", arguments);

    /*
    increase_verbosity();
    increase_verbosity();
    increase_verbosity();

    struct vpn_config cfg;
    memset(&cfg, 0, sizeof (cfg));
    strncpy(cfg.gateway_host, "***REMOVED***", FIELD_SIZE);
    cfg.gateway_host[FIELD_SIZE] = '\0';
    cfg.gateway_port = 10443;
    strncpy(cfg.username, "***REMOVED***", FIELD_SIZE);
    cfg.username[FIELD_SIZE] = '\0';
    strncpy(cfg.password, "***REMOVED***", FIELD_SIZE);
    cfg.password[FIELD_SIZE] = '\0';
    cfg.set_routes = 1;
    run_tunnel(&cfg);
    */

    /*
    QThread* thread = new QThread;
    vpnWorker* worker = new vpnWorker();
    worker->moveToThread(thread);
    //connect(worker, SIGNAL(error(QString)), this, SLOT(errorString(QString)));
    connect(thread, SIGNAL(started()), worker, SLOT(process()));
    //connect(worker, SIGNAL(finished()), thread, SLOT(quit()));
    //connect(worker, SIGNAL(finished()), worker, SLOT(deleteLater()));
    connect(thread, SIGNAL(finished()), thread, SLOT(deleteLater()));
    //connect(worker, SIGNAL(diskRemoved(DeviceDisk*)), this, SLOT(onDiskRemoved(DeviceDisk*)));
    //connect(worker, SIGNAL(diskAdded(DeviceDisk*)), this, SLOT(onDiskAdded(DeviceDisk*)));
    thread->start();

    QThread* thread2 = new QThread;
    vpnWorker2* worker2 = new vpnWorker2();
    worker2->moveToThread(thread2);
    //connect(worker, SIGNAL(error(QString)), this, SLOT(errorString(QString)));
    connect(thread2, SIGNAL(started()), worker2, SLOT(process()));
    //connect(worker, SIGNAL(finished()), thread, SLOT(quit()));
    //connect(worker, SIGNAL(finished()), worker, SLOT(deleteLater()));
    connect(thread2, SIGNAL(finished()), thread2, SLOT(deleteLater()));
    //connect(worker, SIGNAL(diskRemoved(DeviceDisk*)), this, SLOT(onDiskRemoved(DeviceDisk*)));
    //connect(worker, SIGNAL(diskAdded(DeviceDisk*)), this, SLOT(onDiskAdded(DeviceDisk*)));
    thread2->start();
    */
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

        item = new QStandardItem(vpn->name);
        item2 = new QStandardItem(vpn->gateway_host);
        item3 = new QStandardItem(vpn->username);
        item4 = new QStandardItem(QIcon(":/img/disconnected.png"), "");

        row = model->rowCount();
        model->setItem(row, 0, item4);
        model->setItem(row, 1, item);
        model->setItem(row, 2, item2);
        model->setItem(row, 3, item3);

        // Menu
        menu->addAction(QIcon(":/img/disconnected.png"), vpn->name);
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
