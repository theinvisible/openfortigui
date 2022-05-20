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

#include "ticonfmain.h"

#include <QDebug>
#include <QFile>
#include <QDirIterator>
#include <QValidator>
#include <QEventLoop>

#include "config.h"
#include "vpnhelper.h"
#include <qt5keychain/keychain.h>

QString tiConfMain::main_config = tiConfMain::formatPath(openfortigui_config::file_main);
QString tiConfMain::main_gw_cert_cache = tiConfMain::formatPath(openfortigui_config::file_gw_cert_cache);

tiConfMain::tiConfMain()
{
    settings = 0;
    gw_cert_cache = 0;

    initMainConf();

    if(!QFile(tiConfMain::formatPath(tiConfMain::main_config)).exists())
    {
        qCritical() << QString("tiConfMain::tiConfMain() -> Main configuration file <").append(tiConfMain::main_config).append("> not found, please fix this...");
        exit(EXIT_FAILURE);
    }

    settings = new QSettings(tiConfMain::formatPath(tiConfMain::main_config), QSettings::IniFormat);
    gw_cert_cache = new QSettings(tiConfMain::formatPath(tiConfMain::main_gw_cert_cache), QSettings::IniFormat);
}

tiConfMain::~tiConfMain()
{
    if(settings != 0)
        delete settings;

    if(gw_cert_cache != 0)
        delete gw_cert_cache;
}

void tiConfMain::initMainConf()
{
    QFile conf_main(tiConfMain::formatPath(tiConfMain::main_config));
    if(!conf_main.exists())
    {
        QFileInfo finfo(tiConfMain::formatPath(tiConfMain::main_config));
        QDir conf_main_dir = finfo.absoluteDir();
        conf_main_dir.mkpath(conf_main_dir.absolutePath());
        QFile::setPermissions(conf_main_dir.absolutePath(), QFileDevice::ReadOwner | QFileDevice::WriteOwner | QFileDevice::ExeOwner | QFileDevice::ReadGroup | QFileDevice::ExeGroup);

        QString logs_dir = QString("%1/logs").arg(conf_main_dir.absolutePath());
        QString logs_vpn_dir = QString("%1/logs/vpn").arg(conf_main_dir.absolutePath());

        QDir localvpnprofiles_path(tiConfMain::formatPath(openfortigui_config::vpnprofiles_local));
        localvpnprofiles_path.mkpath(tiConfMain::formatPath(openfortigui_config::vpnprofiles_local));
        QDir localvpngroups_path(tiConfMain::formatPath(openfortigui_config::vpngroups_local));
        localvpngroups_path.mkpath(tiConfMain::formatPath(openfortigui_config::vpngroups_local));
        QDir logsdir_path(logs_dir);
        logsdir_path.mkpath(logs_dir);
        QDir logsdir_vpn_path(logs_vpn_dir);
        logsdir_vpn_path.mkpath(logs_vpn_dir);

        QSettings conf(tiConfMain::formatPath(tiConfMain::main_config), QSettings::IniFormat);
        conf.setValue("main/debug", true);
        conf.setValue("main/aeskey", openfortigui_config::aeskey);
        conf.setValue("main/aesiv", openfortigui_config::aesiv);
        conf.setValue("main/start_minimized", false);
        conf.setValue("main/setupwizard", false);
        conf.setValue("main/changelogrev_read", 0);
        conf.setValue("paths/globalvpnprofiles", openfortigui_config::vpnprofiles_global);
        conf.setValue("paths/localvpnprofiles", openfortigui_config::vpnprofiles_local);
        conf.setValue("paths/localvpngroups", openfortigui_config::vpngroups_local);
        conf.setValue("paths/logs", logs_dir);
        conf.setValue("paths/initd", openfortigui_config::initd_default);
        conf.setValue("checks/sudopresenv", false);
        conf.setValue("gui/disable_notifications", false);
        conf.setValue("gui/connect_on_dblclick", false);
        conf.sync();
    }
    else
    {
        QFileInfo finfo(tiConfMain::formatPath(tiConfMain::main_config));
        QDir conf_main_dir = finfo.absoluteDir();
        conf_main_dir.mkpath(conf_main_dir.absolutePath());
        QFile::setPermissions(conf_main_dir.absolutePath(), QFileDevice::ReadOwner | QFileDevice::WriteOwner | QFileDevice::ExeOwner | QFileDevice::ReadGroup | QFileDevice::ExeGroup);

        QString vpnprofiles_dir = QString("%1/vpnprofiles").arg(conf_main_dir.absolutePath());
        QString logs_dir = QString("%1/logs").arg(conf_main_dir.absolutePath());
        QString logs_vpn_dir = QString("%1/logs/vpn").arg(conf_main_dir.absolutePath());

        QDir localvpnprofiles_path(tiConfMain::formatPath(openfortigui_config::vpnprofiles_local));
        localvpnprofiles_path.mkpath(tiConfMain::formatPath(openfortigui_config::vpnprofiles_local));
        QDir localvpngroups_path(tiConfMain::formatPath(openfortigui_config::vpngroups_local));
        localvpngroups_path.mkpath(tiConfMain::formatPath(openfortigui_config::vpngroups_local));
        QDir logsdir_path(logs_dir);
        logsdir_path.mkpath(logs_dir);
        QDir logsdir_vpn_path(logs_vpn_dir);
        logsdir_vpn_path.mkpath(logs_vpn_dir);

        QSettings conf(tiConfMain::formatPath(tiConfMain::main_config), QSettings::IniFormat);
        if(!conf.contains("main/setupwizard"))
        {
            conf.setValue("main/setupwizard", false);
            conf.sync();
        }

        if(!conf.contains("main/changelogrev_read"))
        {
            conf.setValue("main/changelogrev_read", 0);
            conf.sync();
        }

        if(!conf.contains("checks/sudopresenv"))
        {
            conf.setValue("checks/sudopresenv", false);
            conf.sync();
        }

        if(!conf.contains("gui/disable_notifications"))
        {
            conf.setValue("gui/disable_notifications", false);
            conf.sync();
        }

        if(!conf.contains("gui/connect_on_dblclick"))
        {
            conf.setValue("gui/connect_on_dblclick", false);
            conf.sync();
        }
    }
}

