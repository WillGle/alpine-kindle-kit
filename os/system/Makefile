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
GW_POWER_LIGHT_SPIKE       ?= 0
GW_POWER_POWEROFF_ACCEPTED ?= 0
GW_LIFECYCLE_SPIKE         ?= 1
GW_WIFI_SPIKE              ?= 0
GW_WIFI_ACCEPTED           ?= 0

CFLAGS  := -std=c99 -Wall -Wextra -Os -fstack-protector-strong \
           -D_FORTIFY_SOURCE=2 -fPIE \
           -DGW_POWER_SUSPEND_ACCEPTED=$(GW_POWER_SUSPEND_ACCEPTED) \
           -DGW_POWER_LIGHT_ACCEPTED=$(GW_POWER_LIGHT_ACCEPTED) \
           -DGW_POWER_LIGHT_SPIKE=$(GW_POWER_LIGHT_SPIKE) \
           -DGW_LIFECYCLE_SPIKE=$(GW_LIFECYCLE_SPIKE) \
           -DGW_WIFI_SPIKE=$(GW_WIFI_SPIKE) \
           -DGW_WIFI_ACCEPTED=$(GW_WIFI_ACCEPTED) \
           -DGW_POWER_POWEROFF_ACCEPTED=$(GW_POWER_POWEROFF_ACCEPTED)

REPO_ROOT := $(shell cd ../.. && pwd)
LVGL_DIR  ?= $(REPO_ROOT)/work/sources/lvgl
LDFLAGS   ?= -Wl,-z,relro,-z,now

# ── System service ────────────────────────────────────────────────────────

.PHONY: all service ui check check-both clean test test-stage3 test-power test-power-io test-apps test-apps-install fixtures

all: service gwctl gw-appctl

service: gw-system
	@echo "GW_SYSTEM built OK"

gw-system: gw-system.c gw-app-manifest.c gw-network.c gw-stage3.c gw-power-policy.c \
           gw-apps.c gw-signature.c \
           gw-ipc.h gw-power.h gw-power-policy.h gw-app-manifest.h gw-network.h \
           gw-stage3.h gw-apps.h gw-signature.h
	$(CC) $(CFLAGS) -o $@ gw-system.c gw-app-manifest.c gw-network.c gw-stage3.c \
	    gw-power-policy.c gw-apps.c gw-signature.c -lrt -ldl $(LDFLAGS) -pie

gwctl: gwctl.c gw-ipc.h
	$(CC) $(CFLAGS) -o $@ gwctl.c -lrt $(LDFLAGS) -pie

gw-appctl: gw-appctl.c gw-apps.c gw-app-manifest.c gw-signature.c \
           gw-apps.h gw-app-manifest.h gw-signature.h
	$(CC) $(CFLAGS) -o $@ gw-appctl.c gw-apps.c gw-app-manifest.c gw-signature.c -lrt -ldl $(LDFLAGS) -pie

# Experimental data-image trial target (opt-in). Not part of base OS.
data-mount: gw-data-mount.c
	$(CC) $(CFLAGS) -o gw-data-mount gw-data-mount.c $(LDFLAGS) -pie
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
	    gw-system.c gwctl.c gw-appctl.c gw-app-manifest.c gw-network.c \
	    gw-stage3.c gw-power-policy.c gw-apps.c gw-signature.c
	@echo "Syntax check passed"

# Both lifecycle variants must compile: the default is 1, and some product
# builds keep the supervisory paths disabled.
check-both:
	$(MAKE) check GW_LIFECYCLE_SPIKE=0
	$(MAKE) check GW_LIFECYCLE_SPIKE=1

test-stage3: test-stage3.c gw-stage3.c gw-stage3.h
	$(HOSTCC) $(CFLAGS) -I. -o test-stage3 test-stage3.c gw-stage3.c
	./test-stage3

test-power: test-power-policy.c gw-power-policy.c gw-power-policy.h
	$(HOSTCC) $(CFLAGS) -I. -o test-power-policy test-power-policy.c gw-power-policy.c
	./test-power-policy

test-power-io: test-power-io.c gw-power.h
	$(HOSTCC) -std=c99 -Wall -Wextra -Os -I. -o test-power-io test-power-io.c
	./test-power-io

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

test: test-stage3 test-power test-power-io test-apps test-apps-install

clean:
	rm -f gw-system gwctl gw-appctl test-stage3 test-power-policy test-power-io test-apps test-apps-install
	rm -rf test-sandbox
	$(MAKE) -C ../ui clean 2>/dev/null || true
