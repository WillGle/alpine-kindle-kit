#!/bin/sh
# SPDX-License-Identifier: GPL-3.0-or-later
# Prepare only the existing PW3 framebuffer and touch modules for the shell.
set -eu
. /etc/gleworks-device-prepare.sh
create_core_devices
load_touch_modules
create_input_nodes
