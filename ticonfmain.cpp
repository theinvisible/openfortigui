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

#include "ticonfmain.h"

#include <QDebug>
#include <QFile>
#include <QDirIterator>

#include "config.h"
#include "qtinyaes/QTinyAes/qtinyaes.h"

tiConfMain::tiConfMain()
{
    settings = 0;

    initMainConf();

    if(!QFile(tiConfMain::formatPath(openfortigui_config::file_main)).exists())
    {
        qCritical() << QString("tiConfMain::tiConfMain() -> Main configuration file <").append(openfortigui_config::file_main).append("> not found, please fix this...");
        exit(EXIT_FAILURE);
    }

    settings = new QSettings(tiConfMain::formatPath(openfortigui_config::file_main), QSettings::IniFormat);
}

tiConfMain::~tiConfMain()
{
    if(settings != 0)
        delete settings;
}

void tiConfMain::initMainConf()
{
    QFile conf_main(tiConfMain::formatPath(openfortigui_config::file_main));
    if(!conf_main.exists())
    {
        QFileInfo finfo(tiConfMain::formatPath(openfortigui_config::file_main));
        QDir conf_main_dir = finfo.absoluteDir();
        conf_main_dir.mkpath(conf_main_dir.absolutePath());

        QString vpnprofiles_dir = QString("%1/vpnprofiles").arg(conf_main_dir.absolutePath());
        QString logs_dir = QString("%1/logs").arg(conf_main_dir.absolutePath());

        QDir vpnprofiles_path(vpnprofiles_dir);
        vpnprofiles_path.mkpath(vpnprofiles_dir);
        QDir localvpnprofiles_path(tiConfMain::formatPath(openfortigui_config::vpnprofiles_local));
        localvpnprofiles_path.mkpath(tiConfMain::formatPath(openfortigui_config::vpnprofiles_local));
        QDir logsdir_path(logs_dir);
        logsdir_path.mkpath(logs_dir);

        QSettings conf(tiConfMain::formatPath(openfortigui_config::file_main), QSettings::IniFormat);
        conf.setValue("main/debug", true);
        conf.setValue("main/aeskey", openfortigui_config::aeskey);
        conf.setValue("paths/vpnprofiles", vpnprofiles_dir);
        conf.setValue("paths/localvpnprofiles", openfortigui_config::vpnprofiles_local);
        conf.setValue("paths/logs", logs_dir);
        conf.setValue("paths/initd", openfortigui_config::initd_default);
        conf.sync();
    }
}

QVariant tiConfMain::getValue(const QString &iniPath)
{
    return settings->value(iniPath);
}

void tiConfMain::setValue(const QString &iniPath, const QVariant &val)
{
    settings->setValue(iniPath, val);
}

void tiConfMain::sync()
{
    settings->sync();
}

QString tiConfMain::formatPath(const QString &path)
{
    QString p = path;
    return p.replace("~", QDir::homePath());
}

tiConfVpnProfiles::tiConfVpnProfiles()
{
    main_settings = new tiConfMain();
    QList<vpnProfile*> vpnprofiles;
}

tiConfVpnProfiles::~tiConfVpnProfiles()
{
    delete main_settings;
}

void tiConfVpnProfiles::saveVpnProfile(const vpnProfile &profile)
{
    QString filename = QString(tiConfMain::formatPath(main_settings->getValue("paths/localvpnprofiles").toString())).append("/%1.conf").arg(profile.name);
    QDir localvpndir(tiConfMain::formatPath(main_settings->getValue("paths/localvpnprofiles").toString()));
    if(!localvpndir.exists())
        localvpndir.mkpath(tiConfMain::formatPath(main_settings->getValue("paths/localvpnprofiles").toString()));

    if(QFile::exists(filename))
        QFile::remove(filename);

    QSettings *f = new QSettings(filename, QSettings::IniFormat);
    QTinyAes aes(QTinyAes::CBC, main_settings->getValue("main/aeskey").toByteArray(), openfortigui_config::aesiv);

    f->beginGroup("vpn");
    f->setValue("name", profile.name);
    f->setValue("gateway_host", profile.gateway_host);
    f->setValue("gateway_port", profile.gateway_port);
    f->setValue("username", profile.username);
    f->setValue("password", QString::fromUtf8(aes.encrypt(profile.password.toUtf8()).toBase64()));
    f->endGroup();

    f->beginGroup("cert");
    f->setValue("ca_file", profile.ca_file);
    f->setValue("user_cert", profile.user_cert);
    f->setValue("user_key", profile.user_key);
    f->setValue("verify_cert", profile.verify_cert);
    f->endGroup();

    f->beginGroup("options");
    f->setValue("set_routes", profile.set_routes);
    f->setValue("set_dns", profile.set_dns);
    f->setValue("pppd_use_peerdns", profile.pppd_use_peerdns);
    f->setValue("insecure_ssl", profile.insecure_ssl);
    f->endGroup();

    f->sync();
    delete f;
}

