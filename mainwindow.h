#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include <QMainWindow>
#include <QSystemTrayIcon>
#include <QSignalMapper>
#include <QStandardItem>
#include <QFileSystemWatcher>

#include "vpnmanager.h"
#include "vpnprofile.h"
#include "vpngroup.h"

namespace Ui {
class MainWindow;
}

class MainWindow : public QMainWindow
{
    Q_OBJECT

public:
    explicit MainWindow(QWidget *parent = 0);
    ~MainWindow();

    QSystemTrayIcon *tray;
    static vpnManager *vpnmanager;
    QSignalMapper *signalMapper, *signalMapperGroups;

    enum TASKBAR_POSITION {
        TASKBAR_POSITION_TOP,
        TASKBAR_POSITION_BOTTOM
    };

    static TASKBAR_POSITION taskbarPosition();

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

    void onTbActionEdit();
    void onTbActionCopy();
    void onTbActionDelete();

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
    void onTrayIconActivated(QSystemTrayIcon::ActivationReason reason);
    void onVPNSettings();

    void onWatcherVpnProfilesChanged(const QString &path);

    void onClientVPNStatusChanged(QString vpnname, vpnClientConnection::connectionStatus status);

private:
    Ui::MainWindow *ui;

    QMenu *tray_menu;
    QMenu *tray_group_menu;
    QStandardItem *root_local_vpn, *root_global_vpn;
    QFileSystemWatcher *watcherVpnProfiles;

    void refreshVpnProfileList();
    void refreshVpnGroupList();

protected:
    bool eventFilter(QObject *object, QEvent *event);
};

#endif // MAINWINDOW_H
