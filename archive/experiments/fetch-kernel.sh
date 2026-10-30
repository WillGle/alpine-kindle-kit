#!/bin/sh
# Fetch a pinned public audit baseline on the host, not a deployable PW3 port.
set -eu
base=$(CDPATH= cd -- "$(dirname -- "$0")" && pwd)
source_dir="$base/linux-6.18"
partial="$source_dir.partial"
expected=7d0a66e4bb9081d75c82ec4957c50034cb0ea449
for path in "$source_dir" "$partial" "$source_dir.resolved-commit" "$source_dir.source.sha256"; do
    if [ -e "$path" ] || [ -L "$path" ]; then
        echo "Retain existing acquisition for review: $path" >&2
        exit 1
    fi
done
export GIT_TERMINAL_PROMPT=0
git -c core.hooksPath=/dev/null clone --depth 1 --branch v6.18 \
    https://github.com/torvalds/linux.git "$partial"
resolved=$(git -C "$partial" rev-parse --verify 'HEAD^{commit}')
[ "$resolved" = "$expected" ] || {
    echo 'Unexpected source commit; retain partial checkout for review.' >&2
    exit 1
}
printf '%s\n' "$resolved" > "$source_dir.resolved-commit"
git -C "$partial" archive --format=tar HEAD > "$partial/source-snapshot.tar"
(cd "$partial" && sha256sum source-snapshot.tar) > "$source_dir.source.sha256"
mv "$partial" "$source_dir"
echo 'Public audit baseline acquired; PW3 drivers/device tree remain unvalidated.'
echo 'Snapshot hash records provenance, not signature trust or hardware support.'
echo 'No build, installer or device command executed.'
