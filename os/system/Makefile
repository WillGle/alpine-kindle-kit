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
HOSTCC  ?= gcc
STRIP   ?= arm-linux-musleabihf-strip
BUSYBOX ?= $(shell command -v busybox 2>/dev/null || echo /bin/busybox)

GW_POWER_SUSPEND_ACCEPTED  ?= 0
GW_POWER_LIGHT_ACCEPTED    ?= 0
GW_POWER_POWEROFF_ACCEPTED ?= 0
GW_LIFECYCLE_SPIKE         ?= 1
GW_WIFI_SPIKE              ?= 0
GW_WIFI_ACCEPTED           ?= 0

CFLAGS  := -std=c99 -Wall -Wextra -Os -fstack-protector-strong \
           -D_FORTIFY_SOURCE=2 -fPIE \
           -DGW_POWER_SUSPEND_ACCEPTED=$(GW_POWER_SUSPEND_ACCEPTED) \
           -DGW_POWER_LIGHT_ACCEPTED=$(GW_POWER_LIGHT_ACCEPTED) \
           -DGW_LIFECYCLE_SPIKE=$(GW_LIFECYCLE_SPIKE) \
           -DGW_WIFI_SPIKE=$(GW_WIFI_SPIKE) \
           -DGW_WIFI_ACCEPTED=$(GW_WIFI_ACCEPTED) \
           -DGW_POWER_POWEROFF_ACCEPTED=$(GW_POWER_POWEROFF_ACCEPTED)

REPO_ROOT := $(shell cd ../.. && pwd)
LVGL_DIR  ?= $(REPO_ROOT)/work/sources/lvgl
LDFLAGS   ?= -Wl,-z,relro,-z,now
# Static ARM images ship without a dynamic loader, so device builds pass
# PIEFLAGS= and keep an EXEC entry point; host builds default to PIE.
PIEFLAGS  ?= -pie
# libcrypto is loaded with dlopen; dynamic ARM builds get these from libc.
LIBS      ?= -lrt -ldl

# ── System service ────────────────────────────────────────────────────────

.PHONY: all service ui check check-both clean test test-stage3 test-power test-power-io test-network test-command-policy test-ipc test-ipc-client test-ipc-roundtrip test-apps test-apps-install fixtures

all: service gwctl gw-appctl gw-agent

service: gw-system
	@echo "GW_SYSTEM built OK"

gw-system: gw-system.c gw-system-state.c gw-system-power.c gw-app-manifest.c gw-network.c gw-stage3.c gw-file.c gw-power-policy.c \
           gw-apps.c gw-signature.c \
           gw-ipc.h gw-power.h gw-power-policy.h gw-app-manifest.h gw-network.h \
           gw-stage3.h gw-apps.h gw-signature.h gw-system-state.h gw-system-power.h
	$(CC) $(CFLAGS) -o $@ gw-system.c gw-system-state.c gw-system-power.c gw-app-manifest.c gw-network.c gw-stage3.c gw-file.c \
	    gw-power-policy.c gw-apps.c gw-signature.c $(LIBS) $(LDFLAGS) $(PIEFLAGS)

gwctl: gwctl.c gw-ipc.h
	$(CC) $(CFLAGS) -o $@ gwctl.c $(LIBS) $(LDFLAGS) $(PIEFLAGS)

gw-appctl: gw-appctl.c gw-apps.c gw-app-manifest.c gw-signature.c \
           gw-apps.h gw-app-manifest.h gw-signature.h
	$(CC) $(CFLAGS) -o $@ gw-appctl.c gw-apps.c gw-app-manifest.c gw-signature.c $(LIBS) $(LDFLAGS) $(PIEFLAGS)

gw-agent: gw-agent.c gw-hub.h
	$(CC) $(CFLAGS) -o $@ gw-agent.c $(LDFLAGS) $(PIEFLAGS)

