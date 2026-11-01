# SPDX-License-Identifier: GPL-3.0-or-later
# Build system service and UI shell for PW3 Alpine OS.
#
# Usage:
#   make CC=arm-linux-musleabihf-gcc          # system service only
#   make CC=arm-linux-musleabihf-gcc ui       # system service + UI shell
#   make CC=arm-linux-musleabihf-gcc check    # syntax check (host compiler)
#   make clean
#
# LVGL must be placed at $(LVGL_DIR) before 'make ui' will succeed.
# Default: work/sources/lvgl relative to repository root.
# Override: make LVGL_DIR=/path/to/lvgl ui
#
# Acceptance flags (set only after real-device evidence):
#   make CC=... GW_POWER_SUSPEND_ACCEPTED=1    # enables mem write in gw-power.h
#   make CC=... GW_POWER_LIGHT_ACCEPTED=1      # enables frontlight write
#   make CC=... GW_POWER_POWEROFF_ACCEPTED=1   # enables poweroff path

CC      ?= arm-linux-musleabihf-gcc
STRIP   ?= arm-linux-musleabihf-strip

GW_POWER_SUSPEND_ACCEPTED  ?= 0
GW_POWER_LIGHT_ACCEPTED    ?= 0
GW_POWER_LIGHT_SPIKE       ?= 0
GW_POWER_POWEROFF_ACCEPTED ?= 0
GW_LIFECYCLE_SPIKE         ?= 0
GW_WIFI_SPIKE              ?= 0
GW_WIFI_ACCEPTED           ?= 0

CFLAGS  := -std=c99 -Wall -Wextra -Os -fstack-protector-strong \
           -DGW_POWER_SUSPEND_ACCEPTED=$(GW_POWER_SUSPEND_ACCEPTED) \
           -DGW_POWER_LIGHT_ACCEPTED=$(GW_POWER_LIGHT_ACCEPTED) \
           -DGW_POWER_LIGHT_SPIKE=$(GW_POWER_LIGHT_SPIKE) \
           -DGW_LIFECYCLE_SPIKE=$(GW_LIFECYCLE_SPIKE) \
           -DGW_WIFI_SPIKE=$(GW_WIFI_SPIKE) \
           -DGW_WIFI_ACCEPTED=$(GW_WIFI_ACCEPTED) \
           -DGW_POWER_POWEROFF_ACCEPTED=$(GW_POWER_POWEROFF_ACCEPTED)

REPO_ROOT := $(shell cd ../.. && pwd)
LVGL_DIR  ?= $(REPO_ROOT)/work/sources/lvgl

# ── System service ────────────────────────────────────────────────────────

.PHONY: all service ui check clean test test-stage3 test-power test-power-io test-apps

all: service gwctl gw-appctl

service: gw-system
	@echo "GW_SYSTEM built OK"

gw-system: gw-system.c gw-app-manifest.c gw-network.c gw-stage3.c gw-power-policy.c \
           gw-apps.c gw-signature.c \
           gw-ipc.h gw-power.h gw-power-policy.h gw-app-manifest.h gw-network.h \
           gw-stage3.h gw-apps.h gw-signature.h
	$(CC) $(CFLAGS) -o $@ gw-system.c gw-app-manifest.c gw-network.c gw-stage3.c \
	    gw-power-policy.c gw-apps.c gw-signature.c -lrt

gwctl: gwctl.c gw-ipc.h
	$(CC) $(CFLAGS) -o $@ gwctl.c -lrt

gw-appctl: gw-appctl.c gw-apps.c gw-app-manifest.c gw-signature.c \
           gw-apps.h gw-app-manifest.h gw-signature.h
	$(CC) $(CFLAGS) -o $@ gw-appctl.c gw-apps.c gw-app-manifest.c gw-signature.c -lrt

# Experimental data-image trial target (opt-in). Not part of base OS.
data-mount: gw-data-mount.c
	$(CC) $(CFLAGS) -o gw-data-mount gw-data-mount.c
	@echo "GW_DATA_MOUNT (experimental) built OK"
# ── LVGL UI shell spike ───────────────────────────────────────────────────
#
# The UI shell renders Home, Library and Settings directly to the PW3
# framebuffer (8 bpp, pitch 1088) via LVGL + custom EPDC flush driver.
# This target is a build spike: it must compile and link, but the
# EPDC flush is a stub until the real-device rendering probe passes.
#
# See ../ui/gw-shell.c and ../ui/lv_port_epdc.c for the implementation.

ui: gw-system
	$(MAKE) -C ../ui \
	    CC="$(CC)" CFLAGS="$(CFLAGS)" LVGL_DIR="$(LVGL_DIR)"

# ── Syntax check (host) ───────────────────────────────────────────────────

check:
	$(CC) -std=c99 -Wall -Wextra -fsyntax-only \
	    -DGW_POWER_SUSPEND_ACCEPTED=0 \
	    -DGW_POWER_LIGHT_ACCEPTED=0 \
	    -DGW_POWER_POWEROFF_ACCEPTED=0 \
	    -DGW_LIFECYCLE_SPIKE=1 \
	    gw-system.c gwctl.c gw-appctl.c gw-app-manifest.c gw-network.c \
	    gw-stage3.c gw-power-policy.c gw-apps.c gw-signature.c
	@echo "Syntax check passed"

test-stage3: test-stage3.c gw-stage3.c gw-stage3.h
	$(CC) $(CFLAGS) -I. -o test-stage3 test-stage3.c gw-stage3.c
	./test-stage3

test-power: test-power-policy.c gw-power-policy.c gw-power-policy.h
	$(CC) $(CFLAGS) -I. -o test-power-policy test-power-policy.c gw-power-policy.c
	./test-power-policy

test-power-io: test-power-io.c gw-power.h
	$(CC) -std=c99 -Wall -Wextra -Os -I. -o test-power-io test-power-io.c
	./test-power-io

test-apps: test-apps.c gw-apps.c gw-app-manifest.c gw-signature.c \
           gw-apps.h gw-app-manifest.h gw-signature.h
	$(CC) $(CFLAGS) -I. -o test-apps test-apps.c gw-apps.c \
	    gw-app-manifest.c gw-signature.c -ldl
	./test-apps

test: test-stage3 test-power test-power-io test-apps

clean:
	rm -f gw-system gwctl gw-appctl test-stage3 test-power-policy test-power-io test-apps
	$(MAKE) -C ../ui clean 2>/dev/null || true
