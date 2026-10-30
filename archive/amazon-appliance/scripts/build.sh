#!/bin/sh
set -eu
cd "$(dirname "$0")/.."
export CGO_ENABLED=0 GOTOOLCHAIN=local GOPROXY=off GOSUMDB=off
go vet -buildvcs=false ./...
mkdir -p dist
go build -buildvcs=false -trimpath -o dist/gle-kindle-native .
GOOS=linux GOARCH=arm GOARM=7 go build -buildvcs=false -trimpath -o dist/gle-kindle-linux-armv7 .
GOOS=linux GOARCH=arm GOARM=5 go build -buildvcs=false -trimpath -o dist/gle-kindle-linux-armv5 .
go version > dist/BUILDINFO
cd dist
sha256sum gle-kindle-* > SHA256SUMS
echo 'Built locally. ARM binaries require Linux 3.2 or later; device compatibility is not verified.'
