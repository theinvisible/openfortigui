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

#include "vpnworker.h"

extern "C"  {
#include "openfortivpn/src/config.h"
#include "openfortivpn/src/log.h"
#include "openfortivpn/src/tunnel.h"
#include "openfortivpn/src/http.h"
#include "openfortivpn/src/userinput.h"

#ifndef HAVE_X509_CHECK_HOST
#include "openssl_hostname_validation.h"
#endif

#include <openssl/err.h>
#ifdef OPENSSL_ENGINE
#include <openssl/engine.h>
#endif
#include <openssl/ui.h>
#include <openssl/x509v3.h>
#if HAVE_SYSTEMD
#include <systemd/sd-daemon.h>
#endif

#include <unistd.h>
#include <arpa/inet.h>
#include <fcntl.h>
#include <ifaddrs.h>
#include <net/if.h>
#include <netdb.h>
#if HAVE_PTY_H
#include <pty.h>
#elif HAVE_UTIL_H
#include <util.h>
#endif
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/wait.h>
#include <sys/ioctl.h>
#include <termios.h>
#if HAVE_LIBUTIL_H
#include <libutil.h>
#endif

#include <errno.h>
#include <signal.h>
#include <string.h>
}

#include <QDebug>
#include <QCoreApplication>
#include <QHostInfo>

// -------------------
// Included from tunnel.c
// -------------------

struct ofv_varr {
    unsigned int cap;		// current capacity
    unsigned int off;		// next slot to write, always < max(cap - 1, 1)
    const char **data;	// NULL terminated
};

static int ofv_append_varr(struct ofv_varr *p, const char *x)
{
    if (p->off + 1 >= p->cap) {
        const char **ndata;
        unsigned int ncap = (p->off + 1) * 2;

        if (p->off + 1 >= ncap) {
            log_error("%s: ncap exceeded\n", __func__);
            return 1;
        };
        ndata = (const char**) realloc(p->data, ncap * sizeof(const char *));
        if (ndata) {
            p->data = ndata;
            p->cap = ncap;
        } else {
            log_error("realloc: %s\n", strerror(errno));
            return 1;
        }
    }
    if (p->data == NULL) {
        log_error("%s: NULL data\n", __func__);
        return 1;
    }
    if (p->off + 1 >= p->cap) {
        log_error("%s: cap exceeded in p\n", __func__);
        return 1;
    }
    p->data[p->off] = x;
    p->data[++p->off] = NULL;
    return 0;
}


static int on_ppp_if_up(struct tunnel *tunnel)
{
    log_info("Interface %s is UP.\n", tunnel->ppp_iface);

    if (tunnel->config->set_routes) {
        int ret;

        log_info("Setting new routes...\n");

        ret = ipv4_set_tunnel_routes(tunnel);

        if (ret != 0)
            log_warn("Adding route table is incomplete. Please check route table.\n");
    }

    if (tunnel->config->set_dns) {
        log_info("Adding VPN nameservers...\n");
        ipv4_add_nameservers_to_resolv_conf(tunnel);
    }

    log_info("Tunnel is up and running.\n");

#if HAVE_SYSTEMD
    sd_notify(0, "READY=1");
#endif

    return 0;
}

static int on_ppp_if_down(struct tunnel *tunnel)
{
#if HAVE_SYSTEMD
    sd_notify(0, "STOPPING=1");
#endif

    log_info("Setting %s interface down.\n", tunnel->ppp_iface);

    if (tunnel->config->set_routes) {
        log_info("Restoring routes...\n");
        ipv4_restore_routes(tunnel);
    }

    if (tunnel->config->set_dns) {
        log_info("Removing VPN nameservers...\n");
        ipv4_del_nameservers_from_resolv_conf(tunnel);
    }

    return 0;
}