QVariant tiConfMain::getValue(const QString &iniPath, const QVariant &defaultValue)
{
    return settings->value(iniPath, defaultValue);
}

void tiConfMain::setValue(const QString &iniPath, const QVariant &val)
{
    settings->setValue(iniPath, val);
}

void tiConfMain::sync()
{
    settings->sync();
    gw_cert_cache->sync();
}

void tiConfMain::saveGwCertCache(const QString &vpnname, const QString &certhash)
{
    gw_cert_cache->beginGroup("gw_cert_hashes");
    gw_cert_cache->setValue(vpnname, certhash.trimmed());
    gw_cert_cache->endGroup();
    gw_cert_cache->sync();
}

QString tiConfMain::readGwCertCache(const QString &vpnname)
{
    gw_cert_cache->beginGroup("gw_cert_hashes");
    QString hash = gw_cert_cache->value(vpnname).toString();
    gw_cert_cache->endGroup();

    return hash;
}

bool tiConfMain::isWritable()
{
    return settings->isWritable();
}

QString tiConfMain::formatPath(const QString &path)
{
    QString p = path;
    return p.replace("~", QDir::homePath());
}

QString tiConfMain::formatPathReverse(const QString &path)
{
    QString p = path;
    return p.replace(QDir::homePath(), "~");
}

QString tiConfMain::setMainConfig(const QString &config)
{
    QFile conf_main(tiConfMain::formatPath(config));
    if(conf_main.exists())
        tiConfMain::main_config = config;

    return tiConfMain::main_config;
}

QString tiConfMain::getAppDir()
{
    QFileInfo finfo(tiConfMain::formatPath(tiConfMain::main_config));
    return finfo.absoluteDir().absolutePath();
}

tiConfVpnProfiles::tiConfVpnProfiles()
{
    main_settings = new tiConfMain();
    QList<vpnProfile*> vpnprofiles;
    read_profile_passwords = false;
}

