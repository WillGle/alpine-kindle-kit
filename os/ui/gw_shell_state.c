/* SPDX-License-Identifier: GPL-3.0-or-later */
#ifndef _GNU_SOURCE
#define _GNU_SOURCE
#endif
#include <fcntl.h>
#include <stdio.h>
#include <time.h>
#include <unistd.h>
#include <lvgl/lvgl.h>
#include "lv_port_epdc.h"
#define GW_SHELL_STATE_IMPLEMENTATION
#include "gw_shell_state.h"
