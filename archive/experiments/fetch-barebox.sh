#!/bin/sh
# Fetch source on the host only; never touches the connected Kindle.
set -eu
base=$(CDPATH= cd -- "$(dirname -- "$0")" && pwd)
source_dir="$base/barebox-2026.10.0"
partial="$source_dir.partial"
if [ -e "$source_dir" ] || [ -L "$source_dir" ] || [ -e "$partial" ] || [ -L "$partial" ]; then
    echo 'Source or partial download already exists; retain it for review.' >&2
    exit 1
fi
export GIT_TERMINAL_PROMPT=0
git -c core.hooksPath=/dev/null clone --depth 1 --branch v2026.10.0 \
    https://github.com/barebox/barebox.git "$partial"
git -C "$partial" rev-parse --verify 'HEAD^{commit}' > "$partial.resolved-commit"
[ -f "$partial/arch/arm/configs/kindle-gen-6-7_defconfig" ] || {
    echo 'Expected Kindle configuration absent; source retained for review.' >&2
    exit 1
}
mv "$partial" "$source_dir"
mv "$partial.resolved-commit" "$source_dir.resolved-commit"
printf 'Source ready for review: %s\n' "$source_dir"
echo 'No build, installation or device writes performed.'
