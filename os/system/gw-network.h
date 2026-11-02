/* SPDX-License-Identifier: GPL-3.0-or-later */
#ifndef GW_NETWORK_H
#define GW_NETWORK_H
#include <stddef.h>

/* Credentials are hex encoded on IPC and are never command-line arguments. */
int gw_network_save(const char *ssid, size_t ssid_len, const char *password, size_t password_len);
/* 1: associated with IPv4, 0: disconnected, -1: unavailable. */
int gw_network_status(void);
int gw_network_enable_hotplug(void);
/* Returns a bounded printable scan result, indexed from zero.  signal_dbm is
 * the scan signal level (0 when unknown); secure is 1 for WPA/WPA2/RSN/WEP. */
int gw_network_scan_result(unsigned index, char *ssid_hex, size_t size,
                           int *signal_dbm, int *secure);
/* Parse the scan file once and emit "<ssid_hex> <signal> <secure>\n" rows.
 * Returns the row count, or -1 when the file is unavailable. */
int gw_network_scan_all(char *out, size_t size);
/* Read the SSID saved in the persistent Wi-Fi configuration, as hex. */
int gw_network_saved_ssid(char *ssid_hex, size_t size);
/* Read the IPv4 address of wlan0 into buf (at least 16 bytes). */
int gw_network_ip(char *buf, size_t size);
#endif
