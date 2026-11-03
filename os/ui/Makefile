# SPDX-License-Identifier: GPL-3.0-or-later
# Build the LVGL UI shell spike for PW3.
#
# Requires LVGL 8.x source tree at $(LVGL_DIR).
# Run from os/system/Makefile via 'make ui', or standalone:
#
#   make CC=arm-linux-musleabihf-gcc LVGL_DIR=/path/to/lvgl
#
# Acceptance flags (default 0 until real-device evidence):
#   GW_UI_EPDC_ACCEPTED=1    — enables fb mmap + EPDC ioctl in lv_port_epdc.h
#   GW_UI_TOUCH_ACCEPTED=1   — enables evdev read in lv_port_input.h

CC      ?= arm-linux-musleabihf-gcc
HOSTCC  ?= gcc
STRIP   ?= arm-linux-musleabihf-strip
CFLAGS  ?= -std=c99 -Wall -Wextra -Os -D_GNU_SOURCE -D_FORTIFY_SOURCE=2 -fPIE
LDFLAGS ?= -Wl,-z,relro,-z,now
PIEFLAGS ?= -pie

GW_UI_EPDC_ACCEPTED  ?= 0
GW_UI_TOUCH_ACCEPTED ?= 0
GW_ENABLE_HW_TRIAL   ?= 0
GW_UI_LIFECYCLE_SPIKE ?= 0
GW_UI_TOUCH_DEV_OVERRIDE ?= 0
# High-frequency diagnostics are opt-in.  Production builds leave this at 0;
# a diagnostic build may use 1 and must also enable GW_UI_PERF_LOG at runtime.
GW_UI_PERF           ?= 0
GW_UI_DIAG           ?= 0
GW_UI_PERF_WAIT      ?= 0
REPO_ROOT := $(shell cd ../.. && pwd)
LVGL_DIR  ?= $(REPO_ROOT)/work/sources/lvgl

LVGL_CFLAGS := \
    -I$(dir $(LVGL_DIR)) \
    -DLV_CONF_SKIP \
    -DLV_COLOR_DEPTH=8 \
    -DLV_MEM_SIZE=262144 \
    -DLV_FONT_MONTSERRAT_28=1 \
    '-DLV_FONT_DEFAULT=&lv_font_montserrat_28' \
    -DLV_INDEV_DEF_READ_PERIOD=100 \
    -DLV_DISP_DEF_REFR_PERIOD=16 \
    -DGW_UI_EPDC_ACCEPTED=$(GW_UI_EPDC_ACCEPTED) \
    -DGW_UI_TOUCH_ACCEPTED=$(GW_UI_TOUCH_ACCEPTED) \
    -DGW_ENABLE_HW_TRIAL=$(GW_ENABLE_HW_TRIAL) \
    -DGW_UI_TOUCH_DEV_OVERRIDE=$(GW_UI_TOUCH_DEV_OVERRIDE) \
    -DGW_UI_LIFECYCLE_SPIKE=$(GW_UI_LIFECYCLE_SPIKE) \
    -DGW_UI_PERF=$(GW_UI_PERF) \
    -DGW_UI_PERF_WAIT=$(GW_UI_PERF_WAIT) \
    -DGW_UI_DIAG=$(GW_UI_DIAG)

