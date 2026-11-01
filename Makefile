# SPDX-License-Identifier: GPL-3.0-or-later
# Repository-wide checks. No target writes to a device or opens /dev/fb0.
#
#   make check          full host gate
#   make test           system/installer host tests only
#   make public-source  deterministic source-package roundtrip

PYTHON ?= python3
CC ?= cc

.PHONY: check test public-source vm-gate clean

check:
	$(PYTHON) tools/check.py

test:
	$(MAKE) -C os/system test CC="$(CC)" HOSTCC="$(CC)"
	$(PYTHON) -O -m unittest discover -s tools/tests

public-source:
	$(PYTHON) tools/package/public-source.py --check

vm-gate:
	./vm/check-host.sh
	./vm/qemu-arm/run.sh

clean:
	$(MAKE) -C os/system clean
	$(MAKE) -C os/ui clean 2>/dev/null || true
	find . -name '__pycache__' -type d -prune -exec rm -rf {} +
