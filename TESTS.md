# Testing openfortiGUI

openfortiGUI talks to real FortiGate hardware, so the only meaningful test is
one against a real FortiGate. The test lab in `tests/fortigate-vm/` boots a
FortiGate VM under KVM, provisions SSL-VPN on it completely automatically, and
runs the openfortiGUI binary headless against it.

```
tests/fortigate-vm/testlab up      # image, network, VM, provisioning  (~5 min first time)
tests/fortigate-vm/testlab test    # all test cases                    (~80 s)
tests/fortigate-vm/testlab down    # VM and network gone
```

Last full run (FortiOS 7.4.12, evaluation license): **7/7 cases, 93 checks
green in 80 s.** Building this lab uncovered three crash and cleanup bugs in
openfortiGUI; all three are fixed, and the test cases now guard them against
regression — see [Findings](#8-findings-in-openfortigui).

---

## 1. Requirements

| What | How to get it |
|---|---|
| `qemu-system-x86_64`, `qemu-img` | `apt install qemu-system-x86 qemu-utils` |
| Write access to `/dev/kvm` | group `kvm` or an ACL — the VM runs **unprivileged** |
| `socat`, `unzip`, `openssl`, `curl`, `iproute2` | usually already installed |
| `python3-pexpect` | `apt install python3-pexpect` |
| `ppp` | `apt install ppp` (openfortivpn needs `/usr/sbin/pppd`) |
| `sudo` | for bridges/taps/netns and the VPN client (pppd needs root) |
| FortiGate KVM image | `FGT_VM64_KVM-*.kvm.zip` from Fortinet |
| FortiGate VM license | see [VM license](#7-vm-license) — **effectively required** |

libvirt is **not** needed. The lab drives QEMU directly over serial and QMP
unix sockets.

The Fortinet image is not freely redistributable and therefore lives outside
the repository. Without `FGT_IMAGE_ZIP` the newest
`$HOME/FGT_VM64_KVM-*.kvm.zip` is picked automatically. All runtime artifacts
go to `$HOME/.cache/openfortigui-testlab` (override with `OFGUI_LAB_DIR`);
`.gitignore` additionally blocks `*.qcow2`, `*.kvm.zip` and `*.lic`.

`preflight` checks all of this and tells you what is missing:

```bash
tests/fortigate-vm/testlab preflight
```

## 2. Running the tests

### 2.1 One-time preparation

Build the binary under test and grant a sudo ticket. Everything that touches
bridges, taps, network namespaces and pppd needs root, and the harness must
not stop for a password prompt in the middle of a run:

```bash
cd /path/to/openfortigui
cmake --build cmake-build-debug --target openfortigui

sudo -v                                    # refresh the sudo ticket
tests/fortigate-vm/testlab preflight
```

Optionally create a config file. It is gitignored and documents every knob;
environment variables win over the file:

```bash
cp tests/fortigate-vm/lab.env.example tests/fortigate-vm/lab.env
```

The single setting worth putting there is the license:

```ini
FGT_LICENSE=/path/to/FGVM....lic
```

### 2.2 Bring the lab up

```bash
tests/fortigate-vm/testlab up
```

This runs preflight, unpacks the image, creates the bridges/taps/netns, boots
the VM in provisioning mode, configures FortiOS, installs the license if
`FGT_LICENSE` is set, shuts the VM down, marks the result as the golden image
and boots a throwaway overlay from it. First run takes about 5 minutes; every
later `up` reuses the golden image and takes around 40 seconds.

Check what you got:

```bash
tests/fortigate-vm/testlab status
```

```
Image:
  version      7.4.12-b2902
  base         present
  golden       provisioned
Network:
  ofgt-out     present
  ofgt-in      present
  netns        present
  target       running (10.99.10.50:8080)
VM:
  QEMU         running (PID 78292)
SSL-VPN:
  port         open (10.99.99.10:10443)
    Protocol: TLSv1.3
  cert SHA256  0ba4de6c13783d262c44fd3bd482e750fa6c8a47eeae9d4458a48d0257df8b8a
Client:
  binary       /path/to/openfortigui/cmake-build-debug/bin/openfortigui
  test home    /home/you/.cache/openfortigui-testlab/client/home
```

### 2.3 Run the cases

```bash
tests/fortigate-vm/testlab test                       # everything
tests/fortigate-vm/testlab test 10_connect 20_routing # selected cases
tests/fortigate-vm/testlab test --keep-going          # do not stop on the first failure
```

Each case prints `ok` / `FAIL` / `skip` per check and a summary per case. The
exit code is non-zero if any case failed. A single case can also be run
directly, which is convenient while iterating:

```bash
tests/fortigate-vm/cases/20_routing.sh
```

Two checks are opt-in:

| Switch | Effect |
|---|---|
| `OFGUI_TEST_DATAPATH=1` | reachability of the target behind the FortiGate plus source-IP check. Needs a FortiGate that actually forwards, see [VM license](#7-vm-license). |
| `OFGUI_TEST_DNS=1` | DNS push. openfortivpn writes to `/etc/resolv.conf` directly; the case backs the file up and restores it afterwards. |

```bash
OFGUI_TEST_DATAPATH=1 OFGUI_TEST_DNS=1 tests/fortigate-vm/testlab test 20_routing
```

### 2.4 Where the output goes

```
$OFGUI_LAB_DIR/out/<case>/client.log        openfortivpn/pppd output of the run
$OFGUI_LAB_DIR/out/<case>/openfortigui.log  the application log
$OFGUI_LAB_DIR/out/results/<case>.tsv       one row per check
$OFGUI_LAB_DIR/out/junit.xml                collected JUnit report
$OFGUI_LAB_DIR/out/console.log              full FortiGate serial transcript
$OFGUI_LAB_DIR/out/provision.log            the complete provisioning CLI dialog
```

### 2.5 Tear down

```bash
tests/fortigate-vm/testlab down     # VM and network gone, golden image kept
tests/fortigate-vm/testlab clean    # also delete images and the test home
```

`down` is idempotent and also runs on abort. It removes interfaces, the
network namespace, the iptables rules it added and any client leftovers. It
only resets `net.ipv4.ip_forward` if it turned it on itself.

### 2.6 Command reference

| Command | Effect |
|---|---|
| `preflight` | check tools, permissions, image, sudo ticket, binary |
| `up` | preflight → prepare → net-up → vm-up → provision → golden → vm-up |
| `test [case…]` | run test cases; without arguments all of them. `--keep-going` continues past failures |
| `down` | stop the VM, kill client leftovers, tear the network down |
| `clean` | delete the work directory including images |
| `status` | image, network, VM and SSL-VPN state including TLS info and cert digest |
| `prepare [--rebuild]` | unpack the image, create the golden overlay; `--rebuild` discards the provisioning |
| `net-up` / `net-down` | just the network part |
| `vm-up` / `vm-down` | just the VM |
| `provision` | configure FortiOS on the running VM |
| `console` | serial FortiGate console (Ctrl-O quits) |
| `fgt "<cli>"` | run a FortiOS command, e.g. `fgt "diagnose vpn ssl list"` |
| `show` | dump the configuration (status, SSL-VPN, portal, policies) |
| `cert-digest` | SHA-256 of the gateway certificate (same value as `--trusted-cert`) |
| `license <file>` | install a VM license into the golden image via the built-in TFTP server |
| `uplink on\|off` | internet for the FortiGate via NAT through the host (license activation) |

## 3. Topology

```
Host (test client)               FortiGate VM 7.4.x            netns "ofgt-inside"
 10.99.99.1/24 ──ofgt-out(br)──── port1 10.99.99.10
      │                            admin HTTPS :443
      │                            SSL-VPN     :10443
      │
      └─ ppp0 10.212.134.2xx ════ ssl.root   (pool SSLVPN_TUNNEL_ADDR1)
                                    │  policy ssl.root→port2, NAT off
                                  port2 10.99.10.1/24 ──ofgt-in(br)── 10.99.10.50:8080
```

The host deliberately has **no address** on `ofgt-in`: `10.99.10.50` is
reachable through the tunnel only. A successful `curl` therefore proves the
tunnel, the pushed routes and the FortiGate policy in one step. The HTTP
server inside the network namespace logs the source IP, so the test can also
verify that the tunnel pool address is what arrives.

Because Docker sets the `FORWARD` policy to `DROP`, and a loaded
`br_netfilter` sends bridged packets through `FORWARD` as well, `net-up`
installs two targeted `ACCEPT` rules for the two bridges and removes them
again on `net-down`. Global sysctls are left alone.

### Image layers

```
base-<version>.qcow2          unpacked, read-only
  └─ golden-<version>.qcow2   provisioned (first-time setup lands here)
       └─ run/disk.qcow2      throwaway overlay, recreated on every vm-up
```

Every `vm-up` therefore starts from an identical state, and the first-time
setup runs only once. `prepare --rebuild` forces a re-provisioning.

## 4. Isolation from your real user profile

The test client gets a home of its own. This exploits
`tiConfMain::formatPath()` (`openfortigui/ticonfmain.cpp:183`): if
`--main-config` is an absolute path, openfortiGUI derives the home directory
from two levels above it instead of using `QDir::homePath()`.

```
$OFGUI_LAB_DIR/client/home/.openfortigui/main.conf   →  home = .../client/home
                          /vpnprofiles/*.conf
                          /logs/openfortigui.log
                          /gw_cert.cache
```

**`--main-config` alone is not enough.** Two paths depend on `HOME` rather
than on the config path passed in, so the harness also sets `HOME` to the test
home (`CLIENT_EXTRA_ENV` in `lib/client.sh`):

- `tiConfMain::main_gw_cert_cache` is initialized statically
  (`ticonfmain.cpp:32`) and never updated by `setMainConfig()`.
- `logMessageOutput()` opens `openfortigui.log` on the first `qDebug`
  (`main.cpp:150`), which happens **before** `setMainConfig()`, and the path
  stays fixed afterwards. Without `HOME`, the application log ends up in
  `/root/.openfortigui/logs/`, because `sudo` sets `HOME` to `/root`.

With `HOME` set, the user's `~/.openfortigui` is never read or written.

The test `main.conf` sets `use_system_password_store=false`. With the password
store enabled, the root child process asks the GUI for key and IV over the
local socket and times out after 30 s (`proc/vpnprocess.cpp:107`) — not
solvable headless. Profile passwords are instead encrypted with the default
keys from `openfortigui/config.h`, byte-identical to
`vpnHelper::Qaes128_encrypt()`:

```bash
printf '%s' "$pw" | openssl enc -aes-128-cbc -a -A \
    -K "$(printf %s yowp2IwTTRodgdWp | od -An -tx1 | tr -d ' \n')" \
    -iv "$(printf %s VoUT5n5ToogkmQU3 | od -An -tx1 | tr -d ' \n')"
```

## 5. Test cases

| Case | What it checks | Checks |
|---|---|---|
| `10_connect` | connect with a pinned certificate: `Tunnel is up and running.`, ppp interface with a pool address, application log free of Critical/Fatal | 9 |
| `20_routing` | split route to the inside network, **default route unchanged**, byte counters from `/proc/net/dev`; data path and DNS push optional | 5 |
| `30_cert` | unknown certificate is rejected and the digest logged exactly the way `MainWindow` extracts it by regex; wrong digest; `insecure_ssl` alone does **not** disable the check; `gw_cert.cache`; `min_tls`/`seclevel1`/TLS 1.3 | 13 |
| `40_auth` | wrong password, unknown user, missing profile, missing password without a GUI, exit code of the error path | 14 |
| `50_disconnect` | SIGTERM: clean teardown, ppp gone, routes and addresses as before, split route removed, no orphaned gateway route, no process leftovers, exit code | 15 |
| `60_persistent` | a server-side killed tunnel (`execute vpn sslvpn del-tunnel <index>`) is rebuilt via `persistent=true`; SIGTERM stops the process despite `persistent` | 15 |
| `70_guistop` | stop initiated by the GUI while the tunnel is up: `ACTION_STOP` over the local socket, and the GUI going away. Uses `mock_gui.py`, which provides the `openfortiGUI` QLocalServer | 22 |

Four of these checks used to report the crashes described under
[Findings](#8-findings-in-openfortigui). Since the fix they are regression
guards: they assert the exit code, the absence of `invalid pointer` in the
log, and that the `/32` route to the gateway is rolled back.

### What the profiles need

`client_write_profile` sets two options differently from the openfortiGUI
defaults, both necessary:

- **`pppd_accept_remote=true`** — openfortiGUI passes `:169.254.2.1` to pppd
  as the remote address, but the FortiGate proposes its own (here the port1
  IP). Without this option `ipcp-accept-remote` is missing
  (`vpnworker.cpp:276`) and pppd fails with *"Peer refused to agree to his IP
  address"*. This is not a lab peculiarity — it applies to any FortiGate that
  dictates its own remote address.
- **`debug=true`** — otherwise the log lines the assertions rely on are
  missing.

### What the FortiGate needs in the lab

Three settings the provisioning applies that are not obvious:

- **`web-mode enable`** on the portal. FortiOS classifies openfortivpn's POST
  to `/remote/logincheck` as `tunneltype="ssl-web"`. With `web-mode disable`
  the login is rejected with `reason="sslvpn_login_permission_denied"` even
  though user, group, auth rule and password are all correct — a thoroughly
  misleading trail.
- **`login-block-time 0`** and `login-attempt-limit 10`. The default is "block
  the source IP for 60 s after 2 failed attempts". The error-path tests
  produce exactly such failures and would make every following case fail with
  `Empty cookie`.
- **`split-tunneling enable`** plus `split-tunneling-routing-address`. Without
  split routes, openfortivpn deletes the test host's default route
  (`openfortivpn/src/ipv4.c:922`). Provisioning aborts if it cannot confirm
  this setting.

## 6. Extending the suite

A case is a standalone bash script under `cases/`. Discovery is by glob, so
dropping a file in is enough. The scaffolding comes from `lib/case.sh`:

```bash
#!/usr/bin/env bash
set -uo pipefail
source "$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)/lib/case.sh"
case_setup                     # checks network/VM/binary, cleans leftovers,
                               # provides $LAB_GW_DIGEST

PROFILE="my-case"
LOG="$(case_log client)"
client_write_profile "$PROFILE" "trusted_cert=$LAB_GW_DIGEST" >/dev/null
client_start "$PROFILE" "$LOG" || fail "process did not start"

if client_wait_log "$LOG" 'Tunnel is up and running' "$TIMEOUT_CONNECT"; then
    ok "tunnel is up"
else
    fail "tunnel did not come up" "$(tail -n 25 "$LOG")"
fi

client_stop TERM 45
case_finish                    # writes the result rows, sets the exit code
```

Useful helpers:

| Helper | Purpose |
|---|---|
| `ok` / `fail` / `skip` | record one check |
| `assert_eq`, `assert_true`, `assert_false`, `assert_contains`, `assert_not_contains` | common assertions |
| `part "…"` | section heading inside a case |
| `client_write_profile <name> [k=v…]` | write a profile, password encrypted |
| `client_start` / `client_stop` / `client_kill` / `client_cleanup_all` | process lifecycle |
| `client_exit_code`, `client_wait_log`, `client_wait_log_count`, `client_wait_exit` | wait for and inspect results |
| `ppp_iface`, `ppp_ip`, `wait_ppp_up`, `route_to_dev`, `route_snapshot`, `iface_bytes` | network state |
| `fgt_cli "<cmd>"` | run a FortiOS command from inside a case |

## 7. VM license

**An unlicensed FortiGate VM does not start the SSL-VPN daemon at all.**
Measured on FortiOS 7.4.12 build2902.

With an evaluation license installed, `sslvpnd` runs, listens on 10443, and
login, PPP negotiation and route push all work. The FortiGate however
**forwards nothing**: `get vpn ssl monitor` shows the session with
`I/O Bytes 0/0`, the routing table has no host route to the tunnel IP, and a
flow trace on the FortiGate sees no packet at all. `get system status` still
reports `License Status: No License`, serial `FGVM00UNLICENSED`,
`VM Resources: 1 CPU/0 allowed`; the API says `forticare.status: pending`,
`registration_status: registrable`. The VM wants to be registered with
FortiCare to become fully usable.

For the purpose of this lab — testing the openfortiGUI client — that is
enough: the tunnel comes up and every client-side check runs. Reachability
behind the FortiGate is therefore optional, behind `OFGUI_TEST_DATAPATH=1`.

One side effect is worth knowing: because no payload flows, pppd's LCP echoes
go unanswered and the tunnel dies on its own after roughly two minutes
(*"Serial link appears to be disconnected"*, `lcp-echo-interval 30` /
`lcp-echo-failure 4` from `/etc/ppp/options`). All test cases are shorter than
that, but it is the time limit for your own experiments.

### Getting a license in

Request a free 15-day evaluation license at
[support.fortinet.com](https://support.fortinet.com) for the VM's serial
number (`testlab fgt "get system status" | grep Serial`), then:

```bash
tests/fortigate-vm/testlab license /path/to/FGVM....lic
```

This boots the VM in provisioning mode, uploads the license through a built-in
mini TFTP server and thereby writes it into the golden image — otherwise it
would be gone on the next `vm-up`. Alternatively set `FGT_LICENSE` in
`lab.env` and `testlab up` does it as part of the run.

Activation needs to reach FortiCare, but the lab network is isolated.
`testlab license` therefore enables a NAT uplink through the host
automatically (`net.ipv4.ip_forward` plus MASQUERADE for `10.99.99.0/24`, plus
a default route and DNS on the FortiGate), and `testlab down` removes it
again. Verified: `execute ping 1.1.1.1` on the FortiGate answers. Controllable
manually via `testlab uplink on|off`, DNS via `LAB_DNS_SERVER`.

### Evidence that it really is the license

FortiOS says so itself, via `/api/v2/monitor/license/status`:

```json
"vm": { "valid": false, "status": "vm_invalid", "license_source": "local",
        "license_platform_name": "FGVMEV",
        "cpu_used": 1, "cpu_max": 1,
        "mem_used": 2089811968, "mem_max": 2147483648 },
"forticare": { "registration_status": "unregistrable",
               "registration_supported": false }
```

What matters about those numbers: `cpu 1/1` and `mem 1993/2048 MB` are
*within* the evaluation limits. The crashlog line
`from=license msg=VM resource exceeds license limit / CPU:1/1, MEM:1993/2048`
already fires at equality and is misleading — resources are not the cause.
`registration_supported: false` also means the VM cannot fetch a trial license
by itself even with internet access.

Further evidence:

- The configuration is accepted in full — `show full-configuration vpn ssl
  settings` shows `set status enable`, `set port 10443`,
  `set servercert "Fortinet_Factory"`, and a portal with
  `split-tunneling enable`.
- Still no listener: port 10443 answers `connection refused`,
  `diagnose sys tcpsock` has no sslvpn socket, and `diagnose vpn ssl list`
  returns `client connect failed 111: Connection refused`. The word `sslvpn`
  does **not** appear anywhere in the 323-line crashlog — the daemon does not
  crash, it is never started. `diagnose debug application sslvpn -1` produces
  not a single line when the service is toggled.
- After login the GUI redirects straight to `/system/vm/license`, and every
  `/api/v2/monitor/...` endpoint except `license/status` answers
  `401 Unauthorized`. The box is locked behind the license page.
- `ssl.root` exists and is `status: up`, so the evaluation interface limit is
  not blocking anything.
- Admin HTTPS works fine, port1/port2 are up, ping and TLS work.

Ruled out empirically: SSL-VPN port (10443, and 443 after moving the admin
port), `source-interface`, `default-portal`, `authentication-rule`,
`web-mode` on and off, re-enabling the service, reboot, memory (1024 and
2048 MB), machine type (`q35` and `pc`), NIC model (`virtio-net-pci` and
`e1000`), and log disk (absent and `Available`).

**Crypto is not a problem.** The TLS handshake against the evaluation VM works
with openfortivpn's default cipher list
(`HIGH:!aNULL:!kRSA:!PSK:!SRP:!MD5:!RC4`) — measured against the admin port.
The feared low-encryption restriction does not appear here, and
`seclevel1`/`min_tls` workarounds are not needed. Should an explicit cipher
list ever be required: openfortiGUI never sets `vpn_config.cipher_list`
(`proc/vpnworker.cpp`), so it would not be configurable.

**Resource limits.** Evaluation mode allows 1 vCPU and 2 GB, hence
`-smp 1 -m 2048`. On top of that `vpn.ssl.web.portal` is limited to one entry
(`Too many entries ... vdom-max = 1`), so a dedicated portal cannot be
created. The provisioning detects this and reconfigures the existing portal
(`full-access`) instead.

## 8. Findings in openfortiGUI

Findings 1, 2 and 4 are **fixed** — they are documented here because the test
cases keep guarding them. Finding 3 lives in the openfortivpn submodule and is
reported upstream rather than patched locally. Finding 5 is documented
behaviour.

### 1. Crash on every disconnect (SIGSEGV, exit code 139) — fixed

```
#0  vpnWorker::end ()            vpnworker.cpp   ptr_tunnel->on_ppp_if_down(ptr_tunnel);
#1  vpnProcess::closeProcess ()  vpnprocess.cpp
#2  queued signal delivery on the main thread
```

`closeProcess()` is attached to `vpnWorker::finished()` as a *queued* slot and
therefore runs on the main thread after `vpnWorker::process()` has already
returned. `vpnWorker::end()` dereferenced `ptr_tunnel` there without a check —
a pointer to the **stack-local** `struct tunnel` from `process()`, whose stack
frame no longer exists. This hit every teardown: the error path (auth failure)
just like a regular SIGTERM, and via `ACTION_STOP` the GUI as well. Two more
accesses to the same pointer were affected — `vpnProcess::onStatsUpdate()` did
not check it at all, even though its 2 s timer can fire before `process()`
sets it.

The fix has three parts:

* `ptr_tunnel` is private, sits behind a `QMutex` and is only read through
  `tunnelActive()` / `tunnelState()` / `tunnelPppIface()`. `process()` clears
  it **before** `emit finished()`.
* `closeProcess()` no longer tears the tunnel down across threads. It raises
  `kill(getpid(), SIGTERM)` instead, taking the same path an external SIGTERM
  takes: `io_loop()` aborts, `process()` cleans up on the owning thread and
  emits `finished()`. Instead of `QThread::terminate()` — which orphaned pppd —
  there is a watchdog as the last resort.
* `thread_worker`/`thread_vpn` are `QPointer`, because both are attached to
  `finished()` via `deleteLater()` and `closeProcess()` runs after that.

Covered by `40_auth` a), `50_disconnect` and `70_guistop`.

### 2. Heap corruption on the first reconnect (SIGABRT, exit code 134) — fixed

```
#7  malloc_printerr ()             malloc.c:5341
#9  ipv4_set_split_routes ()       openfortivpn/src/ipv4.c:902   free(route_iface(route));
#10 ipv4_set_tunnel_routes ()      openfortivpn/src/ipv4.c:1009
#11 on_ppp_if_up ()                proc/vpnworker.cpp:123
#12 if_config ()                   openfortivpn/src/io.c:585
```

The persistent branch freed `tunnel.ipv4.split_rt` and nulled the pointer but
**did not reset `tunnel.ipv4.split_routes`**. On `goto start_tunnel`,
`ipv4_add_split_vpn_route` handed out the next index based on the stale
counter (`split_rt[split_routes++]`, `ipv4.c:868`), leaving index 0 as
uninitialized malloc memory. `ipv4_set_split_routes` then iterated from index 0
and called `free()` on a garbage pointer — glibc aborts.

Fixed by `tunnel.ipv4.split_routes = 0` next to the `free()`. The `rt_dev` of
each split route is now released as well; it used to leak on every reconnect.
Covered by `60_persistent`; four consecutive reconnects run through without a
single `invalid pointer`.

### 3. Orphaned gateway route after a crash — upstream

`ipv4_protect_tunnel_route()` (`openfortivpn/src/ipv4.c:729`) installs a `/32`
route to the VPN gateway. If the gateway is directly attached to a secondary
interface, `ipv4_get_route()` determines the **wrong** next hop for it — the
default gateway instead of the direct connection:

```
DEBUG:  ip route show to 10.99.99.10/255.255.255.255 dev !ppp0
DEBUG:  Setting route to vpn server...
DEBUG:  ip route add to 10.99.99.10/255.255.255.255 via 10.0.0.138 dev wlp18s0
```

That the host does reach the FortiGate directly is visible in the FortiGate's
own session list: it sees `10.99.99.1`, the bridge address.

While the tunnel is up this goes unnoticed (the TLS connection keeps using its
existing socket), and on a clean teardown `ipv4_restore_routes()` deletes the
route itself. It only stayed behind when the process died first — that is,
through findings 1 and 2. With those fixed the damage is gone;
`50_disconnect` and `60_persistent` now check the gateway route explicitly,
and `client_force_cleanup` in `lib/client.sh` remains as a net for hard aborts.

The wrong choice itself is in the submodule (upstream v1.20.5) and is reported
there, together with a second find on the same line: `ipv4.c:773` uses
`sprintf` to write `"!ppp0"` (6 bytes) into the 5-byte buffer that
`ipv4_get_route()` previously allocated with `strdup("ppp0")`, after freeing
the original `malloc(strlen+2)` from `ipv4.c:759`.

### 4. No clean stop with `persistent=true` — fixed

`vpnWorker::process()` did not check the stop signal in its reconnect branch.
A SIGTERM only terminated `io_loop()` and thereby triggered a rebuild instead
of stopping the process — headless, only SIGKILL was left. The branch now
checks `vpnWorker::stopRequested()`, which covers both its own flag and
`get_sig_received()` from `io_loop()`. So that a SIGTERM cannot tear anything
apart *before* `io_loop()` either, `process()` installs a minimal handler of
its own at startup and checks for an abort between the connection steps.
Covered by `60_persistent`.

### 5. `insecure_ssl` does not disable certificate validation

Contrary to what the name suggests, `insecure_ssl` in openfortivpn 1.20.5 only
affects the cipher list and TLS protocol options
(`openfortivpn/src/tunnel.c:1097-1145`). The digest whitelist is checked
independently of it. A profile with `insecure_ssl=true` and no `trusted_cert`
therefore does **not** connect. `30_cert` c) pins both behaviours down.

## 9. Known limitations

**No way to supply credentials or an OTP headless.** If the password is empty
or `always_ask_otp` is set, the process asks the GUI over the local socket and
exits after 30 s. `submitVPNMessage()` sends the message over the socket only;
nothing about it appears in the log. `40_auth` part d) pins this down. OTP
tests would need FortiToken or a FortiAuthenticator and are not covered.

**Split tunneling is mandatory.** Without split routes, openfortivpn deletes
the host's default route (`openfortivpn/src/ipv4.c:922`,
`ipv4_set_default_routes`). Provisioning therefore aborts if it cannot confirm
split tunneling on the portal, and `20_routing`/`50_disconnect` check the
default route explicitly.

**The DNS test is opt-in.** openfortivpn is compiled here with an empty
`RESOLVCONF_PATH` and therefore writes to `/etc/resolv.conf` directly
(`openfortivpn/src/ipv4.c:1126`) — on systemd-resolved systems a symlink into
the stub. `OFGUI_TEST_DNS=1` enables the test; it backs the file up first and
writes it back afterwards.

**`--main-config` does not move the certificate cache and application log.**
`tiConfMain::main_gw_cert_cache` is fixed during static initialization
(`openfortigui/ticonfmain.cpp:32`) and never updated by `setMainConfig()`;
`logMessageOutput()` opens `openfortigui.log` on the first `qDebug`
(`main.cpp:150`), so also before `setMainConfig()`. Both paths therefore depend
on `HOME` — under `sudo` on `/root`. That is why the harness always sets
`HOME` to the test home, see
[Isolation](#4-isolation-from-your-real-user-profile).

**Orphaned `/root/.openfortigui`, or access to the real home.**
`tiConfMain::main_config` is also initialized statically, before
`--main-config` takes effect. The first `tiConfMain` constructor therefore
still runs against `$HOME/.openfortigui` and calls `initMainConf()` there
(mkpath and `chmod 0750`). Depending on whether `sudo` passes `HOME` through,
this affects `/root/.openfortigui` or the real user home. Only the directory
structure is written, no configuration. `testlab clean` removes an orphaned
`/root/.openfortigui` as long as there are no profiles in it.

**Only one console client.** The QEMU serial socket accepts one connection at
a time, so `testlab console` and `provision`/`fgt` are mutually exclusive. A
complete transcript is available independently in
`$OFGUI_LAB_DIR/out/console.log` (QEMU chardev `logfile`).

## 10. Troubleshooting

| Symptom | Where to look |
|---|---|
| VM does not start | `out/qemu-stderr.log`, then `FGT_MACHINE=pc` or `FGT_NIC_MODEL=e1000` in `lab.env` |
| No login prompt | `out/console.log` and `out/provision.log`; try `testlab console` manually |
| Provisioning aborts | `out/provision.log` contains the full CLI dialog |
| SSL-VPN does not answer | `testlab fgt "diagnose debug application sslvpn -1"`, `testlab show`; check the license first |
| Tunnel does not come up | `out/<case>/client.log` (openfortivpn output) and `out/<case>/openfortigui.log` |
| Target not reachable | `ip route get 10.99.10.50`, `out/inside-http.log`, policy via `testlab show` |
| Connection timeouts in every case | leftover `/32` route to the gateway from a crash: `ip route get 10.99.99.10` must show `dev ofgt-out` |
| "An openfortiGUI instance is running" | close the GUI — otherwise the test process attaches to its local socket and dies with it (`proc/vpnprocess.cpp:47`) |
| sudo asks for a password mid-run | `sudo -v` before `testlab test`; the ticket must outlive the run |

## 11. CI

There is deliberately no hook in `.gitlab-ci.yml`: the runners have no KVM.
The JUnit report is prepared for the day a runner with virtualization support
is added.
