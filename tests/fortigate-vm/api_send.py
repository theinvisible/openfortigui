#!/usr/bin/env python3
"""Send a vpnApi command to a running openfortiGUI.

The KRunner plugin does exactly this to start a VPN from outside the GUI
(krunner_openfortigui.cpp: connect to vpnApi::socketPath(), write one frame,
close). We use it as the trigger for the GUI tests so no mouse clicking is
needed to get a connection going.

The frame has no length prefix: the sender writes the QDataStream straight to the
socket and vpnClientConnection::onClientReadyRead() reads it the same way. The
encoding matches frame_stop() in mock_gui.py.

Actions are the enum in vpnapi.h (order matters, values are implicit).

Usage: api_send.py <action> [object-name]
       actions: vpn-start vpn-stop vpngroup-start vpngroup-stop show-main ping
"""
import os
import socket
import struct
import sys

SOCK_PATH = os.environ.get("LAB_API_SOCK") or "/tmp/openfortiGUI"

# vpnapi.h, vpnApi::apiAction
ACTIONS = {
    "hello": 0,
    "stop": 1,
    "vpn-start": 9,
    "vpn-stop": 10,
    "ping": 11,
    "vpngroup-start": 13,
    "vpngroup-stop": 14,
    "show-main": 15,
}


def frame(action: int, name: str) -> bytes:
    """vpnApi over QDataStream (Qt_6_0, big endian): action, objName, data."""
    out = struct.pack(">i", action)
    utf16 = name.encode("utf-16-be")
    out += struct.pack(">I", len(utf16)) + utf16
    out += struct.pack(">I", 0xFFFFFFFF)  # null QByteArray
    return out


def main() -> int:
    if len(sys.argv) < 2 or sys.argv[1] not in ACTIONS:
        print(f"usage: {sys.argv[0]} <{'|'.join(ACTIONS)}> [object-name]",
              file=sys.stderr)
        return 2

    action = ACTIONS[sys.argv[1]]
    name = sys.argv[2] if len(sys.argv) > 2 else ""

    if not os.path.exists(SOCK_PATH):
        print(f"api_send: no socket at {SOCK_PATH} -- is the GUI running?",
              file=sys.stderr)
        return 1

    sock = socket.socket(socket.AF_UNIX, socket.SOCK_STREAM)
    sock.settimeout(5)
    try:
        sock.connect(SOCK_PATH)
        sock.sendall(frame(action, name))
    except OSError as exc:
        print(f"api_send: {exc}", file=sys.stderr)
        return 1
    finally:
        sock.close()

    print(f"api_send: {sys.argv[1]} {name}".rstrip(), flush=True)
    return 0


if __name__ == "__main__":
    sys.exit(main())
