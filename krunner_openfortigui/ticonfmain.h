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

class vpnProfile
{

public:
    explicit vpnProfile();

    enum Origin
    {
        Origin_LOCAL = 0,
        Origin_GLOBAL,
        Origin_BOTH
    };

    QString name;
    Origin origin_location;

    QString gateway_host;
    qint16 gateway_port;
    QString username;
    QString password;
    QString otp;
    QString realm;

    bool set_routes;
    bool set_dns;
    bool pppd_no_peerdns;
    bool half_internet_routers;

    QString ca_file;
    QString user_cert;
    QString user_key;
    QString trusted_cert;
    bool verify_cert;
    bool insecure_ssl;
    bool debug;
    bool autostart;
};

class vpnGroup
{

public:
    explicit vpnGroup();

    QString name;

    QStringList localMembers;
    QStringList globalMembers;
};

class tiConfMain
{
public:
    tiConfMain();
    ~tiConfMain();

    static QString main_config;

    void initMainConf();

    QVariant getValue(const QString &iniPath, const QVariant &defaultValue = QVariant());
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