static int pppd_run(struct tunnel *tunnel)
{
    pid_t pid;
    int amaster;
    int slave_stderr;

#ifdef HAVE_STRUCT_TERMIOS
    struct termios termp;
    termp.c_cflag = B9600;
    termp.c_cc[VTIME] = 0;
    termp.c_cc[VMIN] = 1;
#endif

    static const char ppp_path[] = PPP_PATH;

    if (access(ppp_path, F_OK) != 0) {
        log_error("%s: %s.\n", ppp_path, strerror(errno));
        return 1;
    }
    log_debug("ppp_path: %s\n", ppp_path);

    slave_stderr = dup(STDERR_FILENO);

    if (slave_stderr < 0) {
        log_error("slave stderr: %s\n", strerror(errno));
        return 1;
    }

#ifdef HAVE_STRUCT_TERMIOS
    pid = forkpty(&amaster, NULL, &termp, NULL);
#else
    pid = forkpty(&amaster, NULL, NULL, NULL);
#endif

    if (pid == 0) { // child process

        struct ofv_varr pppd_args = { 0, 0, NULL };

        dup2(slave_stderr, STDERR_FILENO);
        if (close(slave_stderr))
            log_warn("Could not close slave stderr (%s).\n", strerror(errno));

#if HAVE_USR_SBIN_PPP
        /*
         * assume there is a default configuration to start.
         * Support for taking options from the command line
         * e.g. the name of the configuration or options
         * to send interactively to ppp will be added later
         */
        static const char *const v[] = {
            ppp_path,
            "-direct"
        };
        for (unsigned int i = 0; i < ARRAY_SIZE(v); i++)
            if (ofv_append_varr(&pppd_args, v[i])) {
                free(pppd_args.data);
                return 1;
            }
#endif
#if HAVE_USR_SBIN_PPPD
        if (tunnel->config->pppd_call) {
            if (ofv_append_varr(&pppd_args, ppp_path)) {
                free(pppd_args.data);
                return 1;
            }
            if (ofv_append_varr(&pppd_args, "call")) {
                free(pppd_args.data);
                return 1;
            }
            if (ofv_append_varr(&pppd_args, tunnel->config->pppd_call)) {
                free(pppd_args.data);
                return 1;
            }
        } else {
            static const char *const v[] = {
                ppp_path,
                "230400", // speed
                ":169.254.2.1", // <local_IP_address>:<remote_IP_address>
                "noipdefault",
                "noaccomp",
                "noauth",
                "default-asyncmap",
                "nopcomp",
                "receive-all",
                "nodefaultroute",
                "nodetach",
                "lcp-max-configure", "40",
                "mru", "1354"
            };
            for (unsigned int i = 0; i < ARRAY_SIZE(v); i++)
                if (ofv_append_varr(&pppd_args, v[i])) {
                    free(pppd_args.data);
                    return 1;
                }
        }
        if (tunnel->config->pppd_use_peerdns)
            if (ofv_append_varr(&pppd_args, "usepeerdns")) {
                free(pppd_args.data);
                return 1;
            }
        if (tunnel->config->pppd_log) {
            if (ofv_append_varr(&pppd_args, "debug")) {
                free(pppd_args.data);
                return 1;
            }
            if (ofv_append_varr(&pppd_args, "logfile")) {
                free(pppd_args.data);
                return 1;
            }
            if (ofv_append_varr(&pppd_args, tunnel->config->pppd_log)) {
                free(pppd_args.data);
                return 1;
            }
        } else {
            /*
             * pppd defaults to logging to fd=1, clobbering the
             * actual PPP data
             */
            if (ofv_append_varr(&pppd_args, "logfd")) {
                free(pppd_args.data);
                return 1;
            }
            if (ofv_append_varr(&pppd_args, "2")) {
                free(pppd_args.data);
                return 1;
            }
        }
        if (tunnel->config->pppd_plugin) {
            if (ofv_append_varr(&pppd_args, "plugin")) {
                free(pppd_args.data);
                return 1;
            }
            if (ofv_append_varr(&pppd_args, tunnel->config->pppd_plugin)) {
                free(pppd_args.data);
                return 1;
            }
        }
        if (tunnel->config->pppd_ipparam) {
            if (ofv_append_varr(&pppd_args, "ipparam")) {
                free(pppd_args.data);
                return 1;
            }
            if (ofv_append_varr(&pppd_args, tunnel->config->pppd_ipparam)) {
                free(pppd_args.data);
                return 1;
            }
        }
        if (tunnel->config->pppd_ifname) {
            if (ofv_append_varr(&pppd_args, "ifname")) {
                free(pppd_args.data);
                return 1;
            }
            if (ofv_append_varr(&pppd_args, tunnel->config->pppd_ifname)) {
                free(pppd_args.data);
                return 1;
            }
        }
#endif
#if HAVE_USR_SBIN_PPP
        if (tunnel->config->ppp_system) {
            if (ofv_append_varr(&pppd_args, tunnel->config->ppp_system)) {
                free(pppd_args.data);
                return 1;
            }
        }
#endif

        if (close(tunnel->ssl_socket))
            log_warn("Could not close ssl socket (%s).\n", strerror(errno));
        tunnel->ssl_socket = -1;
        execv(pppd_args.data[0], (char *const *)pppd_args.data);
        free(pppd_args.data);

        fprintf(stderr, "execv: %s\n", strerror(errno));
        _exit(EXIT_FAILURE);
    } else {
        if (close(slave_stderr))
            log_error("Could not close slave stderr (%s).\n",
                      strerror(errno));
        if (pid == -1) {
            log_error("forkpty: %s\n", strerror(errno));
            return 1;
        }
    }

    // Set non-blocking
    int flags = fcntl(amaster, F_GETFL, 0);

    if (flags == -1)
        flags = 0;
    if (fcntl(amaster, F_SETFL, flags | O_NONBLOCK) == -1) {
        log_error("fcntl: %s\n", strerror(errno));
        return 1;
    }

    tunnel->pppd_pid = pid;
    tunnel->pppd_pty = amaster;

    return 0;
}


