#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include <QMainWindow>
#include <QSystemTrayIcon>
#include <QSignalMapper>

#include "vpnmanager.h"
#include "vpnprofile.h"
#include "vpngroup.h"

namespace Ui {
class MainWindow;
}

class MainWindow : public QMainWindow
{
    Q_OBJECT

    QSystemTrayIcon *tray;
    static vpnManager *vpnmanager;
    QSignalMapper *signalMapper, *signalMapperGroups;

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
    void on_btnDeleteGroup_clicked();
    void on_btnEditGroup_clicked();
    void on_btnCopyGroup_clicked();
    void on_tvVPNGroups_doubleClicked(const QModelIndex &index);

    void onvpnAdded(const vpnProfile &vpn);
    void onvpnEdited(const vpnProfile &vpn);
    void onvpnGroupAdded(const vpnGroup &vpngroup);
    void onvpnGroupEdited(const vpnGroup &vpngroup);

    void onStartVPN();
    void onStartVPN(const QString &vpnname);
    void onActionStartVPN(const QString &vpnname);
    void onActionStartVPNGroup(const QString &vpnname);
    void onStopVPN();
    void onStopVPN(const QString &vpnname);
    void onQuit();
    void onActionAbout();

    void onClientVPNStatusChanged(QString vpnname, vpnClientConnection::connectionStatus status);

private:
    Ui::MainWindow *ui;

    QMenu *tray_menu;
    QMenu *tray_group_menu;

    void refreshVpnProfileList();
    void refreshVpnGroupList();

protected:
    bool eventFilter(QObject *object, QEvent *event);
};

#endif // MAINWINDOW_H
