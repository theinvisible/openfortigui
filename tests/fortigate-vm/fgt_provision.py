#!/usr/bin/env python3
"""FortiOS provisioning and CLI access for the openfortiGUI test lab.

Talks to the QEMU VM's serial console via pexpect (unix socket through socat)
or alternatively over SSH to port1. Handles the forced password change on first
boot, configures SSL-VPN completely, and can run single commands for test cases.

  fgt_provision.py wait-login
  fgt_provision.py provision [--license FGVM.lic]
  fgt_provision.py cmd "execute vpn sslvpn del-tunnel vpnuser"
  fgt_provision.py show

Parameters come from the environment (see lib/common.sh) so that shell and
Python see the same configuration.
"""

from __future__ import annotations

import argparse
import os
import re
import socket
import struct
import sys
import threading
import time

try:
    import pexpect
except ImportError:  # pragma: no cover
    sys.exit("python3-pexpect fehlt: sudo apt install python3-pexpect")


# --------------------------------------------------------------------------
# Configuration from the environment
# --------------------------------------------------------------------------

def env(name: str, default: str) -> str:
    value = os.environ.get(name, "")
    return value if value else default


CFG = {
    "console_sock": env("LAB_CONSOLE_SOCK", ""),
    "transcript": env("LAB_PROVISION_LOG", "/tmp/fgt-provision.log"),
    "admin_user": env("FGT_ADMIN_USER", "admin"),
    "admin_pass": env("FGT_ADMIN_PASS", "LabAdmin#2024"),
    "admin_sport": env("FGT_ADMIN_SPORT", "443"),
    "hostname": env("FGT_HOSTNAME", "FGT-LAB"),
    "wan_ip": env("FGT_WAN_IP", "10.99.99.10"),
    "lan_ip": env("FGT_LAN_IP", "10.99.10.1"),
    "inside_net": env("LAB_INSIDE_NET", "10.99.10.0/24"),
    "host_ip": env("LAB_HOST_IP", "10.99.99.1"),
    "sslvpn_port": env("FGT_SSLVPN_PORT", "10443"),
    "vpn_user": env("VPN_USER", "vpnuser"),
    "vpn_pass": env("VPN_PASS", "LabVpn#2024"),
    "vpn_group": env("VPN_GROUP", "sslvpn-lab"),
    "vpn_portal": env("VPN_PORTAL", "lab-portal"),
    "policy_nat": env("FGT_POLICY_NAT", "0") not in ("0", "", "no", "false"),
    "boot_timeout": int(env("TIMEOUT_BOOT", "300")),
}

ADDR_INTERNAL = "lab-internal"
POLICY_ID = "10"
POOL = "SSLVPN_TUNNEL_ADDR1"

# FortiOS prompt, also inside config/edit levels: "FGT-LAB (port1) # "
PROMPT = r"[A-Za-z0-9][A-Za-z0-9_.\-]{0,30} (?:\([^)\r\n]{1,60}\) )?# "
ERR_RE = re.compile(
    r"Command fail|command parse error|value parse error|Unknown action"
    r"|entry not found|node_check_object fail|object set operator error"
    r"|Invalid (?:input|value)|Unknown command",
    re.I,
)


class FgtError(RuntimeError):
    pass


def netmask_of(cidr: str) -> str:
    bits = int(cidr.split("/")[1])
    mask = (0xFFFFFFFF << (32 - bits)) & 0xFFFFFFFF
    return ".".join(str((mask >> s) & 0xFF) for s in (24, 16, 8, 0))


# --------------------------------------------------------------------------
# Connection
# --------------------------------------------------------------------------