tiConfVpnProfiles::~tiConfVpnProfiles()
{
    delete main_settings;
}

void tiConfVpnProfiles::saveVpnProfile(const vpnProfile &profile)
{
    QRegExp rexpName(openfortigui_config::validatorName);
    if(!rexpName.exactMatch(profile.name))
    {
        qWarning() << "tiConfVpnProfile::saveVpnProfile() -> vpnprofile has not a valid name: " << profile.name;
        return;
    }

    QString filename = QString(tiConfMain::formatPath(main_settings->getValue("paths/localvpnprofiles").toString())).append("/%1.conf").arg(profile.name);
    QDir localvpndir(tiConfMain::formatPath(main_settings->getValue("paths/localvpnprofiles").toString()));
    if(!localvpndir.exists())
        localvpndir.mkpath(tiConfMain::formatPath(main_settings->getValue("paths/localvpnprofiles").toString()));

    if(QFile::exists(filename))
        QFile::remove(filename);

    QString aeskey, aesiv;
    if(main_settings->getValue("main/use_system_password_store").toBool()) {
        aeskey = vpnHelper::systemPasswordStoreRead("aeskey").data;
        aesiv = vpnHelper::systemPasswordStoreRead("aesiv").data;
    } else {
        aeskey = main_settings->getValue("main/aeskey").toString();
        aesiv = main_settings->getValue("main/aesiv").toString();
    }

    QSettings *f = new QSettings(filename, QSettings::IniFormat);

    f->beginGroup("vpn");
    f->setValue("name", profile.name.trimmed());
    f->setValue("gateway_host", profile.gateway_host.trimmed());
    f->setValue("gateway_port", profile.gateway_port);
    f->setValue("username", profile.username.trimmed());
    f->setValue("password", vpnHelper::Qaes128_encrypt(profile.password.trimmed(), aeskey, aesiv));
    f->setValue("persistent", profile.persistent);
    f->endGroup();

    f->beginGroup("cert");
    f->setValue("ca_file", profile.ca_file);
    f->setValue("user_cert", profile.user_cert);
    f->setValue("user_key", profile.user_key);
    f->setValue("verify_cert", profile.verify_cert);
    f->setValue("trusted_cert", profile.trusted_cert);
    f->setValue("trust_all_gw_certs", profile.trust_all_gw_certs);
    f->endGroup();

    f->beginGroup("options");
    f->setValue("set_routes", profile.set_routes);
    f->setValue("set_dns", profile.set_dns);
    f->setValue("pppd_no_peerdns", profile.pppd_no_peerdns);
    f->setValue("insecure_ssl", profile.insecure_ssl);
    f->setValue("debug", profile.debug);
    f->setValue("realm", profile.realm);
    f->setValue("autostart", profile.autostart);
    f->setValue("always_ask_otp", profile.always_ask_otp);
    f->setValue("otp_prompt", profile.otp_prompt);
    f->setValue("otp_delay", profile.otp_delay);
    f->setValue("half_internet_routers", profile.half_internet_routers);
    f->setValue("pppd_log_file", profile.pppd_log_file);
    f->setValue("pppd_plugin_file", profile.pppd_plugin_file);
    f->setValue("pppd_ifname", profile.pppd_ifname);
    f->setValue("pppd_ipparam", profile.pppd_ipparam);
    f->setValue("pppd_call", profile.pppd_call);
    f->setValue("seclevel1", profile.seclevel1);
    f->setValue("min_tls", profile.min_tls);
    f->endGroup();

    f->sync();
    delete f;
}

