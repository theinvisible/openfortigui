#include "mainwindow.h"
#include "ui_mainwindow.h"

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
}

MainWindow::~MainWindow()
{
    delete ui;
}