static const char * const ppp_message[] = {
#if HAVE_USR_SBIN_PPPD // pppd(8) - https://ppp.samba.org/pppd.html
    "Has detached, or otherwise the connection was successfully established and terminated at the peer's request.",
    "An immediately fatal error of some kind occurred, such as an essential system call failing, or running out of virtual memory.",
    "An error was detected in processing the options given, such as two mutually exclusive options being used.",
    "Is not setuid-root and the invoking user is not root.",
    "The kernel does not support PPP, for example, the PPP kernel driver is not included or cannot be loaded.",
    "Terminated because it was sent a SIGINT, SIGTERM or SIGHUP signal.",
    "The serial port could not be locked.",
    "The serial port could not be opened.",
    "The connect script failed (returned a non-zero exit status).",
    "The command specified as the argument to the pty option could not be run.",
    "The PPP negotiation failed, that is, it didn't reach the point where at least one network protocol (e.g. IP) was running.",
    "The peer system failed (or refused) to authenticate itself.",
    "The link was established successfully and terminated because it was idle.",
    "The link was established successfully and terminated because the connect time limit was reached.",
    "Callback was negotiated and an incoming call should arrive shortly.",
    "The link was terminated because the peer is not responding to echo requests.",
    "The link was terminated by the modem hanging up.",
    "The PPP negotiation failed because serial loopback was detected.",
    "The init script failed (returned a non-zero exit status).",
    "We failed to authenticate ourselves to the peer."
#else // sysexits(3) - https://www.freebsd.org/cgi/man.cgi?query=sysexits
    // EX_NORMAL = EX_OK (0)
    "Successful exit.",
    NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, // 1-9
    NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL,     // 10-19
    NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, // 20-29
    NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, // 30-39
    NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, // 40-49
    NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, // 50-59
    NULL, NULL, NULL, NULL, // 60-63
    // EX_USAGE (64)
    "The command was used incorrectly, e.g., with the wrong number of arguments, a bad flag, a bad syntax in a parameter, or whatever.",
    NULL, NULL, NULL, NULL, // 65-68
    // EX_UNAVAILABLE (69)
    "A service is unavailable. This can occur if a support program or file does not exist.",
    // EX_ERRDEAD = EX_SOFTWARE (70)
    "An internal software error has been detected.",
    NULL, NULL, NULL, NULL, NULL, NULL, NULL, // 71-77
    // EX_CONFIG (78)
    "Something was found in an unconfigured or misconfigured state.",
    NULL, // 79
    NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, // 80-89
    NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, // 90-98
    NULL // EX_TERMINATE (99), PPP internal pseudo-code
#endif
};

