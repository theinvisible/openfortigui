# krunner_openfortigui

KRunner plugin for openfortiGUI: type the name of a VPN profile or of a group of
profiles into KRunner and it connects, starting openfortiGUI first if it is not
running yet.

For more information visit https://hadler.me/linux/openfortigui/
Official public apt repository: https://apt.iteas.at/

## Requirements

**KDE Frameworks 6.** The Qt5/KF5 version was dropped with the port, so the
plugin is not available on distributions that ship KF5 only — among the currently
supported targets that is Ubuntu 24.04 and Debian bookworm. The application
itself is unaffected and builds there as before.

```bash
apt install cmake extra-cmake-modules qt6-base-dev qt6-declarative-dev \
            libkf6runner-dev libkf6coreaddons-dev libkf6config-dev \
            qtkeychain-qt6-dev libssl-dev
```

`qt6-declarative-dev` is not used by the plugin itself: finding `KF6Config`,
which is in the link interface of `KF6::Runner`, pulls in a `Qt6Qml` dependency.

## Building

```bash
cmake -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build -j
```

The plugin ends up in `build/bin/kf6/krunner/krunner_openfortigui.so`;
`cmake --install build` puts it where KRunner looks for it.

For a package, use the shared build path — the same one CI uses:

```bash
../packaging/build-deb.sh . /tmp/out
```

## Trying it out

```bash
cmake --install build
kquitapp6 krunner; krunner &
```

Then open KRunner and type a profile name.

## Notes on the implementation

The plugin compiles openfortiGUI's own `ticonfmain`, `vpnprofile`, `vpngroup`,
`vpnhelper` and `vpnapi` from `../openfortigui` instead of keeping copies. Copies
used to live here and had drifted: theirs was still the `formatPath()` that
replaced every `~` anywhere in a path, the bug from issue #157. Profiles and paths
are now resolved exactly as the application resolves them.

Connecting is a single `vpnApi` message over openfortiGUI's local socket
(`ACTION_VPN_START` or `ACTION_VPNGROUP_START`); the socket path comes from
`vpnApi::socketPath()`, so it follows the application's `--api-socket`.