# Build the pinned LVGL 8 source tree with its built-in defaults.
LVGL_SRC := $(shell find $(LVGL_DIR)/src -type f -name '*.c')
FONT_SRC := $(wildcard fonts/*.c)

SRCS := gw-shell.c ../system/gw-file.c ../system/gw-ipc-client.c gw_shell_state.c gw_screen_home.c gw_screen_settings.c gw_screen_calendar.c gw_screen_notes.c gw_screen_network.c gw_screen_apps.c gw_screen_hub.c gw_hub.c gw_screen_lock.c gw_lock_config.c gw_shell_date.c gw_todo_store.c gw_ui_kit.c gw_ui_strings.c gw_ui_style.c gw_epdc_policy.c gw_paint_profile.c gw_paint_rules.c lv_port_epdc_state.c $(FONT_SRC) $(LVGL_SRC)

.PHONY: all check clean host-sim preview test-epdc-policy test-epdc-batch test-y8-lut test-paint-profile test-paint-rules test-lock-config

all: gw-shell

# Debug build (no optimization) to isolate segfaults.
debug: $(SRCS) lv_port_epdc.h lv_port_input.h gw_theme_tokens.h
	@if [ ! -d "$(LVGL_DIR)" ]; then echo "ERROR: LVGL not found"; exit 1; fi
	$(CC) -std=c99 -O0 -g -Wall -Wextra $(LVGL_CFLAGS) -I../system -o gw-shell-debug $(SRCS) -lm -lrt
	@echo "GW_SHELL debug build OK"

gw-shell: $(SRCS) lv_port_epdc.h lv_port_epdc_state.c lv_port_input.h gw_theme_tokens.h gw_ui_kit.h \
          gw_shell_types.h gw_shell_state.h \
          gw_ui_strings.h gw_icons.h
	@if [ ! -d "$(LVGL_DIR)" ]; then \
	    echo "ERROR: LVGL not found at $(LVGL_DIR)"; \
	    echo "Place LVGL 8.x source there or override: make LVGL_DIR=..."; \
	    exit 1; \
	fi
	$(CC) $(CFLAGS) $(LVGL_CFLAGS) -I../system -o $@ $(SRCS) -lm -lrt $(LDFLAGS) $(PIEFLAGS)
	$(STRIP) $@
	@echo "GW_SHELL built OK (EPDC_accepted=$(GW_UI_EPDC_ACCEPTED) touch_accepted=$(GW_UI_TOUCH_ACCEPTED))"

# Host-only LVGL simulator.  Renders into an in-memory 1072x1448 framebuffer;
# no /dev/fb0, evdev, EPDC ioctl or device access is involved.
HOST_CFLAGS := -std=c99 -O2 -g -Wall -Wextra \
    -I$(dir $(LVGL_DIR)) \
    -DLV_CONF_SKIP \
    -DLV_COLOR_DEPTH=8 \
    -DLV_MEM_SIZE=262144 \
    -DLV_FONT_MONTSERRAT_14=1 \
    -DLV_FONT_MONTSERRAT_28=1 \
    '-DLV_FONT_DEFAULT=&lv_font_montserrat_14' \
    -DLV_INDEV_DEF_READ_PERIOD=100 \
    -DLV_DISP_DEF_REFR_PERIOD=16 \
    -DGW_UI_HOST_SIM=1 \
    -DGW_UI_EPDC_ACCEPTED=0 \
    -DGW_UI_TOUCH_ACCEPTED=0 \
    -DGW_ENABLE_HW_TRIAL=0 \
    -DGW_UI_LIFECYCLE_SPIKE=1 \
    -DGW_UI_PERF=0 \
    -DGW_UI_DIAG=0 \
    -I../system

host-sim: host_sim.c gw-shell.c ../system/gw-file.c ../system/gw-ipc-client.c gw_shell_state.c gw_screen_home.c gw_screen_settings.c gw_screen_calendar.c gw_screen_notes.c gw_screen_network.c gw_screen_apps.c gw_screen_hub.c gw_hub.c gw_screen_lock.c gw_shell_internal.h gw_shell_test.h gw_shell_state.h gw_shell_types.h lv_port_epdc_state.c gw_ui_kit.c gw_ui_strings.c gw_ui_style.c gw_ui_kit.h gw_ui_style.h \
          gw_theme_tokens.h gw_fonts.h gw_ui_strings.h gw_icons.h \
          lv_port_epdc.h lv_port_input.h $(FONT_SRC)
	@if [ ! -d "$(LVGL_DIR)" ]; then \
	    echo "ERROR: LVGL not found at $(LVGL_DIR)"; exit 1; \
	fi
	$(HOSTCC) $(HOST_CFLAGS) -o $@ host_sim.c gw-shell.c ../system/gw-file.c ../system/gw-ipc-client.c gw_shell_state.c gw_screen_home.c gw_screen_settings.c gw_screen_calendar.c gw_screen_notes.c gw_screen_network.c gw_screen_apps.c gw_screen_hub.c gw_hub.c gw_screen_lock.c gw_lock_config.c gw_shell_date.c gw_todo_store.c gw_ui_kit.c gw_ui_strings.c gw_ui_style.c gw_epdc_policy.c gw_paint_profile.c gw_paint_rules.c lv_port_epdc_state.c $(FONT_SRC) $(LVGL_SRC) -lm
	@echo "GW_UI host simulator built OK"

# Build and render the five acceptance screenshots plus geometry.json.
preview: host-sim
	./sim/run-preview.sh

# Syntax check against the selected LVGL source.
check: gw_theme_tokens.h
	$(CC) -std=c99 -Wall -Wextra -fsyntax-only $(LVGL_CFLAGS) \
	    -I../system gw-shell.c gw_shell_state.c gw_screen_home.c \
    gw_screen_settings.c gw_screen_calendar.c gw_screen_notes.c \
    gw_screen_network.c gw_screen_apps.c gw_screen_hub.c gw_hub.c gw_screen_lock.c gw_lock_config.c gw_shell_date.c gw_todo_store.c

test-epdc-policy: test-epdc-policy.c gw_epdc_policy.c gw_epdc_policy.h
	$(HOSTCC) -std=c99 -Wall -Wextra -O2 -I. -o $@ test-epdc-policy.c gw_epdc_policy.c
	./test-epdc-policy

test-epdc-batch: test-epdc-batch.c lv_port_epdc.h
	$(HOSTCC) -std=c99 -Wall -Wextra -O2 -I$(dir $(LVGL_DIR)) \
	    -DLV_CONF_SKIP -DLV_COLOR_DEPTH=8 -DLV_MEM_SIZE=262144 \
	    -DGW_UI_EPDC_ACCEPTED=1 -DGW_ENABLE_HW_TRIAL=0 \
	    -DGW_UI_EPDC_UNIT_TEST=1 -DGW_UI_PERF=0 \
	    -o test-epdc-batch test-epdc-batch.c gw_epdc_policy.c
	./test-epdc-batch

test-y8-lut: test-y8-lut.c lv_port_epdc.h
	$(HOSTCC) -std=c99 -Wall -Wextra -O2 -I$(dir $(LVGL_DIR)) \
	    -DLV_CONF_SKIP -DLV_COLOR_DEPTH=8 -DLV_MEM_SIZE=262144 \
	    -DGW_UI_EPDC_ACCEPTED=1 -DGW_ENABLE_HW_TRIAL=0 \
	    -DGW_UI_EPDC_UNIT_TEST=1 -DGW_UI_PERF=0 \
	    -o test-y8-lut test-y8-lut.c gw_epdc_policy.c
	./test-y8-lut

test-paint-profile: test-paint-profile.c gw_paint_profile.c gw_paint_profile.h
	$(HOSTCC) -std=c99 -Wall -Wextra -O2 -I. -o $@ test-paint-profile.c gw_paint_profile.c
	./test-paint-profile

test-paint-rules: test-paint-rules.c gw_paint_rules.c gw_paint_rules.h gw_paint_profile.c
	$(HOSTCC) -std=c99 -Wall -Wextra -O2 -I. -o $@ test-paint-rules.c gw_paint_rules.c gw_paint_profile.c
	./test-paint-rules

test-lock-config: test-lock-config.c gw_lock_config.c gw_lock_config.h
	$(HOSTCC) -std=c99 -Wall -Wextra -O2 -I. -o $@ test-lock-config.c gw_lock_config.c
	./test-lock-config

clean:
	rm -f gw-shell gw-shell-debug host-sim test-epdc-policy test-epdc-batch test-y8-lut test-paint-profile test-paint-rules test-lock-config