static int pppd_terminate(struct tunnel *tunnel)
{
    if (close(tunnel->pppd_pty))
        log_warn("Could not close pppd pty (%s).\n", strerror(errno));

    log_debug("Waiting for %s to exit...\n", PPP_DAEMON);

    int status;

    /*
     * Errors outside of the PPP process are returned as negative integers.
     */
    if (waitpid(tunnel->pppd_pid, &status, 0) == -1) {
        log_error("waitpid: %s\n", strerror(errno));
        return -1;
    }

    /*
     * Errors in the PPP process are returned as positive integers.
     */
    if (WIFEXITED(status)) {
        int exit_status = WEXITSTATUS(status);

        /*
         * PPP exit status codes are positive integers. The way we interpret
         * their value is not straightforward:
         * - in the case of "normal" exit, the PPP process may return 0,
         *   but also a strictly positive integer such as 16 in the case of
         *   pppd,
         * - in the case of failure, the PPP process will return a strictly
         *   positive integer.
         * For now we process PPP exit status codes as follows:
         * - exit status codes synonym of success are logged and then
         *   translated to 0 before they are returned to the calling function,
         * - other exit status codes are considered synonyms of failure and
         *   returned to the calling function as is.
         */
        log_debug("waitpid: %s exit status code %d\n",
                  PPP_DAEMON, exit_status);
        if (exit_status >= ARRAY_SIZE(ppp_message) || exit_status < 0) {
            log_error("%s: Returned an unknown exit status code: %d\n",
                      PPP_DAEMON, exit_status);
        } else {
            switch (exit_status) {
            /*
             * PPP exit status codes considered as success
             */
            case 0:
                log_debug("%s: %s\n",
                          PPP_DAEMON, ppp_message[exit_status]);
                break;
#if HAVE_USR_SBIN_PPPD
            case 16: // emitted by Ctrl+C or "kill -15"
                if (get_sig_received() == SIGINT
                    || get_sig_received() == SIGTERM) {
                    log_info("%s: %s\n",
                             PPP_DAEMON, ppp_message[exit_status]);
                    exit_status = 0;
                    break;
                }
#endif
            /*
             * PPP exit status codes considered as failure
             */
            default:
                if (ppp_message[exit_status])
                    log_error("%s: %s\n",
                              PPP_DAEMON, ppp_message[exit_status]);
                else
                    log_error("%s: Returned an unexpected exit status code: %d\n",
                              PPP_DAEMON, exit_status);
                break;
            }
        }
        return exit_status;
    } else if (WIFSIGNALED(status)) {
        int signal_number = WTERMSIG(status);

        /*
         * For now we do not consider interruption of the PPP process by
         * a signal as a failure. Should we?
         */
        log_debug("waitpid: %s terminated by signal %d\n",
                  PPP_DAEMON, signal_number);
        log_error("%s: terminated by signal: %s\n",
                  PPP_DAEMON, strsignal(signal_number));
    }

    return 0;
}

static int get_gateway_host_ip(struct tunnel *tunnel)
{
    QHostInfo hInfo = QHostInfo::fromName(tunnel->config->gateway_host);
    if(hInfo.error() != QHostInfo::NoError || hInfo.addresses().isEmpty())
    {
        qWarning() << "DNS lookup error: " << hInfo.errorString();
        return 1;
    }

    inet_aton(hInfo.addresses().first().toString().toStdString().c_str(), &tunnel->config->gateway_ip);
    setenv("VPN_GATEWAY", inet_ntoa(tunnel->config->gateway_ip), 0);

    return 0;
}

