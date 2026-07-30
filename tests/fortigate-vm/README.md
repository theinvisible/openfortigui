# FortiGate-Testlabor für openfortiGUI

Zieht eine echte FortiGate-VM per KVM hoch, konfiguriert SSL-VPN vollständig
automatisch und testet die openfortiGUI-Binary headless dagegen.

```
tests/fortigate-vm/testlab up      # Image, Netz, VM, Provisioning  (~5 min beim ersten Mal)
tests/fortigate-vm/testlab test    # alle Testfälle                 (~3 min)
tests/fortigate-vm/testlab down    # VM und Netz weg
```

> **Voraussetzung:** eine FortiGate-VM-Lizenz. Ohne sie startet FortiOS den
> SSL-VPN-Daemon überhaupt nicht. Mit einer Eval-Lizenz läuft der Tunnel,
> die FortiGate leitet aber nichts dahinter weiter — die Testfälle prüfen
> deshalb den Tunnel selbst und lassen den Datenpfad optional.
> Details: [VM-Lizenz](#vm-lizenz).

Stand des letzten vollständigen Durchlaufs (FortiOS 7.4.12, Eval-Lizenz):
**10/10 Fälle, 161 Prüfungen grün in 165 s.** Die Abstürze, die dieses Labor
zutage gebracht hat, sind behoben; die Testfälle bewachen sie jetzt gegen
Rückfälle — siehe [Befunde](#befunde-in-openfortigui) und, vollständig und
aktuell, [`TESTS.md`](../../TESTS.md).

Zwei Fälle brauchen **keine** FortiGate: `90_gui` fährt die echte GUI auf einem
virtuellen Bildschirm, `91_sudo_rs` prüft das gebaute Paket in einem
Ubuntu-26.04-Container mit sudo-rs. `testlab test 90_gui` läuft also ohne VM.

## Voraussetzungen

| Was | Prüfung |
|---|---|
| `qemu-system-x86_64`, `qemu-img` | `apt install qemu-system-x86 qemu-utils` |
| Schreibrechte auf `/dev/kvm` | Gruppe `kvm` oder ACL — die VM läuft **unprivilegiert** |
| `socat`, `unzip`, `openssl`, `curl`, `iproute2` | Standard |
| `python3-pexpect` | `apt install python3-pexpect` |
| `ppp` | `apt install ppp` (openfortivpn braucht `/usr/sbin/pppd`) |
| `sudo` | für Bridges/Taps/netns und den VPN-Client (pppd braucht root) |
| FortiGate-KVM-Image | `FGT_VM64_KVM-*.kvm.zip` von Fortinet |
| `xvfb`, `xdotool`, `openbox`, `x11-utils`, `x11-apps` | nur `90_gui`: `apt install xvfb xdotool openbox x11-utils x11-apps` |
| `docker` | nur `91_sudo_rs`: nutzbarer Daemon, Benutzer in der Gruppe `docker` |

Die letzten zwei Zeilen verlangt nur der Fall, der sie in seiner
`# lab-requires:`-Zeile deklariert — alles andere läuft ohne sie.

libvirt wird **nicht** benötigt — das Labor spricht direkt mit QEMU über
Serial- und QMP-Unix-Sockets.

Das Fortinet-Image ist nicht frei verteilbar und liegt deshalb außerhalb des
Repos. Ohne `FGT_IMAGE_ZIP` wird automatisch das neueste
`$HOME/FGT_VM64_KVM-*.kvm.zip` genommen. Alle Laufzeit-Artefakte landen unter
`$HOME/.cache/openfortigui-testlab` (per `OFGUI_LAB_DIR` umstellbar);
`.gitignore` blockt `*.qcow2`, `*.kvm.zip` und `*.lic` zusätzlich ab.

## Konfiguration

```bash
cp tests/fortigate-vm/lab.env.example tests/fortigate-vm/lab.env
```

`lab.env` ist gitignoriert und dokumentiert jeden Schalter. Umgebungsvariablen
gewinnen gegen die Datei.

## Topologie

```
Host (Testclient)                FortiGate-VM 7.4.x            netns "ofgt-inside"
 10.99.99.1/24 ──ofgt-out(br)──── port1 10.99.99.10
      │                             Admin-HTTPS :443
      │                             SSL-VPN     :10443
      │
      └─ ppp0 10.212.134.2xx ═════ ssl.root   (Pool SSLVPN_TUNNEL_ADDR1)
                                     │  Policy ssl.root→port2, NAT aus
                                   port2 10.99.10.1/24 ──ofgt-in(br)── 10.99.10.50:8080
```

Der Host hat auf `ofgt-in` **absichtlich keine Adresse**: `10.99.10.50` ist
ausschließlich durch den Tunnel erreichbar. Ein erfolgreiches `curl` beweist
damit Tunnel, gepushte Routen und FortiGate-Policy in einem Schritt. Der
HTTP-Server im Netzwerk-Namespace protokolliert die Quell-IP, deshalb lässt
sich auch prüfen, dass die Tunnel-Pool-Adresse durchgereicht wird.

Weil Docker die `FORWARD`-Policy auf `DROP` setzt und geladenes
`br_netfilter` auch gebrückte Pakete durch `FORWARD` schickt, setzt `net-up`
zwei gezielte `ACCEPT`-Regeln für die beiden Bridges und entfernt sie bei
`net-down` wieder. Globale sysctls werden nicht angefasst.

## Isolation vom echten Benutzerprofil

Der Testclient bekommt ein eigenes Home. Ausgenutzt wird
`tiConfMain::formatPath()` (`openfortigui/ticonfmain.cpp:183`): ist
`--main-config` ein absoluter Pfad, leitet openfortiGUI das Home aus zwei
Ebenen darüber ab, statt `QDir::homePath()` zu nehmen.

```
$OFGUI_LAB_DIR/client/home/.openfortigui/main.conf   →  Home = .../client/home
                          /vpnprofiles/*.conf
                          /logs/openfortigui.log
                          /gw_cert.cache
```

**`--main-config` genügt dafür inzwischen.** Früher hingen zwei Pfade am `HOME`
statt am übergebenen Konfigurationspfad — `tiConfMain::main_gw_cert_cache` wurde
statisch initialisiert und von `setMainConfig()` nie nachgezogen, und
`logMessageOutput()` öffnete das Log beim ersten `qDebug`, also **vor**
`setMainConfig()`. Unter `sudo` landete beides in `/root/.openfortigui`. Beides
ist behoben (`applyEarlyArgs()` in `main.cpp` läuft als erste Anweisung,
`setMainConfig()` rechnet den Cache-Pfad neu); `80_env` beweist es, indem es mit
absichtlich falschem `HOME` und ohne `XDG_RUNTIME_DIR` verbindet.

Die Harness setzt `HOME` trotzdem noch (`CLIENT_EXTRA_ENV` in `lib/client.sh`),
damit ein Rückfall in die alte Abhängigkeit nicht unbemerkt bliebe, sondern in
`80_env` auffällt. `~/.openfortigui` des Benutzers wird nie gelesen oder
geschrieben.

Eine Einschränkung bleibt: `setMainConfig()` übernimmt den Pfad nur, wenn die
Datei **schon existiert** (`ticonfmain.cpp:262`) — sonst wird das Argument
stillschweigend ignoriert und alles folgt wieder `$HOME`. `client_init_home`
legt die `main.conf` deshalb an, bevor irgendetwas gestartet wird.

Die Test-`main.conf` setzt `use_system_password_store=false`. Mit aktiviertem
Passwortspeicher fragt der root-Kindprozess Key und IV über den Local-Socket
bei der GUI ab und läuft nach 30 s in einen Timeout
(`proc/vpnprocess.cpp:107`) — headless ist das nicht auflösbar. Die Profil-
Passwörter werden stattdessen mit den Default-Schlüsseln aus
`openfortigui/config.h` verschlüsselt, byte-identisch zu
`vpnHelper::Qaes128_encrypt()`:

```bash
printf '%s' "$pw" | openssl enc -aes-128-cbc -a -A \
    -K "$(printf %s yowp2IwTTRodgdWp | od -An -tx1 | tr -d ' \n')" \
    -iv "$(printf %s VoUT5n5ToogkmQU3 | od -An -tx1 | tr -d ' \n')"
```

## Kommandos

| Kommando | Wirkung |
|---|---|
| `up` | preflight → prepare → net-up → vm-up → provision → golden → vm-up |
| `test [case…]` | Testfälle; ohne Argument alle. `--keep-going` läuft trotz Fehlern weiter |
| `down` | VM stoppen, Client-Reste töten, Netz abbauen (golden bleibt) |
| `status` | Image-, Netz-, VM- und SSL-VPN-Zustand samt TLS-Info und Cert-Digest |
| `prepare [--rebuild]` | Image entpacken, golden-Overlay; `--rebuild` verwirft die Provisionierung |
| `provision` | FortiOS auf der laufenden VM konfigurieren |
| `console` | Serielle FortiGate-Konsole (Ctrl-O beendet) |
| `fgt "<cli>"` | FortiOS-Kommando ausführen, z. B. `fgt "diagnose vpn ssl list"` |
| `show` | Konfiguration dumpen (Status, SSL-VPN, Portal, Policies) |
| `cert-digest` | SHA-256 des Gateway-Zertifikats (wie `--trusted-cert`) |
| `license <datei>` | VM-Lizenz per eingebautem TFTP ins golden-Image einspielen |
| `uplink on\|off` | Internet für die FortiGate per NAT über den Host (Lizenzaktivierung) |
| `clean` | Arbeitsverzeichnis inklusive Images löschen |

### Image-Schichten

```
base-<version>.qcow2     entpackt, read-only
  └─ golden-<version>.qcow2   provisioniert (Erstsetup landet hier)
       └─ run/disk.qcow2      Wegwerf-Overlay, pro vm-up neu
```

Jeder `vm-up` startet damit im identischen Zustand, und das Erstsetup läuft
nur einmal. `prepare --rebuild` erzwingt eine Neuprovisionierung.

## Testfälle

| Fall | Prüft | Braucht | Prüfungen |
|---|---|---|---|
| `10_connect` | Verbindung mit gepinntem Zertifikat: `Tunnel is up and running.`, ppp-Interface mit Pool-Adresse, Anwendungslog ohne Critical/Fatal | vm | 9 |
| `20_routing` | Split-Route auf das Innennetz, **Default-Route unverändert**, Byte-Zähler aus `/proc/net/dev`; optional Datenpfad und DNS-Push | vm | 5 |
| `30_cert` | unbekanntes Zertifikat wird abgelehnt und der Digest so geloggt, wie `MainWindow` ihn per Regex extrahiert; falscher Digest; `insecure_ssl` allein hebt die Prüfung **nicht** auf; `gw_cert.cache`; `min_tls`/`seclevel1`/TLS 1.3; SNI; Passphrase-Prompt für einen verschlüsselten Client-Key | vm | 19 |
| `40_auth` | falsches Passwort, unbekannter Benutzer, fehlendes Profil, fehlendes Passwort ohne GUI, Exit-Code des Fehlerpfads; Anmeldung per SVPNCOOKIE | vm | 17 |
| `50_disconnect` | SIGTERM: sauberer Abbau, ppp weg, Routen und Adressen wie vorher, Split-Route abgebaut, keine Prozessreste, Exit-Code | vm | 15 |
| `60_persistent` | serverseitig gekappter Tunnel (`execute vpn sslvpn del-tunnel <index>`) wird per `persistent=true` neu aufgebaut; SIGTERM stoppt trotzdem | vm | 15 |
| `70_guistop` | von der GUI angestoßener Stop: `ACTION_STOP` über den lokalen Socket, und eine verschwindende GUI. Nutzt `mock_gui.py` | vm | 22 |
| `80_env` | Unabhängigkeit von der geerbten Umgebung: falsches `HOME`, kein `XDG_RUNTIME_DIR`, das Kind erreicht die GUI über `--api-socket`, nichts landet in `/root` | vm | 10 |
| `90_gui` | die echte GUI auf 1280×800 (Xvfb): Einstellungen, Profil- und Gruppeneditor passen, haben keine große Mindestgröße und lassen sich verkleinern; Enter speichert, Escape verwirft; bei verschlüsseltem Client-Key erscheint der **Passphrase**-Dialog (nicht der OTP-Dialog) und der Tunnel kommt danach hoch | gui | 25 |
| `91_sudo_rs` | das gebaute `.deb` in einem Ubuntu-26.04-Container mit sudo-rs: Abhängigkeiten auflösbar, sudoers-Datei von `visudo-rs` geparst, Wildcard `--start-vpn *` wirksam und nichts darüber hinaus, `-E`-Verhalten beider sudo-Implementierungen, echter Tunnel durch sudo-rs | vm docker | 24 |

Die Spalte „Braucht" ist die `# lab-requires:`-Zeile im Kopf jedes Falls.
`testlab test` verlangt nur, was die **ausgewählten** Fälle deklarieren — fehlt
die Zeile, gilt `vm`.

Zwei Prüfungen sind opt-in:

| Schalter | Wirkung |
|---|---|
| `OFGUI_TEST_DATAPATH=1` | Erreichbarkeit des Ziels hinter der FortiGate und Quell-IP-Prüfung. Braucht eine FortiGate, die tatsächlich weiterleitet (siehe [VM-Lizenz](#vm-lizenz)). |
| `OFGUI_TEST_DNS=1` | DNS-Push. Schreibt über openfortivpn direkt in `/etc/resolv.conf`; der Testfall sichert und restauriert die Datei. |

Ergebnisse: `ok`/`FAIL`/`skip` auf stdout, vollständige Logs unter
`$OFGUI_LAB_DIR/out/<fall>/`, Sammelbericht `$OFGUI_LAB_DIR/out/junit.xml`.
Jeder Fall läuft auch einzeln:

```bash
tests/fortigate-vm/cases/20_routing.sh
```

### Was die Profile brauchen

`client_write_profile` setzt zwei Optionen abweichend von den
openfortiGUI-Defaults, beide notwendig:

- **`pppd_accept_remote=true`** — openfortiGUI übergibt pppd `:169.254.2.1`
  als Remote-Adresse, die FortiGate schlägt aber ihre eigene vor (hier die
  port1-IP). Ohne diese Option fehlt `ipcp-accept-remote`
  (`vpnworker.cpp:276`) und pppd bricht mit *„Peer refused to agree to his IP
  address"* ab. Das ist keine Laborbesonderheit, sondern gilt gegen jede
  FortiGate, die eine eigene Remote-Adresse vorgibt.
- **`debug=true`** — sonst fehlen im Log die Zeilen, an denen die
  Assertions hängen.

### Was die FortiGate im Labor braucht

Drei Einstellungen, die das Provisioning setzt und die nicht offensichtlich
sind:

- **`web-mode enable`** im Portal. openfortivpns POST auf
  `/remote/logincheck` wertet FortiOS als `tunneltype="ssl-web"`. Mit
  `web-mode disable` wird der Login mit
  `reason="sslvpn_login_permission_denied"` abgelehnt, obwohl Benutzer,
  Gruppe, Auth-Rule und Passwort korrekt sind — eine irreführende Fehlersuche.
- **`login-block-time 0`** und `login-attempt-limit 10`. Default ist
  „nach 2 Fehlversuchen 60 s Sperre für die Quell-IP". Die Fehlerfall-Tests
  erzeugen genau solche Fehlversuche und würden damit alle folgenden Fälle mit
  `Empty cookie` scheitern lassen.
- **`split-tunneling enable`** plus `split-tunneling-routing-address`. Ohne
  gesetzte Split-Routen löscht openfortivpn die Default-Route des Testhosts
  (`openfortivpn/src/ipv4.c:922`). Das Provisioning bricht ab, wenn es das
  nicht bestätigen kann.

## VM-Lizenz

**Eine unlizenzierte FortiGate-VM startet den SSL-VPN-Daemon nicht.** Am
Image FortiOS 7.4.12 build2902 gemessen.

Mit eingespielter Eval-Lizenz läuft `sslvpnd`, lauscht auf 10443, und Login,
PPP-Aushandlung und Routen-Push funktionieren vollständig. Die FortiGate
**leitet aber nichts weiter**: `get vpn ssl monitor` zeigt die Sitzung mit
`I/O Bytes 0/0`, in der Routing-Tabelle fehlt die Host-Route zur Tunnel-IP,
und ein Flow-Trace auf der FortiGate sieht kein einziges Paket. `get system
status` meldet weiterhin `License Status: No License`, Seriennummer
`FGVM00UNLICENSED`, `VM Resources: 1 CPU/0 allowed`; die API sagt
`forticare.status: pending`, `registration_status: registrable`. Die VM will
also noch bei FortiCare registriert werden, um voll nutzbar zu sein.

Für den Zweck dieses Labors — den openfortiGUI-Client testen — genügt das:
der Tunnel steht, alle client-seitigen Prüfungen laufen. Die Erreichbarkeit
hinter der FortiGate ist deshalb hinter `OFGUI_TEST_DATAPATH=1` optional.

Eine Nebenwirkung ist zu kennen: weil keine Nutzdaten fließen, laufen pppds
LCP-Echos ins Leere und der Tunnel stirbt nach rund zwei Minuten von selbst
(*„Serial link appears to be disconnected"*, `lcp-echo-interval 30` /
`lcp-echo-failure 4` aus `/etc/ppp/options`). Alle Testfälle sind kürzer, aber
für eigene Experimente ist das die Zeitgrenze.

FortiOS sagt es selbst, über `/api/v2/monitor/license/status`:

```json
"vm": { "valid": false, "status": "vm_invalid", "license_source": "local",
        "license_platform_name": "FGVMEV",
        "cpu_used": 1, "cpu_max": 1,
        "mem_used": 2089811968, "mem_max": 2147483648 },
"forticare": { "registration_status": "unregistrable",
               "registration_supported": false }
```

Wichtig an diesen Zahlen: `cpu 1/1` und `mem 1993/2048 MB` liegen *innerhalb*
der Eval-Grenzen. Die Crashlog-Zeile
`from=license msg=VM resource exceeds license limit / CPU:1/1, MEM:1993/2048`
feuert bereits bei Gleichstand und ist irreführend — die Ressourcen sind
nicht die Ursache. `registration_supported: false` bedeutet außerdem, dass
sich die VM auch mit Internetzugang keine Trial-Lizenz selbst holen kann.

Weitere Belege:

- Die Konfiguration wird vollständig angenommen — `show full-configuration
  vpn ssl settings` zeigt `set status enable`, `set port 10443`,
  `set servercert "Fortinet_Factory"`, Portal mit `split-tunneling enable`.
- Trotzdem kein Listener: Port 10443 antwortet `connection refused`, in
  `diagnose sys tcpsock` gibt es keinen sslvpn-Socket, `diagnose vpn ssl
  list` liefert `client connect failed 111: Connection refused`. Im gesamten
  Crashlog (323 Zeilen) kommt `sslvpn` **nicht** vor — der Daemon crasht
  nicht, er wird nie gestartet. `diagnose debug application sslvpn -1`
  erzeugt beim Neuschalten des Dienstes keine einzige Zeile.
- Nach dem Login leitet die GUI direkt auf `/system/vm/license` um, und alle
  `/api/v2/monitor/...`-Endpunkte außer `license/status` antworten mit
  `401 Unauthorized`. Die Box ist hinter der Lizenzseite gesperrt.
- `ssl.root` existiert und ist `status: up`, das Interface-Limit des
  Eval-Modus blockt also nichts.
- Admin-HTTPS läuft einwandfrei, port1/port2 sind up, Ping und TLS
  funktionieren.

Empirisch ausgeschlossen wurden: SSL-VPN-Port (10443 und 443 nach Verschieben
des Admin-Ports), `source-interface`, `default-portal`,
`authentication-rule`, `web-mode` an/aus, Dienst-Neuaktivierung, Reboot,
Arbeitsspeicher (1024 und 2048 MB), Maschinentyp (`q35` und `pc`),
NIC-Modell (`virtio-net-pci` und `e1000`) sowie Log-Disk (fehlend und
`Available`).

Abhilfe: kostenlose 15-Tage-Eval-Lizenz auf
[support.fortinet.com](https://support.fortinet.com) für die Seriennummer der
VM anfordern (`testlab fgt "get system status" | grep Serial`), dann

```bash
testlab license /pfad/zu/FGVM....lic
```

Das fährt die VM dafür automatisch im Provision-Modus, spielt die Lizenz über
einen eingebauten Mini-TFTP-Server ein und schreibt sie damit ins
golden-Image — sonst wäre sie beim nächsten `vm-up` wieder weg. Alternativ
`FGT_LICENSE` in `lab.env` setzen, dann erledigt `testlab up` das mit.

Die Aktivierung braucht FortiCare-Kontakt, das Labor-Netz ist aber isoliert.
`testlab license` schaltet deshalb automatisch einen NAT-Uplink über den Host
frei (`net.ipv4.ip_forward` plus MASQUERADE für `10.99.99.0/24`, dazu
Default-Route und DNS auf der FortiGate) und `testlab down` räumt ihn wieder
ab. Verifiziert: `execute ping 1.1.1.1` auf der FortiGate antwortet damit.
Manuell steuerbar über `testlab uplink on|off`, DNS über `LAB_DNS_SERVER`.

**Krypto ist kein Problem.** Der TLS-Handshake gegen die Eval-VM klappt mit
openfortivpns Default-Cipherliste (`HIGH:!aNULL:!kRSA:!PSK:!SRP:!MD5:!RC4`) —
gemessen gegen den Admin-Port. Die befürchtete Low-Encryption-Einschränkung
tritt hier nicht auf, `seclevel1`/`min_tls`-Workarounds sind nicht nötig.
Falls doch einmal eine explizite Cipherliste gebraucht würde: openfortiGUI
setzt `vpn_config.cipher_list` nie (`proc/vpnworker.cpp`), das wäre also
nicht konfigurierbar.

**Ressourcengrenzen.** Der Eval-Modus erlaubt 1 vCPU und 2 GB, deshalb
`-smp 1 -m 2048`. Außerdem ist `vpn.ssl.web.portal` auf einen Eintrag
begrenzt (`Too many entries ... vdom-max = 1`), ein eigenes Portal lässt sich
also nicht anlegen. Das Provisioning erkennt das und konfiguriert stattdessen
das vorhandene Portal (`full-access`) um.

## Befunde in openfortiGUI

> Dieser Abschnitt hält den Stand fest, in dem die Befunde gefunden wurden. Die
> **vollständige und aktuelle** Liste inklusive der Fixes steht in
> [`TESTS.md`, Abschnitt 8](../../TESTS.md) — dort auch die Befunde 6 bis 10
> (Umgebungsabhängigkeit, Cookie-Pfad, `sudo -E`, Paketierung,
> `--main-config`-Rückfall), die erst später dazukamen.

Alle drei sind reproduzierbar und mit gdb-Backtrace belegt.

### 1. Absturz bei jedem Verbindungsabbau (SIGSEGV, Exit-Code 139)

```
#0  vpnWorker::end ()            vpnworker.cpp:767   ptr_tunnel->on_ppp_if_down(ptr_tunnel);
#1  vpnProcess::closeProcess ()  vpnprocess.cpp:77
#2  Queued-Signal-Auslieferung im Main-Thread
```

`closeProcess()` hängt als *queued* Slot an `vpnWorker::finished()` und läuft
daher im Main-Thread, nachdem `vpnWorker::process()` bereits zurückgekehrt
ist. `vpnWorker::end()` dereferenziert dort ungeprüft `ptr_tunnel` — einen
Zeiger auf die **stack-lokale** `struct tunnel` aus `process()`
(`vpnworker.cpp:657`), deren Stack-Frame nicht mehr existiert. General
Protection Fault bei jedem Abbau, im Fehlerpfad (Auth-Fehler) wie beim
regulären SIGTERM. Da die GUI über `ACTION_STOP` denselben `closeProcess()`
auslöst, sehr wahrscheinlich auch im GUI-Betrieb.
Geprüft von `40_auth` a) und `50_disconnect`.

### 2. Heap-Korruption beim ersten Reconnect (SIGABRT, Exit-Code 134)

```
#7  malloc_printerr ()             malloc.c:5341
#9  ipv4_set_split_routes ()       openfortivpn/src/ipv4.c:902   free(route_iface(route));
#10 ipv4_set_tunnel_routes ()      openfortivpn/src/ipv4.c:1009
#11 on_ppp_if_up ()                proc/vpnworker.cpp:123
#12 if_config ()                   openfortivpn/src/io.c:585
```

Der Persistent-Zweig gibt `tunnel.ipv4.split_rt` frei und nullt den Pointer
(`vpnworker.cpp:748-752`), **setzt aber `tunnel.ipv4.split_routes` nicht
zurück**. Beim `goto start_tunnel` vergibt `ipv4_add_split_vpn_route` den
nächsten Index über den stehengebliebenen Zähler
(`split_rt[split_routes++]`, `ipv4.c:868`), sodass Index 0 uninitialisierter
malloc-Speicher bleibt. `ipv4_set_split_routes` läuft anschließend ab Index 0
und ruft `free()` auf einen Zufallspointer — glibc bricht ab.

Reproduzierbar bei jedem Reconnect eines Profils mit `persistent=true`,
sobald das Portal Split-Routen pusht. Ein `split_routes = 0` neben dem
`free()` würde es beheben. Geprüft von `60_persistent`.

### 3. Verwaiste Gateway-Route nach einem Absturz

`ipv4_protect_tunnel_route()` (`openfortivpn/src/ipv4.c:729`) legt eine
`/32`-Route auf das VPN-Gateway an. Liegt das Gateway direkt an einem
sekundären Interface, ermittelt `ipv4_get_route()` dafür den **falschen**
Next-Hop — den Default-Gateway statt der direkten Verbindung:

```
DEBUG:  ip route show to 10.99.99.10/255.255.255.255 dev !ppp0
DEBUG:  Setting route to vpn server...
DEBUG:  ip route add to 10.99.99.10/255.255.255.255 via 10.0.0.138 dev wlp18s0
```

Solange der Tunnel steht, fällt das nicht auf (die TLS-Verbindung nutzt ihren
bestehenden Socket). Stürzt der Prozess aber ab — siehe Befund 1 und 2 —,
bleibt die Route stehen und macht das Gateway unerreichbar; jeder weitere
Verbindungsversuch endet in `connect: connection timed out`.
`client_force_cleanup` in `lib/client.sh` räumt sie deshalb weg.

### 4. Kein sauberer Stop bei `persistent=true`

`vpnWorker::process()` prüft im Reconnect-Zweig (`vpnworker.cpp:754`)
`sig_received` nicht. Ein SIGTERM beendet nur `io_loop()` und löst damit einen
Wiederaufbau aus statt den Prozess zu stoppen — headless bleibt nur SIGKILL.
`60_persistent` prüft das explizit, damit eine Änderung auffällt. Bei aktivem
Split-Tunneling ist es unkritisch: die Routen hängen am ppp-Interface und
verschwinden mit ihm.

### 5. `insecure_ssl` hebt die Zertifikatsprüfung nicht auf

Entgegen dem, was der Name nahelegt, wirkt `insecure_ssl` in openfortivpn
1.20.5 nur auf Cipherliste und TLS-Protokolloptionen
(`openfortivpn/src/tunnel.c:1097-1145`). Die Digest-Whitelist wird davon
unabhängig geprüft. Ein Profil mit `insecure_ssl=true` und ohne
`trusted_cert` verbindet sich also **nicht**. `30_cert` c) hält beides fest.

## Bekannte Einschränkungen

**Kein Weg, headless Zugangsdaten oder OTP nachzureichen.** Ist das Passwort
leer oder `always_ask_otp` gesetzt, fragt der Prozess über den Local-Socket
bei der GUI an und beendet sich nach 30 s. `submitVPNMessage()` schickt die
Meldung ausschließlich über den Socket, im Log steht dazu nichts.
`40_auth` Teil d) hält das fest. OTP-Tests bräuchten FortiToken bzw. einen
FortiAuthenticator und sind nicht abgedeckt.

**Split-Tunneling ist Pflicht.** Ohne gesetzte Split-Routen löscht
openfortivpn die Default-Route des Hosts
(`openfortivpn/src/ipv4.c:922`, `ipv4_set_default_routes`). Das Provisioning
bricht deshalb ab, wenn es Split-Tunneling im Portal nicht bestätigen kann,
und `20_routing`/`50_disconnect` prüfen die Default-Route explizit.

**DNS-Test ist opt-in.** openfortivpn ist hier mit leerem `RESOLVCONF_PATH`
kompiliert und schreibt daher direkt in `/etc/resolv.conf`
(`openfortivpn/src/ipv4.c:1126`) — auf systemd-resolved-Systemen ein Symlink
in den Stub. `OFGUI_TEST_DNS=1` schaltet den Test ein; er sichert die Datei
vorher und schreibt sie danach zurück.

**`--main-config` verlegt Zertifikats-Cache und Anwendungslog nicht mit.**
`tiConfMain::main_gw_cert_cache` wird bei der statischen Initialisierung
festgelegt (`openfortigui/ticonfmain.cpp:32`) und von `setMainConfig()` nie
nachgezogen; `logMessageOutput()` öffnet `openfortigui.log` beim ersten
`qDebug` (`main.cpp:150`), also ebenfalls vor `setMainConfig()`. Beide Pfade
hängen damit an `HOME` — unter `sudo` an `/root`. Die Harness setzt `HOME`
deshalb generell auf das Test-Home, siehe
[Isolation](#isolation-vom-echten-benutzerprofil).

**Verwaistes `/root/.openfortigui` bzw. Zugriff auf das echte Home.**
`tiConfMain::main_config` wird ebenfalls statisch initialisiert, bevor
`--main-config` greift. Der erste `tiConfMain`-Konstruktor läuft daher noch
gegen `$HOME/.openfortigui` und ruft dort `initMainConf()` auf (mkpath und
`chmod 0750`). Je nachdem, ob `sudo` `HOME` durchreicht, betrifft das
`/root/.openfortigui` oder das echte Benutzer-Home. Geschrieben wird nur die
Verzeichnisstruktur, keine Konfiguration. `testlab clean` entfernt ein
verwaistes `/root/.openfortigui`, sofern dort keine Profile liegen.

**Nur ein Konsolen-Client.** Der QEMU-Serial-Socket nimmt eine Verbindung zur
Zeit. `testlab console` und `provision`/`fgt` schließen sich also aus. Ein
vollständiger Mitschnitt liegt unabhängig davon in
`$OFGUI_LAB_DIR/out/console.log` (QEMU-Chardev-`logfile`).

## Fehlersuche

| Symptom | Ansatz |
|---|---|
| VM startet nicht | `out/qemu-stderr.log`, dann `FGT_MACHINE=pc` bzw. `FGT_NIC_MODEL=e1000` in `lab.env` |
| Kein Login-Prompt | `out/console.log` und `out/provision.log`; manuell `testlab console` |
| Provisioning bricht ab | `out/provision.log` enthält den vollständigen CLI-Dialog |
| SSL-VPN antwortet nicht | `testlab fgt "diagnose debug application sslvpn -1"`, `testlab show` |
| Tunnel kommt nicht hoch | `out/<fall>/client.log` (openfortivpn-Ausgabe) und `out/<fall>/openfortigui.log` |
| Ziel nicht erreichbar | `ip route get 10.99.10.50`, `out/inside-http.log`, Policy per `testlab show` |
| „Es läuft eine openfortiGUI-Instanz" | GUI beenden — der Testprozess hängt sich sonst an deren Local-Socket und stirbt mit ihr (`proc/vpnprocess.cpp:47`) |

## CI

Es gibt bewusst keine Anbindung in `.gitlab-ci.yml`: die Runner haben kein
KVM. Der JUnit-Bericht ist für den Fall vorbereitet, dass ein Runner mit
Virtualisierung dazukommt.