# Experimental data-image trial target (opt-in). Not part of base OS.
data-mount: gw-data-mount.c
	$(CC) $(CFLAGS) -o gw-data-mount gw-data-mount.c $(LDFLAGS) $(PIEFLAGS)
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
	    -DGW_LIFECYCLE_SPIKE=$(GW_LIFECYCLE_SPIKE) \
	    gw-system.c gw-system-state.c gw-system-power.c gwctl.c gw-appctl.c \
	    gw-app-manifest.c gw-network.c gw-stage3.c gw-file.c gw-power-policy.c \
	    gw-apps.c gw-signature.c gw-agent.c
	@echo "Syntax check passed"

# Both lifecycle variants must compile: the default is 1, and some product
# builds keep the supervisory paths disabled.
check-both:
	$(MAKE) check GW_LIFECYCLE_SPIKE=0
	$(MAKE) check GW_LIFECYCLE_SPIKE=1

test-stage3: test-stage3.c gw-stage3.c gw-file.c gw-stage3.h gw-file.h
	$(HOSTCC) $(CFLAGS) -I. -o test-stage3 test-stage3.c gw-stage3.c gw-file.c
	./test-stage3

test-power: test-power-policy.c gw-power-policy.c gw-power-policy.h
	$(HOSTCC) $(CFLAGS) -I. -o test-power-policy test-power-policy.c gw-power-policy.c
	./test-power-policy

test-power-io: test-power-io.c gw-power.h
	@tmp=$$(mktemp -d); trap 'rm -rf "$$tmp"' EXIT; \
	$(HOSTCC) -std=c99 -Wall -Wextra -Os -I. \
	    -DGW_SYSFS_RTC_WAKEALARM="\"$$tmp/wakealarm\"" \
	    -DGW_SYSFS_RTC_EPOCH="\"$$tmp/epoch\"" \
	    -DGW_SYSFS_WAKEUP_COUNT="\"$$tmp/wakeup-count\"" \
	    -DGW_SYSFS_PM_STATE="\"$$tmp/pm-state\"" \
	    -o test-power-io test-power-io.c && \
	./test-power-io

test-command-policy: test-command-policy.c gw-command-policy.h
	$(HOSTCC) -std=c99 -Wall -Wextra -O2 -I. -o test-command-policy test-command-policy.c
	./test-command-policy

# Integration binary: same service sources with test paths, no hardware gates.
gw-system-test: gw-system.c gw-system-state.c gw-system-power.c gw-app-manifest.c gw-network.c gw-stage3.c gw-file.c \
               gw-power-policy.c gw-apps.c gw-signature.c gw-ipc.h
	$(HOSTCC) -std=c99 -Wall -Wextra -O1 -I. \
	    -DGW_LIFECYCLE_SPIKE=0 \
	    -DGW_POWER_SUSPEND_ACCEPTED=0 -DGW_POWER_LIGHT_ACCEPTED=0 \
	    -DGW_POWER_POWEROFF_ACCEPTED=0 -DGW_ENABLE_HW_TRIAL=0 \
	    -DGW_WIFI_ACCEPTED=0 \
	    -DGW_IPC_CMD_SOCKET='"test-ipc.cmd"' \
	    -DGW_IPC_EVT_SOCKET='"test-ipc.evt"' \
	    -DGW_IPC_PID_FILE='"test-ipc.pid"' \
	    -DGW_IPC_IDLE_SECONDS=1 \
	    -DGW_TIME_CONFIG_PATH='"test-ipc-time.conf"' \
	    -DGW_LIGHT_CONFIG_PATH='"test-ipc-light.conf"' \
	    -DGW_WIFI_ENABLED_PATH='"test-ipc-wifi.conf"' \
	    -o gw-system-test gw-system.c gw-system-state.c gw-system-power.c gw-app-manifest.c gw-network.c \
	    gw-stage3.c gw-file.c gw-power-policy.c gw-apps.c gw-signature.c \
	    $(LIBS)

