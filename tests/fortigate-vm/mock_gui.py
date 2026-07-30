#!/usr/bin/env python3
"""Minimal stand-in for the openfortiGUI GUI.

Creates the QLocalServer socket vpnProcess::setup() connects to, accepts the VPN
child process's connection and then either triggers ACTION_STOP or closes the
connection. This makes the GUI stop path in vpnProcess::closeProcess() testable
headless.

The socket path comes from LAB_API_SOCK (see lib/common.sh) and is the same one
the harness passes to the client as --api-socket. It must not be guessed: the
path is no longer derived from the runtime location, precisely so that the root
child process does not have to share the GUI's environment.

Modes:
  stop        send ACTION_STOP once the tunnel is up
  disconnect  close the connection once the tunnel is up (the GUI exits)
  watch       just accept and hold the connection, change nothing -- for
              testing that the child reaches us at all (80_env)

Usage: mock_gui.py <stop|disconnect|watch> <profile-name> <log-file> [timeout]
"""
import os
import socket
import struct
import sys
import time

SOCK_PATH = os.environ.get("LAB_API_SOCK") or "/tmp/openfortiGUI"
ACTION_STOP = 1


def frame_stop(name: str) -> bytes:
    """vpnApi over QDataStream (Qt_6_0, big endian): action, objName, data."""
    out = struct.pack(">i", ACTION_STOP)
    utf16 = name.encode("utf-16-be")
    out += struct.pack(">I", len(utf16)) + utf16
    out += struct.pack(">I", 0xFFFFFFFF)   # null QByteArray
    return out


def main() -> int:
    mode, profile, logfile = sys.argv[1], sys.argv[2], sys.argv[3]
    timeout = int(sys.argv[4]) if len(sys.argv) > 4 else 60

    if os.path.exists(SOCK_PATH):
        os.unlink(SOCK_PATH)

    srv = socket.socket(socket.AF_UNIX, socket.SOCK_STREAM)
    srv.bind(SOCK_PATH)
    os.chmod(SOCK_PATH, 0o777)
    srv.listen(1)
    srv.settimeout(timeout)
    print("mock-gui: waiting for the child process", flush=True)

    try:
        conn, _ = srv.accept()
    except socket.timeout:
        print("mock-gui: no child process connected", flush=True)
        return 1

    conn.settimeout(1)
    try:
        hello = conn.recv(4096)
        print(f"mock-gui: HELLO ({len(hello)} bytes)", flush=True)
    except socket.timeout:
        print("mock-gui: no HELLO", flush=True)

    if mode == "watch":
        # Hold the connection open and leave the tunnel alone, but report what
        # arrives. The vpnApi payloads are JSON inside the QDataStream frame, so
        # the interesting fields are readable without decoding the framing.
        print("mock-gui: watching, sending nothing", flush=True)
        deadline = time.time() + timeout
        while time.time() < deadline:
            try:
                chunk = conn.recv(8192)
            except socket.timeout:
                continue
            if not chunk:
                print("mock-gui: connection closed by the child", flush=True)
                break
            text = chunk.decode("utf-8", errors="replace")
            for field in ("vpn_start", "bytes_read", "status"):
                idx = text.find(f'"{field}"')
                if idx >= 0:
                    print(f"mock-gui: recv {text[idx:idx + 40]!r}", flush=True)
        return 0

    # Wait until the tunnel is up -- only then is the stop path interesting.
    deadline = time.time() + timeout
    while time.time() < deadline:
        try:
            with open(logfile, "r", errors="replace") as fh:
                if "Tunnel is up and running" in fh.read():
                    break
        except FileNotFoundError:
            pass
        time.sleep(1)
    else:
        print("mock-gui: the tunnel did not come up", flush=True)
        return 1

    time.sleep(2)

    if mode == "stop":
        print("mock-gui: sending ACTION_STOP", flush=True)
        conn.sendall(frame_stop(profile))
        time.sleep(30)
    else:
        print("mock-gui: closing the connection (the GUI exits)", flush=True)
        conn.close()
        time.sleep(30)

    return 0


if __name__ == "__main__":
    try:
        sys.exit(main())
    finally:
        if os.path.exists(SOCK_PATH):
            os.unlink(SOCK_PATH)
