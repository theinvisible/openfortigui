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
#include <QRegularExpression>

#include "config.h"
#include "vpnhelper.h"
#include <qt6keychain/keychain.h>

#include <unistd.h>

QString tiConfMain::main_config = tiConfMain::formatPath(openfortigui_config::file_main);
QString tiConfMain::main_gw_cert_cache = tiConfMain::formatPath(openfortigui_config::file_gw_cert_cache);

namespace {

/*
 * chmod() to owner-only, but only when it is not already owner-only.
 *
 * A chmod() that changes nothing still updates ctime and still emits inotify
 * IN_ATTRIB. MainWindow watches the profile directories, so "set it anyway, it
 * is cheap" is exactly what it is not: every such call was reported as a
 * directory change and triggered another refresh (issue #210).
 *
 * Note the comparison. On Unix QFile reports the owner bits twice -- ReadOwner
 * and ReadUser are the same mode bit -- so an equality test against
 * ReadOwner|WriteOwner never matches and would chmod() every single time. What
 * is checked instead is that no group or other access is left over, that no
 * unwanted execute bit is set, and that the owner can read and write.
 */
void restrictToOwner(const QString &path, bool isDir = false)
{
    const QFileDevice::Permissions wanted = QFileDevice::ReadOwner | QFileDevice::WriteOwner
                                            | (isDir ? QFileDevice::ExeOwner : QFileDevice::Permissions());
    QFileDevice::Permissions unwanted = QFileDevice::ReadGroup | QFileDevice::WriteGroup | QFileDevice::ExeGroup
                                        | QFileDevice::ReadOther | QFileDevice::WriteOther | QFileDevice::ExeOther;
    if(!isDir)
        unwanted |= QFileDevice::ExeOwner | QFileDevice::ExeUser;

    const QFileDevice::Permissions have = QFile(path).permissions();
    if((have & unwanted) == 0 && (have & wanted) == wanted)
        return;

    if(!QFile::setPermissions(path, wanted))
        qWarning() << "tiConfMain -> could not restrict permissions on" << path;
}

} // namespace

tiConfMain::tiConfMain()
{
    initMainConf();

    if(!QFile(tiConfMain::formatPath(tiConfMain::main_config)).exists())
    {
        qCritical() << QString("tiConfMain::tiConfMain() -> Main configuration file <").append(tiConfMain::main_config).append("> not found, please fix this...");
        exit(EXIT_FAILURE);
    }

    settings = std::make_unique<QSettings>(tiConfMain::formatPath(tiConfMain::main_config), QSettings::IniFormat);
    gw_cert_cache = std::make_unique<QSettings>(tiConfMain::formatPath(tiConfMain::main_gw_cert_cache), QSettings::IniFormat);
}

tiConfMain::~tiConfMain() = default;

void tiConfMain::initMainConf()
{
    QFile conf_main(tiConfMain::formatPath(tiConfMain::main_config));
    if(!conf_main.exists())
    {
        QFileInfo finfo(tiConfMain::formatPath(tiConfMain::main_config));
        QDir conf_main_dir = finfo.absoluteDir();
        conf_main_dir.mkpath(conf_main_dir.absolutePath());
        QFile::setPermissions(conf_main_dir.absolutePath(), QFileDevice::ReadOwner | QFileDevice::WriteOwner | QFileDevice::ExeOwner);

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
        // Off by default. Debug logging writes every VPN process line and every
        // list refresh to openfortigui.log, which has no rotation and no size
        // limit -- it is a diagnostic aid, enabled in the settings dialog when
        // it is actually needed (issue #212).
        conf.setValue("main/debug", false);
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
        conf.setValue("gui/disable_notifications", false);
        conf.setValue("gui/connect_on_dblclick", false);
        conf.sync();
        // main.conf holds the AES key -- owner-only, and set explicitly: the
        // fresh file above was created with umask defaults.
        QFile::setPermissions(tiConfMain::formatPath(tiConfMain::main_config),
                              QFileDevice::ReadOwner | QFileDevice::WriteOwner);
    }
    else
    {
        QFileInfo finfo(tiConfMain::formatPath(tiConfMain::main_config));
        QDir conf_main_dir = finfo.absoluteDir();
        conf_main_dir.mkpath(conf_main_dir.absolutePath());
        // The mode of an existing directory is fixed up by migrateMainConf(),
        // not here: this runs for every tiConfMain and must stay free of writes.

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
    }
}