vpnWorker::vpnWorker(QObject *parent) : QObject(parent)
{
    ptr_tunnel = 0;
}

void vpnWorker::setConfig(vpnProfile c)
{
    vpnConfig = c;
}

void vpnWorker::updateStatus(vpnClientConnection::connectionStatus status)
{
    emit statusChanged(status);
    QCoreApplication::processEvents();
}

void vpnWorker::process()
{
    qDebug() << "vpnWorker::process::slot";

    struct vpn_config config = {};

    //memset(&config, 0, sizeof (config));
    init_logging();

    log_info("Start tunnel.\n");

    //init_vpn_config(&config);
    strncpy(config.gateway_host, vpnConfig.gateway_host.toStdString().c_str(), GATEWAY_HOST_SIZE);
    config.gateway_host[GATEWAY_HOST_SIZE] = '\0';
    config.gateway_port = vpnConfig.gateway_port;
    strncpy(config.username, vpnConfig.username.toStdString().c_str(), USERNAME_SIZE);
    config.username[USERNAME_SIZE] = '\0';
    strncpy(config.password, vpnConfig.password.toStdString().c_str(), PASSWORD_SIZE);
    config.password[PASSWORD_SIZE] = '\0';
    config.set_routes = (vpnConfig.set_routes) ? 1 : 0;
    config.half_internet_routes = (vpnConfig.half_internet_routers) ? 1 : 0;
    config.user_agent = strdup("Mozilla/5.0 SV1");

    if(!vpnConfig.user_cert.isEmpty() && !vpnConfig.user_key.isEmpty())
    {
        config.user_cert = strdup(vpnConfig.user_cert.toStdString().c_str());
        config.user_key = strdup(vpnConfig.user_key.toStdString().c_str());
    }

    if(!vpnConfig.trusted_cert.isEmpty())
        add_trusted_cert(&config, vpnConfig.trusted_cert.toStdString().c_str());

    if(!vpnConfig.realm.isEmpty())
        strncpy(config.realm, vpnConfig.realm.toStdString().c_str(), REALM_SIZE);

    if(!vpnConfig.ca_file.isEmpty())
        config.ca_file = strdup(vpnConfig.ca_file.toStdString().c_str());

    config.set_dns = (vpnConfig.set_dns) ? 1 : 0;
    config.insecure_ssl = (vpnConfig.insecure_ssl) ? 1 : 0;
    config.pppd_use_peerdns = (vpnConfig.pppd_no_peerdns) ? 0 : 1;

    if(!vpnConfig.otp_prompt.isEmpty())
        config.otp_prompt = strdup(vpnConfig.otp_prompt.toStdString().c_str());

    if(vpnConfig.otp_delay > 0)
        config.otp_delay = vpnConfig.otp_delay;

    if(!vpnConfig.otp.isEmpty())
    {
        strncpy(config.otp, vpnConfig.otp.toStdString().c_str(), OTP_SIZE);
        config.otp[OTP_SIZE] = '\0';
    }

    if(!vpnConfig.pppd_log_file.isEmpty())
        config.pppd_log = strdup(vpnConfig.pppd_log_file.toStdString().c_str());

    if(!vpnConfig.pppd_plugin_file.isEmpty())
        config.pppd_plugin = strdup(vpnConfig.pppd_plugin_file.toStdString().c_str());

    if(!vpnConfig.pppd_ifname.isEmpty())
        config.pppd_ifname = strdup(vpnConfig.pppd_ifname.toStdString().c_str());

    if(!vpnConfig.pppd_ipparam.isEmpty())
        config.pppd_ipparam = strdup(vpnConfig.pppd_ipparam.toStdString().c_str());

    if(!vpnConfig.pppd_call.isEmpty())
        config.pppd_call = strdup(vpnConfig.pppd_call.toStdString().c_str());

    if(vpnConfig.debug)
        increase_verbosity();

    int ret;
    struct tunnel tunnel;

    memset(&tunnel, 0, sizeof(tunnel));
#ifdef __APPLE__
    // initialize value
    tunnel.ipv4.split_routes = 0;
#endif
    tunnel.config = &config;
    tunnel.on_ppp_if_up = on_ppp_if_up;
    tunnel.on_ppp_if_down = on_ppp_if_down;
    tunnel.ipv4.ns1_addr.s_addr = 0;
    tunnel.ipv4.ns2_addr.s_addr = 0;
    tunnel.ssl_handle = NULL;
    tunnel.ssl_socket = -1;
    tunnel.ssl_context = NULL;

start_tunnel:

    tunnel.state = STATE_CONNECTING;
    ptr_tunnel = &tunnel;

    // Step 0: get gateway host IP
    ret = get_gateway_host_ip(&tunnel);
    if (ret)
        goto err_tunnel;

    // Step 1: open a SSL connection to the gateway
    ret = ssl_connect(&tunnel);
    if (ret)
        goto err_tunnel;
    log_info("Connected to gateway.\n");

    // Step 2: connect to the HTTP interface and authenticate to get a
    // cookie
    ret = auth_log_in(&tunnel);
    if (ret != 1) {
        log_error("Could not authenticate to gateway (%s).\n",
                  err_http_str(ret));
        ret = 1;
        goto err_tunnel;
    }
    log_info("Authenticated.\n");
    log_debug("Cookie: %s\n", tunnel.cookie);

    ret = auth_request_vpn_allocation(&tunnel);
    if (ret != 1) {
        log_error("VPN allocation request failed (%s).\n",
                  err_http_str(ret));
        ret = 1;
        goto err_tunnel;
    }
    log_info("Remote gateway has allocated a VPN.\n");

    ret = ssl_connect(&tunnel);
    if (ret)
        goto err_tunnel;

    // Step 3: get configuration
    ret = auth_get_config(&tunnel);
    if (ret != 1) {
        log_error("Could not get VPN configuration (%s).\n",
                  err_http_str(ret));
        ret = 1;
        goto err_tunnel;
    }

    // Step 4: run a pppd process
    ret = pppd_run(&tunnel);
    if (ret)
        goto err_tunnel;

    // Step 5: ask gateway to start tunneling
    ret = http_send(&tunnel,
                    "GET /remote/sslvpn-tunnel HTTP/1.1\r\n"
                    "Host: sslvpn\r\n"
                    "Cookie: %s\r\n\r\n",
                    tunnel.cookie);
    if (ret != 1) {
        log_error("Could not start tunnel (%s).\n", err_http_str(ret));
        ret = 1;
        goto err_start_tunnel;
    }

    tunnel.state = STATE_CONNECTING;
    ret = 0;

    // Step 6: perform io between pppd and the gateway, while tunnel is up
    io_loop(&tunnel);

    if (tunnel.state == STATE_UP)
        if (tunnel.on_ppp_if_down != NULL)
            tunnel.on_ppp_if_down(&tunnel);

    tunnel.state = STATE_DISCONNECTING;
    //emit finished();

err_start_tunnel:
    pppd_terminate(&tunnel);
    log_info("Terminated pppd.\n");
    //emit finished();
err_tunnel:
    log_info("Closed connection to gateway.\n");

    if (ssl_connect(&tunnel)) {
        log_info("Could not log out.\n");
    } else {
        auth_log_out(&tunnel);
        log_info("Logged out.\n");
    }

    // explicitly free the buffer allocated for split routes of the ipv4 config
    if (tunnel.ipv4.split_rt != NULL) {
        free(tunnel.ipv4.split_rt);
        tunnel.ipv4.split_rt = NULL;
    }

    // If persistent try to connect again after successful connect disconnected
    if(vpnConfig.persistent and tunnel.state == STATE_DISCONNECTING)
    {
        log_info("Persistent mode enabled, trying to reconnect...\n");
        goto start_tunnel;
    }

    tunnel.state = STATE_DOWN;
    emit finished();
}

void vpnWorker::end()
{
    ptr_tunnel->on_ppp_if_down(ptr_tunnel);
}