void tiConfVpnProfiles::readVpnProfiles()
{
    QList<vpnProfile*> vpns = getVpnProfiles();
    for(int i=0; i < vpns.count(); i++)
    {
        vpnProfile *vpn = vpns.at(i);
        vpn->password = "";
        delete vpn;
    }
    vpnprofiles.clear();

    QMap<vpnProfile::Origin, QString> profileDirs;
    profileDirs[vpnProfile::Origin_LOCAL] = tiConfMain::formatPath(main_settings->getValue("paths/localvpnprofiles").toString());
    profileDirs[vpnProfile::Origin_GLOBAL] = tiConfMain::formatPath(main_settings->getValue("paths/globalvpnprofiles").toString());

    QString aeskey, aesiv;
    if(read_profile_passwords)
    {
        if(main_settings->getValue("main/use_system_password_store").toBool()) {
            aeskey = vpnHelper::systemPasswordStoreRead("aeskey").data;
            aesiv = vpnHelper::systemPasswordStoreRead("aesiv").data;
        } else {
            aeskey = main_settings->getValue("main/aeskey").toString();
            aesiv = main_settings->getValue("main/aesiv").toString();
        }
    }

    QMapIterator<vpnProfile::Origin, QString> it_profileDirs(profileDirs);
    QRegExp rexpName(openfortigui_config::validatorName);
    while(it_profileDirs.hasNext())
    {
        it_profileDirs.next();

        QDirIterator it_localvpndir(it_profileDirs.value());
        QString vpnprofilefilepath;
        while (it_localvpndir.hasNext())
        {
            vpnprofilefilepath = it_localvpndir.next();
            if(vpnprofilefilepath.endsWith(".conf"))
            {
                qDebug() << "tiConfVpnProfile::readVpnProfiles() -> vpnprofile found:" << vpnprofilefilepath;
                QString vpnprofilename = QDir(vpnprofilefilepath).dirName().split(".conf")[0];
                if(!rexpName.exactMatch(vpnprofilename))
                {
                    qWarning() << "tiConfVpnProfile::readVpnProfiles() -> vpnprofile has not a valid name, skip loading: " << vpnprofilefilepath;
                    continue;
                }

                QSettings *f = new QSettings(vpnprofilefilepath, QSettings::IniFormat);
                vpnProfile *vpnprofile = new vpnProfile;

                f->beginGroup("vpn");
                vpnprofile->name = f->value("name").toString();
                vpnprofile->gateway_host = f->value("gateway_host").toString();
                vpnprofile->gateway_port = f->value("gateway_port").toInt();
                vpnprofile->username = f->value("username").toString();
                if(read_profile_passwords) {
                    vpnprofile->password = vpnHelper::Qaes128_decrypt(f->value("password").toString(), aeskey, aesiv);
                }
                vpnprofile->persistent = f->value("persistent", false).toBool();
                f->endGroup();

                f->beginGroup("cert");
                vpnprofile->ca_file = f->value("ca_file").toString();
                vpnprofile->user_cert = f->value("user_cert").toString();
                vpnprofile->user_key = f->value("user_key").toString();
                vpnprofile->verify_cert = f->value("verify_cert").toBool();
                vpnprofile->trusted_cert = f->value("trusted_cert").toString();
                vpnprofile->trust_all_gw_certs = f->value("trust_all_gw_certs").toBool();
                f->endGroup();

                f->beginGroup("options");
                vpnprofile->set_routes = f->value("set_routes").toBool();
                vpnprofile->set_dns = f->value("set_dns").toBool();
                vpnprofile->pppd_no_peerdns = f->value("pppd_no_peerdns").toBool();
                vpnprofile->insecure_ssl = f->value("insecure_ssl").toBool();
                vpnprofile->debug = f->value("debug").toBool();
                vpnprofile->realm = f->value("realm").toString();
                vpnprofile->autostart = f->value("autostart").toBool();
                vpnprofile->always_ask_otp = f->value("always_ask_otp").toBool();
                vpnprofile->otp_prompt = f->value("otp_prompt").toString();
                vpnprofile->otp_delay = f->value("otp_delay").toInt();
                vpnprofile->half_internet_routers = f->value("half_internet_routers").toBool();
                vpnprofile->pppd_log_file = f->value("pppd_log_file").toString();
                vpnprofile->pppd_plugin_file = f->value("pppd_plugin_file").toString();
                vpnprofile->pppd_ifname = f->value("pppd_ifname").toString();
                vpnprofile->pppd_ipparam = f->value("pppd_ipparam").toString();
                vpnprofile->pppd_call = f->value("pppd_call").toString();
                vpnprofile->seclevel1 = f->value("seclevel1", false).toBool();
                vpnprofile->min_tls = f->value("min_tls", "default").toString();
                f->endGroup();

                switch(it_profileDirs.key())
                {
                case vpnProfile::Origin_LOCAL:
                    vpnprofile->origin_location = vpnProfile::Origin_LOCAL;
                    break;
                case vpnProfile::Origin_GLOBAL:
                    vpnprofile->origin_location = vpnProfile::Origin_GLOBAL;
                    break;
                case vpnProfile::Origin_BOTH:
                    break;
                }

                vpnprofiles.append(vpnprofile);
                delete f;
            }
        }
    }
}

