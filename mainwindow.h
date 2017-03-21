#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include <QMainWindow>
#include <QSystemTrayIcon>
#include <QSignalMapper>

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
    QSignalMapper* signalMapper;

public:
    explicit MainWindow(QWidget *parent = 0);
    ~MainWindow();

private slots:
    void on_btnAddVPN_clicked();
    void on_btnDeleteVPN_clicked();
    void on_btnEditVPN_clicked();
    void on_btnCopyVPN_clicked();
    void on_tvVpnProfiles_doubleClicked(const QModelIndex &index);

    void on_btnAddGroup_clicked();

    void onvpnAdded(const vpnProfile &vpn);
    void onvpnEdited(const vpnProfile &vpn);

    void onStartVPN();
    void onStartVPN(const QString &vpnname);
    void onActionStartVPN(const QString &vpnname);
    void onStopVPN();
    void onStopVPN(const QString &vpnname);
    void onQuit();
    void onActionAbout();

    void onClientVPNStatusChanged(QString vpnname, vpnClientConnection::connectionStatus status);

private:
    Ui::MainWindow *ui;

    void refreshVpnProfileList();

protected:
    bool eventFilter(QObject *object, QEvent *event);
};

#endif // MAINWINDOW_H
