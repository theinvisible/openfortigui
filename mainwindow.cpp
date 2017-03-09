#include "mainwindow.h"
#include "ui_mainwindow.h"

#include "vpnworker.h"
#include "vpnworker2.h"

#include "QThread"

extern "C"  {
#include "openfortivpn/src/config.h"
#include "openfortivpn/src/log.h"
#include "openfortivpn/src/tunnel.h"
}

MainWindow::MainWindow(QWidget *parent) :
    QMainWindow(parent),
    ui(new Ui::MainWindow)
{
    ui->setupUi(this);

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

    increase_verbosity();
    increase_verbosity();
    increase_verbosity();
    increase_verbosity();

    /*
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
}

MainWindow::~MainWindow()
{
    delete ui;
}