void tiConfVpnProfiles::setReadProfilePasswords(bool read)
{
    read_profile_passwords = read;
}

QList<vpnProfile *> tiConfVpnProfiles::getVpnProfiles()
{
    return vpnprofiles;
}

vpnProfile *tiConfVpnProfiles::getVpnProfileByName(const QString &vpnname, vpnProfile::Origin sourceOrigin)
{
    readVpnProfiles();
    vpnProfile *vpn = 0;

    for(int i=0; i < vpnprofiles.count(); i++)
    {
        vpn = vpnprofiles.at(i);
        if((vpn->name == vpnname && vpn->origin_location == sourceOrigin) || (vpn->name == vpnname && sourceOrigin == vpnProfile::Origin_BOTH))
            return vpn;
    }

    return vpn;
}

bool tiConfVpnProfiles::removeVpnProfileByName(const QString &vpnname)
{
    qDebug() << "deletevpn:::::" << QString("%1/%2.conf").arg(tiConfMain::formatPath(main_settings->getValue("paths/localvpnprofiles").toString()), vpnname);
    return QFile::remove(QString("%1/%2.conf").arg(tiConfMain::formatPath(main_settings->getValue("paths/localvpnprofiles").toString()), vpnname));
}

bool tiConfVpnProfiles::renameVpnProfile(const QString &oldname, const QString &newname)
{
    return QFile::rename(QString("%1/%2.conf").arg(tiConfMain::formatPath(main_settings->getValue("paths/localvpnprofiles").toString()), oldname),
                         QString("%1/%2.conf").arg(tiConfMain::formatPath(main_settings->getValue("paths/localvpnprofiles").toString()), newname));
}

bool tiConfVpnProfiles::copyVpnProfile(const QString &origname, const QString &cpname, vpnProfile::Origin sourceOrigin)
{
    vpnProfile *vpn = getVpnProfileByName(origname, sourceOrigin);
    vpnProfile newvpn = *vpn;
    newvpn.name = cpname;
    saveVpnProfile(newvpn);

    return true;
}

tiConfVpnGroups::tiConfVpnGroups()
{
    main_settings = new tiConfMain();
    QList<vpnGroup*> vpngroups;
}

tiConfVpnGroups::~tiConfVpnGroups()
{
    delete main_settings;
}

void tiConfVpnGroups::saveVpnGroup(const vpnGroup &group)
{
    QRegExp rexpName(openfortigui_config::validatorName);
    if(!rexpName.exactMatch(group.name))
    {
        qWarning() << "tiConfVpnProfile::saveVpnGroup() -> vpngroup has not a valid name: " << group.name;
        return;
    }

    QString filename = QString(tiConfMain::formatPath(main_settings->getValue("paths/localvpngroups").toString())).append("/%1.conf").arg(group.name);
    QDir localvpndir(tiConfMain::formatPath(main_settings->getValue("paths/localvpngroups").toString()));
    if(!localvpndir.exists())
        localvpndir.mkpath(tiConfMain::formatPath(main_settings->getValue("paths/localvpngroups").toString()));

    if(QFile::exists(filename))
        QFile::remove(filename);

    QSettings *f = new QSettings(filename, QSettings::IniFormat);

    f->beginGroup("group");
    f->setValue("name", group.name);

    f->beginWriteArray("localMembers");
    QListIterator<QString> it(group.localMembers);
    int i = 0;
    while(it.hasNext())
    {
        f->setArrayIndex(i);
        f->setValue("name", it.next());
        i++;
    }
    f->endArray();

    f->beginWriteArray("globalMembers");
    QListIterator<QString> git(group.globalMembers);
    int j = 0;
    while(git.hasNext())
    {
        f->setArrayIndex(j);
        f->setValue("name", git.next());
        j++;
    }
    f->endArray();
    f->endGroup();

    f->sync();
    delete f;
}