void tiConfVpnProfiles::readVpnProfiles()
{
    // TODO if job objects exist we must *delete* them first
    vpnprofiles.clear();

    QString vpnprofilesdir = tiConfMain::formatPath(main_settings->getValue("paths/localvpnprofiles").toString());
    QDirIterator it_localvpndir(vpnprofilesdir);
    QString vpnprofilefilepath;
    while (it_localvpndir.hasNext())
    {
        vpnprofilefilepath = it_localvpndir.next();
        if(vpnprofilefilepath.endsWith(".conf"))
        {
            qDebug() << "tiConfVpnProfile::readVpnProfiles() -> vpnprofile found:" << vpnprofilefilepath;

            QSettings *f = new QSettings(vpnprofilefilepath, QSettings::IniFormat);
            vpnProfile *vpnprofile = new vpnProfile;
            QTinyAes aes(QTinyAes::CBC, main_settings->getValue("main/aeskey").toByteArray(), openfortigui_config::aesiv);

            f->beginGroup("vpn");
            vpnprofile->name = f->value("name").toString();
            vpnprofile->gateway_host = f->value("gateway_host").toString();
            vpnprofile->gateway_port = f->value("gateway_port").toInt();
            vpnprofile->username = f->value("username").toString();
            vpnprofile->password = QString::fromUtf8(aes.decrypt(QByteArray::fromBase64(f->value("password").toString().toUtf8())));
            f->endGroup();

            f->beginGroup("cert");
            vpnprofile->ca_file = f->value("ca_file").toString();
            vpnprofile->user_cert = f->value("user_cert").toString();
            vpnprofile->user_key = f->value("user_key").toString();
            vpnprofile->verify_cert = f->value("verify_cert").toBool();
            f->endGroup();

            f->beginGroup("options");
            vpnprofile->set_routes = f->value("set_routes").toBool();
            vpnprofile->set_dns = f->value("set_dns").toBool();
            vpnprofile->pppd_use_peerdns = f->value("pppd_use_peerdns").toBool();
            vpnprofile->insecure_ssl = f->value("insecure_ssl").toBool();
            f->endGroup();

            vpnprofiles.append(vpnprofile);
            delete f;
        }
    }
}

QList<vpnProfile *> tiConfVpnProfiles::getVpnProfiles()
{
    return vpnprofiles;
}

vpnProfile *tiConfVpnProfiles::getVpnProfileByName(const QString &vpnname)
{
    readVpnProfiles();
    vpnProfile *vpn = 0;

    for(int i=0; i < vpnprofiles.count(); i++)
    {
        vpn = vpnprofiles.at(i);
        if(vpn->name == vpnname)
            return vpn;
    }

    return vpn;
}

bool tiConfVpnProfiles::removeVpnProfileByName(const QString &vpnname)
{
    qInfo() << "deletevpn:::::" << QString("%1/%2.conf").arg(tiConfMain::formatPath(main_settings->getValue("paths/localvpnprofiles").toString()), vpnname);
    return QFile::remove(QString("%1/%2.conf").arg(tiConfMain::formatPath(main_settings->getValue("paths/localvpnprofiles").toString()), vpnname));
}

bool tiConfVpnProfiles::renameVpnProfile(const QString &oldname, const QString &newname)
{
    return QFile::rename(QString("%1/%2.conf").arg(tiConfMain::formatPath(main_settings->getValue("paths/localvpnprofiles").toString()), oldname),
                         QString("%1/%2.conf").arg(tiConfMain::formatPath(main_settings->getValue("paths/localvpnprofiles").toString()), newname));
}
