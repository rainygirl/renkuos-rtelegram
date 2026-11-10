#!/bin/sh
# Cross-compile R Telegram on a workstation, producing dist/<arch>/RTelegram
# with its resources. TDLib must be built first (tools/build-tdlib.sh).
#
#   tools/cross-build.sh arm64
set -e
ARCH="${1:-arm64}"
HERE="$(cd "$(dirname "$0")/.." && pwd)"
CONTAINER="${CONTAINER:-haiku-builder}"
WORK="/tmp/rtelegram-cross"
docker exec "$CONTAINER" rm -rf "$WORK"
docker exec "$CONTAINER" mkdir -p "$WORK/tools"
docker cp "$HERE/src" "$CONTAINER:$WORK/src"
docker cp "$HERE/RTelegram.rdef" "$CONTAINER:$WORK/RTelegram.rdef"
docker cp "$HERE/tools/cross-compile.sh" "$CONTAINER:$WORK/tools/cross-compile.sh"
docker cp "$HERE/tools/attach-resources.sh" "$CONTAINER:$WORK/tools/attach-resources.sh"
docker exec -w "$WORK" "$CONTAINER" sh tools/cross-compile.sh "$ARCH"
mkdir -p "$HERE/dist/$ARCH"
docker cp "$CONTAINER:$WORK/RTelegram" "$HERE/dist/$ARCH/RTelegram"
file "$HERE/dist/$ARCH/RTelegram"