test-ipc-client: tests/test-ipc-client.c gw-ipc-client.c gw-ipc-client.h
	$(HOSTCC) -std=c99 -Wall -Wextra -O1 -I. \
	    -DGW_IPC_CMD_SOCKET='"test-client.cmd"' \
	    -o test-ipc-client tests/test-ipc-client.c gw-ipc-client.c
	./test-ipc-client
	rm -f test-ipc-client test-client.cmd

test-ipc-roundtrip: tests/test-ipc-roundtrip.c gw-ipc-client.c gw-ipc-client.h
	$(HOSTCC) -std=c99 -Wall -Wextra -O1 -I. \
	    -DGW_IPC_CMD_SOCKET='"test-roundtrip.cmd"' \
	    -o test-ipc-roundtrip tests/test-ipc-roundtrip.c gw-ipc-client.c
	./test-ipc-roundtrip
	rm -f test-ipc-roundtrip test-roundtrip.cmd

test-ipc: gw-system-test tests/test-ipc-service.c
	$(HOSTCC) -std=c99 -Wall -Wextra -O1 -I. -o test-ipc-service \
	    tests/test-ipc-service.c
	./test-ipc-service ./gw-system-test
	rm -f gw-system-test test-ipc-service test-ipc.cmd test-ipc.evt \
	    test-ipc.pid test-ipc-time.conf test-ipc-light.conf test-ipc-wifi.conf

test-network: tests/test-network.c gw-network.c gw-network.h
	rm -rf test-network-sandbox
	$(HOSTCC) -std=c99 -Wall -Wextra -O1 -I. \
	    -DGW_WIFI_TESTING \
	    -DGW_WIFI_DIR='"$(CURDIR)/test-network-sandbox"' \
	    -o test-network tests/test-network.c gw-network.c
	./test-network
	rm -rf test-network-sandbox

test-apps: test-apps.c gw-apps.c gw-app-manifest.c gw-signature.c \
           gw-apps.h gw-app-manifest.h gw-signature.h
	$(HOSTCC) $(CFLAGS) -I. -o test-apps test-apps.c gw-apps.c \
	    gw-app-manifest.c gw-signature.c -ldl
	./test-apps

# Signed-package fixtures.  The private keys are test-only and are never
# installed in an image.  Run with libcrypto available to the dynamic loader.
TEST_KEY_SHA256 := 6551112d5e888ad755c25fbcff706c5b23fafce5a805955af25db0477ea969b4
test-apps-install: tests/test-apps-install.c tests/fixtures/valid.gwapp \
                   gw-apps.c gw-app-manifest.c gw-signature.c
	$(HOSTCC) -std=c99 -Wall -Wextra -O1 -I. \
	    -DGW_APPS_TESTING -DGW_CRYPTO_ALLOW_SEARCH=1 \
	    -DGW_BUSYBOX_PATH='"$(BUSYBOX)"' \
	    -DGW_APP_KEY_PATH='"$(CURDIR)/tests/keys/test-release.pem"' \
	    -DGW_APP_KEY_SHA256='"$(TEST_KEY_SHA256)"' \
	    -o test-apps-install tests/test-apps-install.c gw-apps.c \
	    gw-app-manifest.c gw-signature.c -ldl
	rm -rf test-sandbox && mkdir -p test-sandbox
	./test-apps-install tests/fixtures test-sandbox
	rm -rf test-sandbox

fixtures:
	python3 tests/generate-fixtures.py

test: test-stage3 test-power test-power-io test-network test-command-policy test-ipc test-ipc-client test-ipc-roundtrip test-apps test-apps-install

clean:
	rm -f gw-system gwctl gw-appctl gw-agent test-stage3 test-power-policy test-power-io test-apps test-apps-install test-network test-command-policy test-ipc test-ipc-client test-ipc-roundtrip gw-system-test
	rm -rf test-sandbox test-network-sandbox
	$(MAKE) -C ../ui clean 2>/dev/null || true
