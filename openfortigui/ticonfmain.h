/*
 *
openfortiGUI - GUI for openfortivpn

Copyright (C) 2017 Rene Hadler, rene@hadler.me, https://hadler.me

    This file is part of tiBackup.

    tiBackupLib is free software: you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation, either version 3 of the License, or
    (at your option) any later version.

    tiBackupLib is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with tiBackupLib.  If not, see <http://www.gnu.org/licenses/>.

*/

#ifndef TICONFMAIN_H
#define TICONFMAIN_H

#include <QSettings>

#include "vpnprofile.h"
#include "vpngroup.h"

class tiConfMain
{
public:
    tiConfMain();
    ~tiConfMain();

    static QString main_config;

    void initMainConf();

    QVariant getValue(const QString &iniPath);
    void setValue(const QString &iniPath, const QVariant &val);
    void sync();

    static QString formatPath(const QString &path);
    static QString formatPathReverse(const QString &path);
    static QString setMainConfig(const QString &config);

    static QString getAppDir();

private:
    QSettings *settings;
};

class tiConfVpnProfiles
{
public:
    tiConfVpnProfiles();
    ~tiConfVpnProfiles();

    void saveVpnProfile(const vpnProfile &profile);
    void readVpnProfiles();
    void setReadProfilePasswords(bool read);

    QList<vpnProfile*> getVpnProfiles();
    vpnProfile* getVpnProfileByName(const QString &vpnname, vpnProfile::Origin sourceOrigin = vpnProfile::Origin_LOCAL);

    bool removeVpnProfileByName(const QString &vpnname);

    bool renameVpnProfile(const QString &oldname, const QString &newname);
    bool copyVpnProfile(const QString &origname, const QString &cpname, vpnProfile::Origin sourceOrigin = vpnProfile::Origin_LOCAL);

private:
    tiConfMain *main_settings;
    bool read_profile_passwords;

    QList<vpnProfile*> vpnprofiles;
};

class tiConfVpnGroups
{
public:
    tiConfVpnGroups();
    ~tiConfVpnGroups();

    void saveVpnGroup(const vpnGroup &group);
    void readVpnGroups();

    QList<vpnGroup *> getVpnGroups();
    vpnGroup* getVpnGroupByName(const QString &groupname);

    bool removeVpnGroupByName(const QString &groupname);

    bool renameVpnGroup(const QString &oldname, const QString &newname);
    bool copyVpnGroup(const QString &origname, const QString &cpname);

private:
    tiConfMain *main_settings;

    QList<vpnGroup*> vpngroups;
};

#endif // TICONFMAIN_H
