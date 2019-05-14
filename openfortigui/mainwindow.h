/*
 *  Copyright (C) 2018 Rene Hadler
 *
 *  This program is free software: you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation, either version 3 of the License, or
 *  (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

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
#include "vpnlogger.h"

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
    void onTbActionSearch();

    void onvpnAdded(const vpnProfile &vpn);
    void onvpnEdited(const vpnProfile &vpn);
    void onvpnGroupAdded(const vpnGroup &vpngroup);
    void onvpnGroupEdited(const vpnGroup &vpngroup);
    void onvpnSearch(const QString &searchtext);

    void onStartVPN();
    void onStartVPN(const QString &vpnname, vpnProfile::Origin origin = vpnProfile::Origin_LOCAL);
    void onActionStartVPN(const QString &vpnname);
    void onActionStartVPNGroup(const QString &vpnname);
    void onStopVPN();
    void onStopVPN(const QString &vpnname);
    void onQuit();
    void onActionAbout();
    void onTrayIconActivated(QSystemTrayIcon::ActivationReason reason);
    void onVPNSettings();
    void onSetupWizard();
    void onActionLogs();
    void onChangelog();

    void onWatcherVpnProfilesChanged(const QString &path);

    void onClientVPNStatusChanged(QString vpnname, vpnClientConnection::connectionStatus status);
    void onClientVPNCredRequest(QString vpnname);
    void onClientVPNOTPRequest(QProcess *proc);
    void onClientCertValidationFAiled(QString vpnname, QString buffer);

    void onClientVPNStatsUpdate(QString vpnname, vpnStats stats);
    void ontvVpnProfilesCustomContextMenu(const QPoint &point);

private:
    Ui::MainWindow *ui;

    QMenu *tray_menu;
    QMenu *tray_group_menu;
    QStandardItem *root_local_vpn, *root_global_vpn;
    QFileSystemWatcher *watcherVpnProfiles;
    QMap<QString, QAction*> trayItems;

    void refreshVpnProfileList();
    void refreshVpnGroupList();
    void autostartVPNs();

    QStandardItem *getVpnProfileItem(const QString &vpnname, int column);

protected:
    bool eventFilter(QObject *object, QEvent *event);
};

#endif // MAINWINDOW_H
