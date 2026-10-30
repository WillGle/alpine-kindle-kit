#!/bin/sh
# Obtain the published PW3 source bundle for review; never extract or execute it.
set -eu
base=$(CDPATH= cd -- "$(dirname -- "$0")" && pwd)
name=Kindle_src_5.16.2.1.1_4097470002.tar.gz
dest="$base/$name"
for path in "$dest" "$dest.partial" "$dest.sha256" "$dest.members.txt"; do
    if [ -e "$path" ] || [ -L "$path" ]; then
        echo "Retain existing input for review: $path" >&2
        exit 1
    fi
done
curl --fail --location --proto '=https' --proto-redir '=https' \
    --connect-timeout 20 --max-filesize 2147483648 \
    "https://kindledownloads.s3.amazonaws.com/$name" -o "$dest.partial"
gzip -t "$dest.partial"
tar -tzf "$dest.partial" > "$dest.members.txt"
mv "$dest.partial" "$dest"
(cd "$base" && sha256sum "$name") > "$dest.sha256"
echo 'Published PW3 source bundle downloaded and archive integrity checked.'
echo 'Local hash is a provenance record, not a published signature verification.'
echo 'No extraction, build, module loading or device operation performed.'