class Fgt:
    def __init__(self, transport: str, verbose: bool = False):
        self.transport = transport
        self.verbose = verbose
        self.child: pexpect.spawn | None = None
        self.last_prompt = ""
        self._log = open(CFG["transcript"], "a", buffering=1, errors="replace")

    # -- Setup -------------------------------------------------------------

    def open(self) -> None:
        if self.transport == "ssh":
            args = [
                "-o", "StrictHostKeyChecking=no",
                "-o", "UserKnownHostsFile=/dev/null",
                "-o", "LogLevel=ERROR",
                "-o", "PubkeyAuthentication=no",
                "-o", "NumberOfPasswordPrompts=1",
                f"{CFG['admin_user']}@{CFG['wan_ip']}",
            ]
            self.child = pexpect.spawn("ssh", args, timeout=20,
                                       encoding="utf-8", codec_errors="replace")
        else:
            sock = CFG["console_sock"]
            if not sock:
                raise FgtError("LAB_CONSOLE_SOCK is not set")
            if not os.path.exists(sock):
                raise FgtError(f"console socket missing: {sock} (is the VM running?)")
            self.child = pexpect.spawn(
                "socat", ["-,raw,echo=0", f"UNIX-CONNECT:{sock}"],
                timeout=20, encoding="utf-8", codec_errors="replace",
            )
        self.child.logfile_read = self._log
        self.child.delaybeforesend = 0.05

    def close(self) -> None:
        if self.child and self.child.isalive():
            try:
                self.child.sendline("exit")
                self.child.expect([pexpect.EOF, pexpect.TIMEOUT], timeout=3)
            except Exception:
                pass
            self.child.close(force=True)
        self._log.close()

    # -- Login -------------------------------------------------------------

    def wait_login(self, total_timeout: int | None = None) -> None:
        """Log in; handles first boot (empty password + forced change)."""
        total_timeout = total_timeout or CFG["boot_timeout"]
        deadline = time.monotonic() + total_timeout
        candidates = [CFG["admin_pass"], ""]
        cand = 0
        sent_user = False

        patterns = [
            PROMPT,                              # 0 done
            r"[Ll]ogin: ",                       # 1
            r"New Password: ",                   # 2
            r"Confirm Password: ",               # 3
            r"[Rr]etype[^:\r\n]*: ",             # 4 (variant)
            r"[Pp]assword: ",                    # 5
            r"Login incorrect",                  # 6
            r"\(y/n\)",                          # 7
            pexpect.TIMEOUT,                     # 8
            pexpect.EOF,                         # 9
        ]

        # Nudge with an Enter so that a waiting login prompt becomes visible.
        self.child.send("\r")

        while time.monotonic() < deadline:
            idx = self.child.expect(patterns, timeout=5)
            if idx == 0:
                self.last_prompt = self.child.after
                self._settle()
                return
            elif idx == 1:
                self.child.sendline(CFG["admin_user"])
                sent_user = True
            elif idx in (2, 4):
                # Forced change on first boot -> set our lab password
                self.child.sendline(CFG["admin_pass"])
                candidates = [CFG["admin_pass"]]
                cand = 0
            elif idx == 3:
                self.child.sendline(CFG["admin_pass"])
            elif idx == 5:
                self.child.sendline(candidates[cand % len(candidates)])
            elif idx == 6:
                cand += 1
                sent_user = False
                if cand >= len(candidates) * 2:
                    raise FgtError(
                        "login rejected. FGT_ADMIN_PASS does not match the "
                        "image state -- try 'testlab prepare --rebuild'."
                    )
            elif idx == 7:
                self.child.sendline("y")
            elif idx == 8:
                # Still booting or a silent prompt: nudge it
                self.child.send("\r" if sent_user else "\r")
            else:
                raise FgtError("connection to the console was lost (EOF)")

        raise FgtError(f"no login prompt within {total_timeout}s")

    def _settle(self) -> None:
        """Drain the buffer until nothing follows any more."""
        for _ in range(20):
            idx = self.child.expect([PROMPT, pexpect.TIMEOUT, pexpect.EOF], timeout=1)
            if idx == 0:
                self.last_prompt = self.child.after
                continue
            return

    # -- Commands ----------------------------------------------------------

    def cmd(self, line: str, strict: bool = True, timeout: int = 30) -> str:
        if self.verbose:
            print(f"    | {line}")
        self.child.sendline(line)
        out = self._read_to_prompt(timeout)
        if strict and ERR_RE.search(out):
            raise FgtError(f"FortiOS rejected '{line}':\n{out.strip()}")
        return out

    def _read_to_prompt(self, timeout: int = 30) -> str:
        chunks: list[str] = []
        deadline = time.monotonic() + timeout
        while time.monotonic() < deadline:
            idx = self.child.expect(
                [PROMPT, r"--More--", r"\(y/n\)", r"\(y/n/\S+\)",
                 pexpect.TIMEOUT, pexpect.EOF],
                timeout=min(10, timeout),
            )
            chunks.append(self.child.before)
            if idx == 0:
                self.last_prompt = self.child.after
                return "".join(chunks)
            if idx == 1:
                self.child.send(" ")
            elif idx in (2, 3):
                self.child.sendline("y")
            elif idx == 4:
                continue
            else:
                raise FgtError("the console closed the connection")
        raise FgtError(f"timed out waiting for the prompt ({timeout}s)")

    def at_top_level(self) -> bool:
        return "(" not in self.last_prompt

    def leave_config(self) -> None:
        """Return to the top level from any open config/edit levels."""
        for _ in range(6):
            if self.at_top_level():
                return
            try:
                self.cmd("end", strict=False, timeout=10)
            except FgtError:
                return

    def block(self, title: str, lines: list[str], strict: bool = True) -> list[str]:
        """Send a configuration block. Returns the error lines."""
        print(f"  - {title}")
        problems: list[str] = []
        for line in lines:
            try:
                self.cmd(line, strict=strict)
            except FgtError as exc:
                problems.append(f"{line}: {exc}")
                if strict:
                    self.leave_config()
                    raise
        self.leave_config()
        return problems


