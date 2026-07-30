# openfortigui
VPN-GUI to connect to Fortigate-Hardware, based on openfortivpn

For more information visit https://hadler.me/linux/openfortigui/

Official public apt-repo: https://apt.iteas.at/

![alt text](https://hadler.me/wordpress/wp-content/uploads/2018/02/openfortigui1.png "OpenFortiGUI main page")

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
