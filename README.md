# openfortigui
VPN-GUI to connect to Fortigate-Hardware, based on openfortivpn

For more information visit https://hadler.me/linux/openfortigui/

Official public apt-repo: https://apt.iteas.at/

![alt text](https://hadler.me/wordpress/wp-content/uploads/2018/02/openfortigui1.png "OpenFortiGUI main page")

## SAML single sign-on (SSO)

For gateways that authenticate via SAML (Microsoft SSO and the like), enable
*Login via browser (SAML SSO)* in the VPN profile. On connect, openfortiGUI
opens your web browser on the gateway's SAML login page; after signing in, the
gateway redirects to a local listener (`127.0.0.1`, port configurable in the
profile, default 8020) and the tunnel comes up automatically. Alternatively, a
manually obtained `SVPNCOOKIE` can still be pasted into the profile's Cookie
field.

## Upgrading from 0.9.10 (the Qt5 line)

0.9.11 is the Qt6 port. Profiles, groups, the configuration and stored
passwords carry over unchanged -- there is nothing to migrate by hand. What
does change:

* **Restart openfortiGUI once after the upgrade.** A 0.9.10 GUI that keeps
  running cannot start new connections (it may still call `sudo -E`, which the
  new sudoers rule no longer permits), and a newly started 0.9.11 binary will
  not find the old GUI's socket. Established tunnels keep running.
* **Supported distributions:** Qt6 packages exist for Ubuntu 24.04 and later
  and Debian bookworm and later. Ubuntu 20.04/22.04 and Debian 10/11 stay on
  the 0.9.10 line, apt keeps them there automatically.
* **The KRunner plugin needs KDE Frameworks 6** (Ubuntu 26.04 / Debian trixie
  or later). The KF5 plugin is removed on upgrade -- it cannot talk to 0.9.11.
* `~/.openfortigui` is kept private to the owning user now (0700, files 0600),
  re-tightened on every start. Global profiles under
  `/etc/openfortigui/vpnprofiles` are unaffected.
* The embedded openfortivpn is 1.24.1, which drops the OpenSSL ENGINE code
  path (keys via PKCS#11 engines).
* If you modified `/etc/sudoers.d/openfortigui`, dpkg asks about the changed
  file; keeping your version is safe -- the `SETENV:` tag is simply no longer
  needed.

## Storing passwords in the system keyring

By default openfortiGUI encrypts profile passwords itself, with the AES key and
IV from `~/.openfortigui/main.conf`. Enabling **Use system password store**
(File → Settings) moves key and IV into the keyring of your desktop session
instead, so they are no longer readable in the configuration file.

This needs a Secret Service provider running in your session — openfortiGUI
talks to it through qtkeychain, not to any specific implementation:

| Desktop | Package |
|---|---|
| GNOME, Xfce, Cinnamon, i3/sway, … | `gnome-keyring` (plus `seahorse` to manage it) |
| KDE Plasma | `kwalletmanager` (KWallet is part of Plasma) |

The `libgnome-keyring*` packages named in older issues are obsolete and are not
used. If the keyring is missing or locked, openfortiGUI says so when the option
is switched on and leaves it off.

Note that switching the option changes where key and IV live, not the passwords
themselves. Passwords stored under the previous key have to be entered once
again afterwards.

## The system tray, and Wayland

openfortiGUI lives in the system tray: closing the window hides it and leaves the
tunnel up, and the tray icon brings it back.

**A tray only exists if something provides one.** KDE Plasma ships a
StatusNotifierItem host, and Ubuntu's GNOME enables the AppIndicator extension by
default. Plain GNOME — Fedora Workstation, Debian with GNOME — and wlroots
sessions without a tray applet have none, and then there is no icon. openfortiGUI
notices this: it keeps running with the tunnel up, says so once when you close the
window, and ignores *"start minimized"* rather than starting invisibly. Starting
openfortiGUI again brings the window back — the second start hands the request to
the instance that is already running. On GNOME, installing the AppIndicator
extension gets you a real tray icon.

Two Wayland limits worth knowing about, neither of them fixable from inside the
application:

- **openfortiGUI cannot raise or focus its own window.** A Wayland client needs an
  activation token from the compositor for that, tied to an input event it
  received itself. A click in the tray menu goes to the compositor's panel, and
  the StatusNotifierItem protocol carries no token — so *"Show mainwindow"* maps
  the window but cannot pull it in front of whatever you were looking at. On X11
  it can.
- **Window positions are the compositor's business.** openfortiGUI centres its
  window on startup under X11; under Wayland the compositor places it.

If either matters to you, `QT_QPA_PLATFORM=xcb openfortigui` runs the application
on XWayland, where both work as they do on X11.

## sudo configuration

The VPN process needs root for pppd and the routing table, so openfortiGUI
starts it through `sudo`. The packages install a rule for exactly that one
command:

```
%sudo  ALL=NOPASSWD: /usr/bin/openfortigui --start-vpn *
```

If connecting fails with *"sudo asked for a password"*, that rule does not apply
to your account. Check that `/etc/sudoers` contains `@includedir /etc/sudoers.d`,
and that you are a member of the group the rule names. Domain and AD accounts
usually are **not** in the local `sudo` group — in that case name their group in
the rule instead, rather than granting general sudo rights:

```
%domain\ users  ALL=NOPASSWD: /usr/bin/openfortigui --start-vpn *
```

## Testing

openfortiGUI is tested against a real FortiGate VM running under KVM. See
[TESTS.md](TESTS.md) for requirements and how to run the test suite.