# --------------------------------------------------------------------------
# Provisioning
# --------------------------------------------------------------------------

def discover_portal(fgt: Fgt) -> str:
    """Determine a usable SSL-VPN portal.

    In evaluation mode vpn.ssl.web.portal is limited to one entry
    ("Too many entries ... vdom-max = 1"), so a dedicated portal cannot be
    created. In that case the existing one gets reconfigured.
    """
    out = fgt.cmd("show vpn ssl web portal", strict=False, timeout=60)
    names = re.findall(r'^\s*edit "([^"]+)"', out, re.M)
    want = CFG["vpn_portal"]
    if want in names:
        return want
    if names:
        print(f"    portal '{want}' cannot be created (evaluation limit), "
              f"using the existing '{names[0]}'")
        return names[0]
    return want


def provision(fgt: Fgt) -> None:
    mask_in = netmask_of(CFG["inside_net"])
    mask_out = "255.255.255.0"

    # Without "output standard", --More-- blocks any automation.
    fgt.block("unpaging the console", [
        "config system console",
        "set output standard",
        "end",
    ], strict=False)

    fgt.block("System", [
        "config system global",
        f"set hostname {CFG['hostname']}",
        "set admintimeout 480",
        f"set admin-sport {CFG['admin_sport']}",
        "end",
    ], strict=False)

    fgt.block("Interfaces", [
        "config system interface",
        "edit port1",
        "set mode static",
        f"set ip {CFG['wan_ip']} {mask_out}",
        "set allowaccess ping https ssh http",
        "set description lab-outside",
        "next",
        "edit port2",
        "set mode static",
        f"set ip {CFG['lan_ip']} {mask_in}",
        "set allowaccess ping",
        "set description lab-inside",
        "next",
        "end",
    ])

    net = CFG["inside_net"].split("/")[0]
    fgt.block("Adressobjekt", [
        "config firewall address",
        f'edit "{ADDR_INTERNAL}"',
        "set type ipmask",
        f"set subnet {net} {mask_in}",
        "next",
        "end",
    ])

    fgt.block("VPN user", [
        "config user local",
        f'edit "{CFG["vpn_user"]}"',
        "set type password",
        f'set passwd "{CFG["vpn_pass"]}"',
        "next",
        "end",
        "config user group",
        f'edit "{CFG["vpn_group"]}"',
        f'set member "{CFG["vpn_user"]}"',
        "next",
        "end",
    ])

    # The portal name may differ from what we want (evaluation limit), so ask first.
    CFG["vpn_portal"] = discover_portal(fgt)

    # Split tunneling matters here: without split routes, openfortivpn replaces
    # the test host's default route (see openfortivpn/src/ipv4.c,
    # ipv4_set_default_routes).
    portal_problems = fgt.block(f"SSL-VPN portal ({CFG['vpn_portal']})", [
        "config vpn ssl web portal",
        f'edit "{CFG["vpn_portal"]}"',
        "set tunnel-mode enable",
        # web-mode MUST stay enabled: FortiOS classifies openfortivpn's POST to
        # /remote/logincheck as tunneltype="ssl-web". With "web-mode disable" the
        # FortiGate rejects the login with
        # reason="sslvpn_login_permission_denied" even though user, group, auth
        # rule and password are all correct.
        "set web-mode enable",
        f'set ip-pools "{POOL}"',
        "set split-tunneling enable",
        f'set split-tunneling-routing-address "{ADDR_INTERNAL}"',
        "next",
        "end",
    ], strict=False)

    if portal_problems:
        # Older/differing field names as a fallback
        fgt.block("SSL-VPN portal (fallback field names)", [
            "config vpn ssl web portal",
            f'edit "{CFG["vpn_portal"]}"',
            "set ipv4-split-tunneling enable",
            f'set ipv4-split-tunneling-routing-address "{ADDR_INTERNAL}"',
            "next",
            "end",
        ], strict=False)

    fgt.block("SSL-VPN settings", [
        "config vpn ssl settings",
        'set servercert "Fortinet_Factory"',
        f'set tunnel-ip-pools "{POOL}"',
        'set source-interface "port1"',
        'set source-address "all"',
        f'set default-portal "{CFG["vpn_portal"]}"',
        f"set port {CFG['sslvpn_port']}",
        f"set dns-server1 {CFG['lan_ip']}",
        "set status enable",
        "config authentication-rule",
        "edit 1",
        f'set groups "{CFG["vpn_group"]}"',
        f'set portal "{CFG["vpn_portal"]}"',
        "next",
        "end",
        "end",
    ])

    # By default FortiOS blocks the source IP for 60 s after two failed attempts
    # (login-attempt-limit 2 / login-block-time 60). The wrong-password and
    # unknown-user cases produce exactly such failures and would make every
    # following case fail with "Empty cookie". So relax it in the lab.
    fgt.block("relaxing the login lockout", [
        "config vpn ssl settings",
        "set login-attempt-limit 10",
        "set login-block-time 0",
        "end",
    ], strict=False)

    fgt.block("firewall policy", [
        "config firewall policy",
        f"edit {POLICY_ID}",
        'set name "sslvpn-lab-to-internal"',
        'set srcintf "ssl.root"',
        'set dstintf "port2"',
        "set action accept",
        'set srcaddr "all"',
        f'set dstaddr "{ADDR_INTERNAL}"',
        'set schedule "always"',
        'set service "ALL"',
        f'set groups "{CFG["vpn_group"]}"',
        f"set nat {'enable' if CFG['policy_nat'] else 'disable'}",
        "next",
        "end",
    ])

    verify(fgt)


