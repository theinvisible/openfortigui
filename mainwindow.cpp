#include "mainwindow.h"
#include "ui_mainwindow.h"

#include <QThread>
#include <QProcess>
#include <QTextStream>
#include <QDesktopWidget>
#include <QStandardItemModel>

extern "C"  {
#include "openfortivpn/src/config.h"
#include "openfortivpn/src/log.h"
#include "openfortivpn/src/tunnel.h"
}

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

    // Center window on startup
    QRect geom = QApplication::desktop()->availableGeometry();
    move((geom.width() - width()) / 2, (geom.height() - height()) / 2);

    QStringList headers;
    headers << trUtf8("Name") << trUtf8("Gateway") << trUtf8("Benutzer");

    QStandardItemModel *model = new QStandardItemModel(ui->tvVpnProfiles);
    model->setHorizontalHeaderLabels(headers);

    ui->tvVpnProfiles->setModel(model);

    QMenu *menu = new QMenu();
    menu->addAction("Beenden");
    menu->addAction("Hauptfenster öffnen");
    menu->addSeparator();
    menu->addAction(QIcon(":/img/disconnected.png"), "vpn1");
    menu->addAction(QIcon(":/img/connected.png"), "vpn2");
    menu->addAction(QIcon(":/img/disconnected.png"), "vpn3");
    menu->addAction(QIcon(":/img/disconnected.png"), "vpn4");
    menu->addAction(QIcon(":/img/connected.png"), "vpn5");

    tray = new QSystemTrayIcon(this);
    tray->setIcon(QIcon(":/img/app.png"));
    tray->show();
    tray->setContextMenu(menu);

    refreshVpnProfileList();

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


void MainWindow::onvpnAdded(const vpnProfile &vpn)
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
    int row = model->rowCount();

    QList<vpnProfile*> vpns = vpnss.getVpnProfiles();
    for(int i=0; i < vpns.count(); i++)
    {
        vpnProfile *vpn = vpns.at(i);
        qDebug() << "MainWindow::refreshVpnProfileList() -> vpnprofiles found::" << vpn->name;

        item = new QStandardItem(vpn->name);
        item2 = new QStandardItem(vpn->gateway_host);
        item3 = new QStandardItem(vpn->username);

        row = model->rowCount();
        model->setItem(row, 0, item);
        model->setItem(row, 1, item2);
        model->setItem(row, 2, item3);
    }

    ui->tvVpnProfiles->header()->resizeSection(0, 150);
    ui->tvVpnProfiles->header()->resizeSection(1, 300);
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