/*
 * The one-time configuration migration, called explicitly from main().
 *
 * It used to sit in initMainConf(), which meant it ran for every single
 * tiConfMain -- including the one logMessageOutput() builds for every log line.
 * Its permission sweep chmod()s every profile file, and chmod() emits inotify
 * IN_ATTRIB even when the mode is unchanged, so MainWindow's watcher on the
 * profile directories reported a change, refreshed the lists, logged while doing
 * so, and every log line swept again -- which is why it did not settle down but
 * ran away: with N profiles one pass produced N log lines and each of those
 * swept N files again. The tray menu was rebuilt without end: it flickered, and
 * QMenu::clear() deleted the very action the user was about to click, so "Show
 * mainwindow" and "Settings" did nothing (issue #210). The same loop kept a core
 * at 100% and wrote the log at ~800 kB/s, some 2.7 GiB per hour (issue #212).
 * With no profiles at all the sweep found no files and nothing happened, which
 * is why the reports only show up once an entry exists.
 *
 * Only ever migrate as the owning user, never from the VPN child process.
 * QSettings replaces the file atomically -- writing it as root would leave
 * main.conf owned by root, after which isWritable() is false and the settings
 * dialog stays disabled for good. The GUI runs before any VPN process, so it is
 * always the one that migrates.
 */