def verify(fgt: Fgt) -> None:
    print("  - verification")
    portal = fgt.cmd(
        f'show full-configuration vpn ssl web portal "{CFG["vpn_portal"]}"',
        strict=False, timeout=60,
    )
    split_on = re.search(r"set (?:ipv4-)?split-tunneling enable", portal)
    split_addr = re.search(
        rf'set (?:ipv4-)?split-tunneling-routing-address "?{ADDR_INTERNAL}"?', portal
    )
    if not split_on or not split_addr:
        raise FgtError(
            "split tunneling is not confirmed. Without split routes openfortivpn "
            "would replace the test host's default route -- aborting "
            "provisioning.\n"
            f"split-tunneling: {'ok' if split_on else 'MISSING'}, "
            f"routing-address: {'ok' if split_addr else 'MISSING'}"
        )

    # "show" omits default values (in 7.4, port 10443 and status enable are the
    # defaults), hence full-configuration.
    settings = fgt.cmd("show full-configuration vpn ssl settings",
                       strict=False, timeout=60)
    if f"set port {CFG['sslvpn_port']}" not in settings:
        print(f"    warning: SSL-VPN port {CFG['sslvpn_port']} not in the configuration")
    if "set status enable" not in settings:
        print("    warning: SSL-VPN status is not 'enable'")

    lic = fgt.cmd("get system status", strict=False, timeout=30)
    invalid_license = False
    for key in ("Version:", "License Status", "VM Resources", "Serial-Number:"):
        for line in lic.splitlines():
            if key in line:
                print(f"    {line.strip()}")
                if key == "License Status" and "Invalid" in line:
                    invalid_license = True

    print("  - configuration confirmed (split tunneling active)")

    if invalid_license:
        print("""
    WARNING: this VM has no valid license. An unlicensed FortiGate VM does not
    start the SSL-VPN daemon -- the configuration is accepted, but no listener
    appears on port %s. Running the tests requires a VM license:

        testlab license /path/to/FGVM....lic
""" % CFG["sslvpn_port"])


# --------------------------------------------------------------------------
# License upload via a built-in TFTP server (RRQ only, ~50 lines)
# --------------------------------------------------------------------------

class TinyTftp(threading.Thread):
    """Minimal read-only TFTP server, used only for the license upload."""

    def __init__(self, path: str, bind: str, port: int = 69):
        super().__init__(daemon=True)
        self.path = path
        self.bind = bind
        self.port = port
        self.error: str | None = None
        self.served = threading.Event()
        with open(path, "rb") as handle:
            self._data = handle.read()
        self._sock = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
        self._sock.setsockopt(socket.SOL_SOCKET, socket.SO_REUSEADDR, 1)
        self._sock.bind((bind, port))
        self._sock.settimeout(1.0)
        self._stop = threading.Event()

    def stop(self) -> None:
        self._stop.set()

    def run(self) -> None:
        data = self._data
        while not self._stop.is_set():
            try:
                pkt, peer = self._sock.recvfrom(1024)
            except socket.timeout:
                continue
            if len(pkt) < 4 or struct.unpack("!H", pkt[:2])[0] != 1:  # RRQ only
                continue
            conn = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
            conn.settimeout(5.0)
            block = 1
            offset = 0
            try:
                while True:
                    chunk = data[offset:offset + 512]
                    conn.sendto(struct.pack("!HH", 3, block) + chunk, peer)
                    ack = conn.recvfrom(64)[0]
                    if struct.unpack("!HH", ack[:4]) != (4, block):
                        break
                    offset += 512
                    block = (block + 1) & 0xFFFF
                    if len(chunk) < 512:
                        self.served.set()
                        break
            except Exception as exc:  # pragma: no cover
                self.error = str(exc)
            finally:
                conn.close()


