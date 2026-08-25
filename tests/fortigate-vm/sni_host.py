#!/usr/bin/env python3
"""A minimal StatusNotifierWatcher/Host, just enough to observe the tray icon.

The lab has no session bus on purpose (lib/gui.sh), so Qt finds no
StatusNotifierItem host and QSystemTrayIcon::isSystemTrayAvailable() is false --
which is what makes cases/90_gui.sh part c2 testable, but leaves the tray icon
itself unobservable. This stub provides the other half: own
org.kde.StatusNotifierWatcher, claim a host is registered (Qt gates the whole
D-Bus tray backend on that one property), and then simply write down every icon
the application publishes.

What lands in the log is one line per observed icon state:

    <seq> <event> sha=<12 hex> name=<IconName> sizes=<WxH,...>

"event" is "initial" for the icon the item already had when it registered and
"NewIcon" for every change signalled afterwards. The sha is over the raw
IconPixmap bytes, so two different icons cannot collide, and re-publishing the
same icon is visible as a second line with an unchanged sha -- which is exactly
what MainWindow::updateTrayIcon() exists to prevent.

Usage: sni_host.py <logfile>
"""
import hashlib
import sys

import dbus
import dbus.service
import dbus.mainloop.glib
from gi.repository import GLib

WATCHER_BUS = "org.kde.StatusNotifierWatcher"
WATCHER_PATH = "/StatusNotifierWatcher"
WATCHER_IFACE = "org.kde.StatusNotifierWatcher"
ITEM_IFACE = "org.kde.StatusNotifierItem"


class Watcher(dbus.service.Object):
    def __init__(self, bus, logfile):
        super().__init__(dbus.service.BusName(WATCHER_BUS, bus), WATCHER_PATH)
        self.bus = bus
        self.items = []
        self.seq = 0
        self.log = open(logfile, "w", buffering=1)

    # -- logging ----------------------------------------------------------
    def note(self, event, service, path):
        """Read the item's icon and write one line about it."""
        self.seq += 1
        sha, name, sizes = "?", "?", "?"
        try:
            props = dbus.Interface(self.bus.get_object(service, path),
                                   "org.freedesktop.DBus.Properties")
            name = str(props.Get(ITEM_IFACE, "IconName"))
            pixmap = props.Get(ITEM_IFACE, "IconPixmap")
            h = hashlib.sha256()
            dims = []
            for width, height, data in pixmap:
                dims.append("%dx%d" % (int(width), int(height)))
                h.update(bytes(bytearray(data)))
            sha = h.hexdigest()[:12]
            sizes = ",".join(dims) or "none"
        except dbus.DBusException as exc:
            sha = "error"
            sizes = str(exc.get_dbus_name())
        self.log.write("%d %s sha=%s name=%s sizes=%s\n"
                       % (self.seq, event, sha, name, sizes))

    # -- StatusNotifierWatcher -------------------------------------------
    @dbus.service.method(WATCHER_IFACE, in_signature="s", sender_keyword="sender")
    def RegisterStatusNotifierItem(self, service, sender=None):
        # The spec allows either a bus name or an object path here; Qt sends a
        # bus name and puts the item on /StatusNotifierItem.
        if service.startswith("/"):
            bus_name, path = sender, service
        else:
            bus_name, path = service, "/StatusNotifierItem"

        self.items.append(bus_name)
        self.StatusNotifierItemRegistered(bus_name)

        # The bus name is what a case needs to reach the item's exported menu
        # (Qt puts it on /MenuBar), so write it down next to the icon log.
        with open(self.log.name + ".item", "w") as fh:
            fh.write("%s %s\n" % (bus_name, path))

        self.bus.add_signal_receiver(
            lambda *a, **kw: self.note("NewIcon", bus_name, path),
            signal_name="NewIcon", dbus_interface=ITEM_IFACE,
            bus_name=bus_name, path=path)

        # The item is not necessarily ready to answer at this instant -- Qt
        # registers and only then finishes setting properties.
        GLib.timeout_add(300, lambda: self.note("initial", bus_name, path) or False)

    @dbus.service.method(WATCHER_IFACE, in_signature="s")
    def RegisterStatusNotifierHost(self, service):
        self.StatusNotifierHostRegistered()

    @dbus.service.signal(WATCHER_IFACE, signature="s")
    def StatusNotifierItemRegistered(self, service):
        pass

    @dbus.service.signal(WATCHER_IFACE, signature="s")
    def StatusNotifierItemUnregistered(self, service):
        pass

    @dbus.service.signal(WATCHER_IFACE, signature="")
    def StatusNotifierHostRegistered(self):
        pass

    # -- properties -------------------------------------------------------
    # Qt reads IsStatusNotifierHostRegistered to decide whether a tray exists at
    # all, so this is the property the whole stub is really about.
    def _props(self):
        return {
            "RegisteredStatusNotifierItems": dbus.Array(self.items, signature="s"),
            "IsStatusNotifierHostRegistered": dbus.Boolean(True),
            "ProtocolVersion": dbus.Int32(0),
        }

    @dbus.service.method("org.freedesktop.DBus.Properties",
                         in_signature="ss", out_signature="v")
    def Get(self, interface, prop):
        try:
            return self._props()[prop]
        except KeyError:
            raise dbus.DBusException("No such property " + prop,
                                     name="org.freedesktop.DBus.Error.UnknownProperty")

    @dbus.service.method("org.freedesktop.DBus.Properties",
                         in_signature="s", out_signature="a{sv}")
    def GetAll(self, interface):
        return self._props()


def main():
    if len(sys.argv) != 2:
        print("usage: sni_host.py <logfile>", file=sys.stderr)
        return 2
    dbus.mainloop.glib.DBusGMainLoop(set_as_default=True)
    Watcher(dbus.SessionBus(), sys.argv[1])
    print("sni_host: watching", flush=True)
    GLib.MainLoop().run()
    return 0


if __name__ == "__main__":
    sys.exit(main())