void tiConfMain::migrateMainConf()
{
    if(geteuid() == 0)
        return;

    // Belt and braces. There is a single call site in main(), but a future one
    // must not be able to bring the sweep storm back. Keyed on the config file,
    // because --main-config (setMainConfig) can still change it.
    static QString migrated;
    if(migrated == tiConfMain::main_config)
        return;
    migrated = tiConfMain::main_config;

    QSettings conf(tiConfMain::formatPath(tiConfMain::main_config), QSettings::IniFormat);

    /*
     * AES key and IV used to be written only when main.conf was created
     * from scratch. A configuration carried over from an older version
     * therefore had none, and every stored password decrypted to
     * garbage -- reported as "bad decrypt" and as authentication
     * failures with no visible cause (issues #160, #201). The keys are
     * not a secret in themselves (they sit in main.conf), so filling
     * them in is safe; only the password store variant keeps them
     * elsewhere and is left alone.
     */
    if(!conf.value("main/use_system_password_store", false).toBool()
       && !vpnHelper::aesKeyUsable(conf.value("main/aeskey").toString(),
                                   conf.value("main/aesiv").toString()))
    {
        qWarning() << "AES key/IV missing or invalid in" << tiConfMain::main_config
                   << "-- restoring the defaults. Stored passwords that were"
                      " encrypted with a different key have to be entered again.";
        conf.setValue("main/aeskey", openfortigui_config::aeskey);
        conf.setValue("main/aesiv", openfortigui_config::aesiv);
        conf.sync();
    }

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

    /*
     * "sudo -E" is gone: the VPN child process no longer depends on an
     * inherited environment, it gets the config and socket paths handed
     * over explicitly. Existing configurations must be switched off
     * actively, because sudo-rs (Ubuntu 26.04 and later) rejects -E and
     * the connection would keep failing (issue #208).
     */
    if(!conf.contains("checks/sudo_env_migrated"))
    {
        conf.remove("main/sudo_preserve_env");
        conf.remove("checks/sudopresenv");
        conf.remove("checks/sudopresenv_lastos");
        conf.setValue("checks/sudo_env_migrated", true);
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

    /*
     * Debug logging is off by default now. Every existing configuration carries
     * main/debug=true, because initMainConf() used to write it on every fresh
     * install -- it was never a choice the user made, and openfortigui.log is
     * neither rotated nor capped, so it just kept growing (issue #212).
     *
     * Switched off actively rather than by changing the default, which a stored
     * value would ignore. Runs exactly once: whoever ticks the box in the
     * settings dialog afterwards keeps it on.
     */
    if(!conf.contains("checks/debug_default_migrated"))
    {
        conf.setValue("main/debug", false);
        conf.setValue("checks/debug_default_migrated", true);
        conf.sync();
    }

    /*
     * Existing installations carry umask-default modes from before the files
     * were restricted to the owner, and a profile restored from a backup needs
     * the same treatment. restrictToOwner() only chmod()s what is actually
     * wrong -- an unnecessary chmod() is an inotify event, and that is what fed
     * the refresh loop described above.
     */
    restrictToOwner(QFileInfo(tiConfMain::formatPath(tiConfMain::main_config)).absolutePath(), true);
    restrictToOwner(tiConfMain::formatPath(tiConfMain::main_config));
    const QStringList secretDirs = {
        tiConfMain::formatPath(conf.value("paths/localvpnprofiles", openfortigui_config::vpnprofiles_local).toString()),
        tiConfMain::formatPath(conf.value("paths/localvpngroups", openfortigui_config::vpngroups_local).toString())
    };
    for(const QString &dir : secretDirs)
    {
        QDirIterator it(dir, QStringList() << "*.conf", QDir::Files);
        while(it.hasNext())
            restrictToOwner(it.next());
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
    // Only a leading tilde is a home reference. Replacing every "~" in the
    // string mangled paths that legitimately contain one (issue #157).
    if(!path.startsWith(QLatin1Char('~')))
        return path;

    // "~otheruser/..." refers to a different user's home and needs a passwd
    // lookup, which vpnHelper::linHomeExpansion() does.
    if(!path.startsWith(QLatin1String("~/")) && path != QLatin1String("~"))
        return vpnHelper::linHomeExpansion(path);

    /*
     * When main_config is an absolute path (e.g. passed via --main-config),
     * derive the user's home directory from it instead of using
     * QDir::homePath(), which returns /root when the VPN process runs as root
     * via sudo. Expected layout: <home>/.openfortigui/main.conf
     */
    QString home = QDir::homePath();
    if(QDir::isAbsolutePath(tiConfMain::main_config))
    {
        QFileInfo finfo(tiConfMain::main_config);
        // Go up two levels: main.conf -> .openfortigui -> <home>
        home = QFileInfo(finfo.absolutePath()).absolutePath();
    }

    return home + path.mid(1);
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
    {
        tiConfMain::main_config = config;

        /*
         * Recompute the certificate cache from the constant, not from the old
         * value: main_gw_cert_cache was expanded during static initialization
         * and is already absolute, so formatPath() -- which only replaces a
         * leading "~" -- would keep a wrong home in it. Without this the cache
         * follows HOME instead of --main-config, which meant root's /root in
         * the VPN child process.
         */
        tiConfMain::main_gw_cert_cache = tiConfMain::formatPath(openfortigui_config::file_gw_cert_cache);
    }

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

bool tiConfVpnProfiles::saveVpnProfile(const vpnProfile &profile)
{
    QRegularExpression rexpName(openfortigui_config::validatorName);
    if(!rexpName.match(profile.name).hasMatch())
    {
        qWarning() << "tiConfVpnProfile::saveVpnProfile() -> vpnprofile has not a valid name: " << profile.name;
        return false;
    }

    // Get the keys before touching the file: aborting after the remove below
    // would already have destroyed the stored profile.
    QString aeskey, aesiv;
    if(!vpnHelper::mainAesKeyIv(*main_settings, aeskey, aesiv))
    {
        qWarning() << "tiConfVpnProfile::saveVpnProfile() -> AES key/IV unavailable, not saving:" << profile.name;
        return false;
    }

    QString filename = QString(tiConfMain::formatPath(main_settings->getValue("paths/localvpnprofiles").toString())).append("/%1.conf").arg(profile.name);
    QDir localvpndir(tiConfMain::formatPath(main_settings->getValue("paths/localvpnprofiles").toString()));
    if(!localvpndir.exists())
        localvpndir.mkpath(tiConfMain::formatPath(main_settings->getValue("paths/localvpnprofiles").toString()));

    if(QFile::exists(filename))
        QFile::remove(filename);

    auto f = std::make_unique<QSettings>(filename, QSettings::IniFormat);

    f->beginGroup("vpn");
    f->setValue("name", profile.name.trimmed());
    f->setValue("gateway_host", profile.gateway_host.trimmed());
    f->setValue("gateway_port", profile.gateway_port);
    f->setValue("username", profile.username.trimmed());
    f->setValue("password", vpnHelper::Qaes128_encrypt(profile.password.trimmed(), aeskey, aesiv));
    // The SVPNCOOKIE is a session credential -- store it like the password.
    f->setValue("cookie", vpnHelper::Qaes128_encrypt(profile.cookie.trimmed(), aeskey, aesiv));
    f->setValue("sni", profile.sni.trimmed());
    f->setValue("persistent", profile.persistent);
    f->setValue("device_type", profile.device_type);
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
    f->setValue("pppd_accept_remote", profile.pppd_accept_remote);
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
    f->setValue("saml_login", profile.saml_login);
    f->setValue("saml_port", profile.saml_port);
    f->endGroup();

    f->sync();
    // The remove above makes every save create a fresh file with umask defaults,
    // so this has to happen after every sync, not just once.
    QFile::setPermissions(filename, QFileDevice::ReadOwner | QFileDevice::WriteOwner);

    return true;
}

void tiConfVpnProfiles::readVpnProfiles()
{
    QList<vpnProfile*> vpns = getVpnProfiles();
    for (vpnProfile *vpn : vpns)
    {
        vpn->password = "";
        delete vpn;
    }
    vpnprofiles.clear();

    QMap<vpnProfile::Origin, QString> profileDirs;
    profileDirs[vpnProfile::Origin_LOCAL] = tiConfMain::formatPath(main_settings->getValue("paths/localvpnprofiles").toString());
    profileDirs[vpnProfile::Origin_GLOBAL] = tiConfMain::formatPath(main_settings->getValue("paths/globalvpnprofiles").toString());

    QString aeskey, aesiv;
    bool have_keys = false;
    if(read_profile_passwords)
    {
        have_keys = vpnHelper::mainAesKeyIv(*main_settings, aeskey, aesiv);
        if(!have_keys)
            qWarning() << "tiConfVpnProfile::readVpnProfiles() -> AES key/IV unavailable, profiles are loaded without their secrets";
    }

    QRegularExpression rexpName(openfortigui_config::validatorName);
    for (auto it_profileDirs = profileDirs.cbegin(); it_profileDirs != profileDirs.cend(); ++it_profileDirs)
    {
        QDirIterator it_localvpndir(it_profileDirs.value());
        QString vpnprofilefilepath;
        while (it_localvpndir.hasNext())
        {
            vpnprofilefilepath = it_localvpndir.next();
            if(vpnprofilefilepath.endsWith(".conf"))
            {
                qDebug() << "tiConfVpnProfile::readVpnProfiles() -> vpnprofile found:" << vpnprofilefilepath;
                QString vpnprofilename = QDir(vpnprofilefilepath).dirName().split(".conf")[0];
                if(!rexpName.match(vpnprofilename).hasMatch())
                {
                    qWarning() << "tiConfVpnProfile::readVpnProfiles() -> vpnprofile has not a valid name, skip loading: " << vpnprofilefilepath;
                    continue;
                }

                auto f = std::make_unique<QSettings>(vpnprofilefilepath, QSettings::IniFormat);
                auto *vpnprofile = new vpnProfile;

                f->beginGroup("vpn");
                vpnprofile->name = f->value("name").toString();
                vpnprofile->gateway_host = f->value("gateway_host").toString();
                vpnprofile->gateway_port = f->value("gateway_port").toInt();
                vpnprofile->username = f->value("username").toString();
                if(read_profile_passwords && have_keys) {
                    vpnprofile->password = vpnHelper::Qaes128_decrypt(f->value("password").toString(), aeskey, aesiv);
                    vpnprofile->cookie = vpnHelper::Qaes128_decrypt(f->value("cookie").toString(), aeskey, aesiv);
                }
                vpnprofile->sni = f->value("sni").toString();
                vpnprofile->persistent = f->value("persistent", false).toBool();
                vpnprofile->device_type = static_cast<vpnProfile::Device>(f->value("device_type", 0).toInt());
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
                vpnprofile->pppd_accept_remote = f->value("pppd_accept_remote").toBool();
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
                vpnprofile->saml_login = f->value("saml_login", false).toBool();
                vpnprofile->saml_port = f->value("saml_port", 8020).toInt();
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
    vpnProfile *vpn = nullptr;

    for (vpnProfile *p : vpnprofiles)
    {
        if((p->name == vpnname && p->origin_location == sourceOrigin) || (p->name == vpnname && sourceOrigin == vpnProfile::Origin_BOTH))
            return p;
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
    if(vpn == nullptr)
    {
        qWarning() << "tiConfVpnProfiles::copyVpnProfile() -> source profile not found:" << origname;
        return false;
    }

    vpnProfile newvpn = *vpn;
    newvpn.name = cpname;
    return saveVpnProfile(newvpn);
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
    QRegularExpression rexpName(openfortigui_config::validatorName);
    if(!rexpName.match(group.name).hasMatch())
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

    auto f = std::make_unique<QSettings>(filename, QSettings::IniFormat);

    f->beginGroup("group");
    f->setValue("name", group.name);

    f->beginWriteArray("localMembers");
    for (int i = 0; i < group.localMembers.count(); i++)
    {
        f->setArrayIndex(i);
        f->setValue("name", group.localMembers.at(i));
    }
    f->endArray();

    f->beginWriteArray("globalMembers");
    for (int j = 0; j < group.globalMembers.count(); j++)
    {
        f->setArrayIndex(j);
        f->setValue("name", group.globalMembers.at(j));
    }
    f->endArray();
    f->endGroup();

    f->sync();
    QFile::setPermissions(filename, QFileDevice::ReadOwner | QFileDevice::WriteOwner);
}

void tiConfVpnGroups::readVpnGroups()
{
    QList<vpnGroup*> groups = getVpnGroups();
    for (vpnGroup *group : groups)
    {
        delete group;
    }
    vpngroups.clear();

    QString vpngroupsdir = tiConfMain::formatPath(main_settings->getValue("paths/localvpngroups").toString());
    QDirIterator it_localgroupdir(vpngroupsdir);
    QString vpngroupfilepath;
    QRegularExpression rexpName(openfortigui_config::validatorName);
    while (it_localgroupdir.hasNext())
    {
        vpngroupfilepath = it_localgroupdir.next();
        if(vpngroupfilepath.endsWith(".conf"))
        {
            qDebug() << "tiConfVpnGroups::readVpnGroups() -> vpngroup found:" << vpngroupfilepath;

            QString vpngroupname = QDir(vpngroupfilepath).dirName().split(".conf")[0];
            if(!rexpName.match(vpngroupname).hasMatch())
            {
                qWarning() << "tiConfVpnProfile::readVpnGroups() -> vpngroup has not a valid name, skip loading: " << vpngroupfilepath;
                continue;
            }

            auto f = std::make_unique<QSettings>(vpngroupfilepath, QSettings::IniFormat);
            auto *vpngroup = new vpnGroup;

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
    vpnGroup *vpngroup = nullptr;

    for (vpnGroup *g : vpngroups)
    {
        if(g->name == groupname)
            return g;
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
