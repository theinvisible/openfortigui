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

tiConfMain::tiConfMain()
{
    settings = 0;

    initMainConf();

    if(!QFile(openfortigui_config::file_main).exists())
    {
        qCritical() << QString("tiConfMain::tiConfMain() -> Main configuration file <").append(openfortigui_config::file_main).append("> not found, please fix this...");
        exit(EXIT_FAILURE);
    }

    settings = new QSettings(openfortigui_config::file_main, QSettings::IniFormat);
}

tiConfMain::~tiConfMain()
{
    if(settings != 0)
        delete settings;
}

void tiConfMain::initMainConf()
{
    QFile conf_main(openfortigui_config::file_main);
    if(!conf_main.exists())
    {
        QFileInfo finfo(openfortigui_config::file_main);
        QDir conf_main_dir = finfo.absoluteDir();
        conf_main_dir.mkpath(conf_main_dir.absolutePath());

        QString vpnprofiles_dir = QString("%1/vpnprofiles").arg(conf_main_dir.absolutePath());
        QString logs_dir = QString("%1/logs").arg(conf_main_dir.absolutePath());

        QDir backupjobsdir_path(vpnprofiles_dir);
        backupjobsdir_path.mkpath(vpnprofiles_dir);
        QDir logsdir_path(logs_dir);
        logsdir_path.mkpath(logs_dir);

        QSettings conf(openfortigui_config::file_main, QSettings::IniFormat);
        conf.setValue("main/debug", true);
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
