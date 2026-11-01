/* ── IPC client helper ───────────────────────────────────────────────────── */

#if GW_UI_HOST_SIM
/* Deterministic host responses — no device, no /dev/fb0, no /run socket. */

GW_SHELL_INTERNAL void host_ipc_fail_next(const char *prefix) {
    g_host_ipc_fail_prefix = prefix;
}

GW_SHELL_INTERNAL int ipc_cmd(const char *cmd, char *resp, size_t resp_size) {
    if (!cmd || !resp || resp_size == 0) return -1;
    resp[0] = '\0';
    if (g_host_ipc_fail_prefix && !strncmp(cmd, g_host_ipc_fail_prefix,
                                           strlen(g_host_ipc_fail_prefix))) {
        g_host_ipc_fail_prefix = NULL;
        return -1;
    }
    if (!strcmp(cmd, GW_CMD_STATUS)) {
        snprintf(resp, resp_size,
                 "STATUS version=host boot=preview power=battery battery=82 "
                 "light=37 foreground=none net=connected usb=none "
                 "reader-exit=0 lock=unlocked wifi-error=0 wifi-busy=0\n");
        return 0;
    }
    if (!strcmp(cmd, "NET SCAN")) {
        snprintf(resp, resp_size, "OK\n");
        return 0;
    }
    if (!strcmp(cmd, GW_CMD_NET_STATUS)) {
        static const char ssid[] = "Home Wi-Fi";
        size_t i, used = (size_t)snprintf(resp, resp_size, "NET connected ");
        for (i = 0; ssid[i] && used + 2 < resp_size; i++)
            used += (size_t)snprintf(resp + used, resp_size - used, "%02x",
                                     (unsigned char)ssid[i]);
        snprintf(resp + used, resp_size - used, " 192.168.1.200\n");
        return 0;
    }
    if (!strncmp(cmd, "NET RESULT ", 11)) {
        static const struct { const char *ssid; int signal; int secure; } nets[] = {
            { "Home Wi-Fi", -50, 1 },
            { "Library Wi-Fi", -70, 0 },
            { "Cafe", -78, 0 }
        };
        unsigned index = (unsigned)atoi(cmd + 11);
        if (index >= sizeof(nets) / sizeof(nets[0])) return -1;
        {
            const unsigned char *s = (const unsigned char *)nets[index].ssid;
            size_t i, used = (size_t)snprintf(resp, resp_size, "NET SSID ");
            for (i = 0; s[i] && used + 2 < resp_size; i++)
                used += (size_t)snprintf(resp + used, resp_size - used, "%02x", s[i]);
            snprintf(resp + used, resp_size - used, " %d %d\n",
                     nets[index].signal, nets[index].secure);
        }
        return 0;
    }
    if (!strcmp(cmd, GW_CMD_LIGHT_GET)) {
        snprintf(resp, resp_size, "LIGHT 37\n");
        return 0;
    }
    if (!strcmp(cmd, "APP COUNT")) {
        snprintf(resp, resp_size, "APPS 8\n");
        return 0;
    }
    if (!strncmp(cmd, "APP INFO ", 9)) {
        static const char *const names[8] = {
            "notes", "dictionary", "clock", "gallery",
            "calc", "reader", "radio", "tools"
        };
        int index = atoi(cmd + 9);
        if (index < 0 || index >= 8) {
            snprintf(resp, resp_size, "APP END\n");
        } else {
            snprintf(resp, resp_size, "APP %s 1.%d.0 %s\n",
                     names[index], index, "Host long-list fixture");
        }
        return 0;
    }
    if (!strcmp(cmd, "APP SCAN")) {
        snprintf(resp, resp_size, "APP USB 1\n");
        return 0;
    }
    if (!strcmp(cmd, "APP USB 0")) {
        snprintf(resp, resp_size, "APP USBFILE notes-1.3.0.gwapp\n");
        return 0;
    }
    if (!strcmp(cmd, "APP USB 1")) {
        snprintf(resp, resp_size, "APP USB END\n");
        return 0;
    }
    if (!strncmp(cmd, "APP INSTALL USB", 15) || !strncmp(cmd, "APP REMOVE ", 11)) {
        snprintf(resp, resp_size, "OK\n");
        return 0;
    }
    snprintf(resp, resp_size, "OK\n");
    return 0;
}
#else
/*
 * Send one command to gw-system and read back one line response.
 * Returns 0 on success, -1 on error.  buf must be at least GW_IPC_MAX_LINE.
 */
GW_SHELL_INTERNAL int ipc_cmd(const char *cmd, char *resp, size_t resp_size) {
    int fd;
    struct sockaddr_un addr;
    struct timeval timeout = { .tv_sec = 2, .tv_usec = 0 };
    size_t used = 0;

    if (!cmd || !resp || resp_size == 0) return -1;
    resp[0] = '\0';
    fd = socket(AF_UNIX, SOCK_STREAM | SOCK_CLOEXEC, 0);
    if (fd < 0) return -1;
    if (setsockopt(fd, SOL_SOCKET, SO_RCVTIMEO, &timeout, sizeof(timeout)) < 0 ||
        setsockopt(fd, SOL_SOCKET, SO_SNDTIMEO, &timeout, sizeof(timeout)) < 0) {
        close(fd); return -1;
    }

    memset(&addr, 0, sizeof(addr));
    addr.sun_family = AF_UNIX;
    strncpy(addr.sun_path, GW_IPC_CMD_SOCKET, sizeof(addr.sun_path) - 1);

    if (connect(fd, (struct sockaddr *)&addr, sizeof(addr)) < 0) {
        close(fd); return -1;
    }

    size_t len = strlen(cmd);
    if (write(fd, cmd, len) != (ssize_t)len ||
        write(fd, "\n", 1) != 1) {
        close(fd); return -1;
    }

    while (used + 1 < resp_size) {
        ssize_t n = read(fd, resp + used, resp_size - 1 - used);
        if (n <= 0) { close(fd); return -1; }
        used += (size_t)n;
        if (resp[used - 1] == '\n') break;
    }
    close(fd);
    if (!used || resp[used - 1] != '\n') return -1;
    resp[used] = '\0';
    return 0;
}
#endif

