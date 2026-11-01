#!/bin/sh
# SPDX-License-Identifier: GPL-3.0-or-later
# Standard prerequisite gate before any build/test/deploy: x86-64 Linux host.
set -eu
REPO=$(CDPATH= cd -- "$(dirname "$0")/.." && pwd)

[ "$(uname -s)" = Linux ] || { echo "ERROR: need Linux (got $(uname -s))" >&2; exit 1; }
case "$(uname -m)" in
    x86_64|amd64) ;;
    *) echo "ERROR: need Linux x86-64/AMD64 (got $(uname -m))" >&2; exit 1 ;;
esac
for tool in python3 make cc; do
    command -v "$tool" >/dev/null 2>&1 ||
        { echo "ERROR: missing host tool: $tool" >&2; exit 1; }
done
echo "host: Linux $(uname -m) OK"
exec make -C "$REPO" check CC="${CC:-cc}"