def install_license(fgt: Fgt, lic_path: str) -> None:
    if not os.path.isfile(lic_path):
        raise FgtError(f"license file not found: {lic_path}")
    name = os.path.basename(lic_path)
    print(f"  - license {name} via TFTP from {CFG['host_ip']}")
    try:
        tftp = TinyTftp(lic_path, CFG["host_ip"])
    except PermissionError:
        raise FgtError(
            "TFTP port 69 requires root. Run the script with sudo, or "
            "install the license manually via the GUI (https://%s)." % CFG["wan_ip"]
        )
    tftp.start()
    try:
        # Not through fgt.cmd(): FortiOS reboots right after the restore and cuts
        # the CLI session doing so, so waiting for the prompt would run into EOF
        # or a timeout. Read it ourselves instead, answer the confirmation, and
        # use the TFTP transfer to tell that it worked.
        fgt.child.sendline(f"execute restore vmlicense tftp {name} {CFG['host_ip']}")
        deadline = time.monotonic() + 120
        while time.monotonic() < deadline:
            idx = fgt.child.expect(
                [r"\(y/n\)", r"[Dd]o you want to continue", PROMPT,
                 pexpect.EOF, pexpect.TIMEOUT],
                timeout=5,
            )
            if idx in (0, 1):
                fgt.child.sendline("y")
            elif idx in (2, 3):
                break
            if tftp.served.is_set():
                break

        if not tftp.served.wait(30):
            raise FgtError(
                "the FortiGate did not fetch the license via TFTP. "
                f"Can it reach {CFG['host_ip']}? Check the console transcript."
            )
        if tftp.error:
            raise FgtError(f"TFTP transfer failed: {tftp.error}")
    finally:
        tftp.stop()
    print("  - license transferred, the FortiGate is rebooting")


# --------------------------------------------------------------------------
# CLI
# --------------------------------------------------------------------------

def main() -> int:
    ap = argparse.ArgumentParser(description=__doc__,
                                 formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument("action", choices=["provision", "cmd", "wait-login", "show", "license"])
    ap.add_argument("args", nargs="*", help="commands for 'cmd', or the license path")
    ap.add_argument("--transport", choices=["serial", "ssh"], default="serial")
    ap.add_argument("--timeout", type=int, default=0, help="login timeout in seconds")
    ap.add_argument("-v", "--verbose", action="store_true")
    opts = ap.parse_args()

    fgt = Fgt(opts.transport, verbose=opts.verbose)
    try:
        fgt.open()
        fgt.wait_login(opts.timeout or None)
    except FgtError as exc:
        print(f"fgt_provision: {exc}", file=sys.stderr)
        print(f"console transcript: {CFG['transcript']}", file=sys.stderr)
        return 2

    rc = 0
    try:
        if opts.action == "wait-login":
            print(f"  - login ok ({fgt.last_prompt.strip()})")
        elif opts.action == "provision":
            provision(fgt)
        elif opts.action == "license":
            if not opts.args:
                raise FgtError("path to the .lic file is missing")
            install_license(fgt, opts.args[0])
        elif opts.action == "show":
            for what in ("get system status",
                         "show vpn ssl settings",
                         f'show full-configuration vpn ssl web portal "{CFG["vpn_portal"]}"',
                         "show firewall policy",
                         "diagnose vpn ssl status"):
                print(f"\n===== {what} =====")
                print(fgt.cmd(what, strict=False, timeout=60))
        elif opts.action == "cmd":
            if not opts.args:
                raise FgtError("no command given")
            fgt.block("unpaging the console", [
                "config system console", "set output standard", "end",
            ], strict=False)
            for line in opts.args:
                print(fgt.cmd(line, strict=False, timeout=60))
    except FgtError as exc:
        print(f"fgt_provision: {exc}", file=sys.stderr)
        print(f"console transcript: {CFG['transcript']}", file=sys.stderr)
        rc = 1
    finally:
        fgt.close()
    return rc


if __name__ == "__main__":
    sys.exit(main())