void tiConfVpnGroups::readVpnGroups()
{
    QList<vpnGroup*> groups = getVpnGroups();
    for(int i=0; i < groups.count(); i++)
    {
        vpnGroup *group = groups.at(i);
        delete group;
    }
    vpngroups.clear();

    QString vpngroupsdir = tiConfMain::formatPath(main_settings->getValue("paths/localvpngroups").toString());
    QDirIterator it_localgroupdir(vpngroupsdir);
    QString vpngroupfilepath;
    QRegExp rexpName(openfortigui_config::validatorName);
    while (it_localgroupdir.hasNext())
    {
        vpngroupfilepath = it_localgroupdir.next();
        if(vpngroupfilepath.endsWith(".conf"))
        {
            qDebug() << "tiConfVpnGroups::readVpnGroups() -> vpngroup found:" << vpngroupfilepath;

            QString vpngroupname = QDir(vpngroupfilepath).dirName().split(".conf")[0];
            if(!rexpName.exactMatch(vpngroupname))
            {
                qWarning() << "tiConfVpnProfile::readVpnGroups() -> vpngroup has not a valid name, skip loading: " << vpngroupfilepath;
                continue;
            }

            QSettings *f = new QSettings(vpngroupfilepath, QSettings::IniFormat);
            vpnGroup *vpngroup = new vpnGroup;

            f->beginGroup("group");
            vpngroup->name = f->value("name").toString();
            int size = f->beginReadArray("localMembers");
            for (int i = 0; i < size; ++i)
            {
                f->setArrayIndex(i);
                vpngroup->localMembers.append(f->value("name").toString());
            }
            f->endArray();
            int gsize = f->beginReadArray("globalMembers");
            for (int j = 0; j < gsize; ++j)
            {
                f->setArrayIndex(j);
                vpngroup->globalMembers.append(f->value("name").toString());
            }
            f->endArray();
            f->endGroup();


            vpngroups.append(vpngroup);
            delete f;
        }
    }
}

QList<vpnGroup *> tiConfVpnGroups::getVpnGroups()
{
    return vpngroups;
}

vpnGroup *tiConfVpnGroups::getVpnGroupByName(const QString &groupname)
{
    readVpnGroups();
    vpnGroup *vpngroup = 0;

    for(int i=0; i < vpngroups.count(); i++)
    {
        vpngroup = vpngroups.at(i);
        if(vpngroup->name == groupname)
            return vpngroup;
    }

    return vpngroup;
}

bool tiConfVpnGroups::removeVpnGroupByName(const QString &groupname)
{
    qDebug() << "deletegroup:::::" << QString("%1/%2.conf").arg(tiConfMain::formatPath(main_settings->getValue("paths/localvpngroups").toString()), groupname);
    return QFile::remove(QString("%1/%2.conf").arg(tiConfMain::formatPath(main_settings->getValue("paths/localvpngroups").toString()), groupname));
}

bool tiConfVpnGroups::renameVpnGroup(const QString &oldname, const QString &newname)
{
    return QFile::rename(QString("%1/%2.conf").arg(tiConfMain::formatPath(main_settings->getValue("paths/localvpngroups").toString()), oldname),
                         QString("%1/%2.conf").arg(tiConfMain::formatPath(main_settings->getValue("paths/localvpngroups").toString()), newname));
}

bool tiConfVpnGroups::copyVpnGroup(const QString &origname, const QString &cpname)
{
    vpnGroup *vpngroup = getVpnGroupByName(origname);
    vpnGroup newvpngroup = *vpngroup;
    newvpngroup.name = cpname;
    saveVpnGroup(newvpngroup);

    return true;
}
