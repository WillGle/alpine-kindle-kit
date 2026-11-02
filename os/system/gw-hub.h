/* SPDX-License-Identifier: GPL-3.0-or-later
 * Kindle hub wire contract shared by gw-agent and gw-shell.
 *
 * The agent owns the network connection and writes raw 8 bpp frames into a
 * tmpfs file.  The shell owns /dev/fb0 and is the only process that applies a
 * frame to the panel.  Control messages are newline-terminated ASCII lines;
 * frame pixels never travel over the control socket.
 */
#ifndef GW_HUB_CONTRACT_H
#define GW_HUB_CONTRACT_H

#define GW_HUB_SOCKET          "/run/gw-hub.sock"
#define GW_HUB_DIR             "/run/gw-hub"
#define GW_HUB_FRAME           GW_HUB_DIR "/frame.raw"
#define GW_HUB_FRAME_TMP       GW_HUB_DIR "/frame.raw.tmp"
#define GW_HUB_CONFIG          "/etc/gleworks/hub.conf"
#define GW_HUB_CONFIG_FALLBACK "/mnt/us/gleworks/hub.conf"

#define GW_HUB_W               1072
#define GW_HUB_H               1448
#define GW_HUB_BPP             8
#define GW_HUB_FRAME_MAX       (GW_HUB_W * GW_HUB_H)

#define GW_HUB_LINE_MAX        256
#define GW_HUB_HOST_MAX        128
#define GW_HUB_TOKEN_MAX       96
#define GW_HUB_ID_MAX          64
#define GW_HUB_PATH_MAX        128

/* Agent → shell. */
#define GW_HUB_MSG_FRAME       "FRAME"
#define GW_HUB_MSG_CLEAR       "CLEAR"
#define GW_HUB_MSG_STATUS      "STATUS"

/* Shell → agent. */
#define GW_HUB_MSG_TOUCH       "TOUCH"
#define GW_HUB_MSG_KEY         "KEY"
#define GW_HUB_MSG_VIEW        "VIEW"

#endif /* GW_HUB_CONTRACT_H */
