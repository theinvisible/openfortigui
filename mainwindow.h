#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include <QMainWindow>
#include <QSystemTrayIcon>

#include "vpnmanager.h"
#include "vpnprofile.h"

namespace Ui {
class MainWindow;
}

class MainWindow : public QMainWindow
{
    Q_OBJECT

    QSystemTrayIcon *tray;
    static vpnManager *vpnmanager;

public:
    explicit MainWindow(QWidget *parent = 0);
    ~MainWindow();

private slots:
    void on_btnAddVPN_clicked();
    void onvpnAdded(const vpnProfile &vpn);

private:
    Ui::MainWindow *ui;

    void refreshVpnProfileList();

protected:
    bool eventFilter(QObject *object, QEvent *event);
};

#endif // MAINWINDOW_H
