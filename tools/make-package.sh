#!/bin/sh
# Builds the R Telegram hpkg in a container with Haiku's host package tools
# (the arm64 build tree's), from a binary that already has its resources.
#
#   sh tools/make-package.sh arm64|x86 <binary> <version-revision> <out-dir>
#
# arm64 makes rtelegram (vendor RENKU, the arm64 repository's); x86 makes
# rtelegram_x86 for the x86_gcc2 hybrid's secondary architecture (vendor
# rainygirl).
set -e
ARCH="$1"; BIN="$2"; VER="$3"; OUT="$4"
HERE="$(cd "$(dirname "$0")/.." && pwd)"
GEN="${GEN:-/root/renku-arm64-work/generated.arm64}"
TOOLS="$GEN/objects/linux/arm64/release/tools"
export LD_LIBRARY_PATH="${LD_LIBRARY_PATH:-/root/rretina-lib}"
case "$ARCH" in
arm64)
	NAME=rtelegram; PKGARCH=arm64; VENDOR=RENKU
	PACKAGER="RENKU <renku@localhost>"
	REQUIRES="haiku
	lib:libssl >= 3
	lib:libcrypto >= 3"
	;;
x86)
	NAME=rtelegram_x86; PKGARCH=x86_gcc2; VENDOR=rainygirl
	PACKAGER="rainygirl <rainygirl@gmail.com>"
	REQUIRES="haiku_x86
	lib:libssl_x86 >= 3
	lib:libcrypto_x86 >= 3"
	;;
*) echo "usage: $0 arm64|x86 <binary> <version-revision> <out-dir>" >&2; exit 1 ;;
esac
SHORT="${VER%-*}"
W=$(mktemp -d)
mkdir -p "$W/apps" "$W/data/deskbar/menu/Applications" \
	"$W/documentation/packages/$NAME"
cat "$BIN" > "$W/apps/R Telegram"
chmod 755 "$W/apps/R Telegram"
# Tracker and Deskbar read the icon and signature from attributes.
"$TOOLS/addattr/addattr" -t mime BEOS:TYPE application/x-vnd.Be-elfexecutable "$W/apps/R Telegram"
"$TOOLS/addattr/addattr" -t mime BEOS:APP_SIG application/x-vnd.rainygirl-RTelegram "$W/apps/R Telegram"
"$TOOLS/addattr/addattr" -f "$HERE/assets/RTelegram.hvif" -c VICN BEOS:ICON "$W/apps/R Telegram"
ln -s "../../../../apps/R Telegram" "$W/data/deskbar/menu/Applications/R Telegram"
# Haiku ships no Boost licence text, so the package carries it (TDLib's).
mkdir -p "$W/data/licenses"
cp "$HERE/assets/licenses/Boost v1.0" "$W/data/licenses/"
cp "$HERE/README.md" "$HERE/README.ko.md" "$HERE/LICENSE" "$HERE/NOTICE" \
	"$W/documentation/packages/$NAME/"
cat > "$W/.PackageInfo" <<PI
name		$NAME
version		$VER
architecture	$PKGARCH
summary		"Native Telegram client for Haiku, no Qt"
description	"R Telegram is a Telegram client written against the Haiku API, on Telegram's own TDLib. Chat list, messages with photos, sending text and notifications. It uses your own api_id and api_hash from my.telegram.org; the README says how to get them."
packager	"$PACKAGER"
vendor		"$VENDOR"
copyrights {
	"2026 rainygirl"
	"2014-2026 Aliaksei Levin, Arseny Smirnov (TDLib)"
}
licenses {
	"MIT"
	"Boost v1.0"
}
provides {
	$NAME = $SHORT
	app:R_Telegram = $SHORT
}
requires {
	$REQUIRES
}
urls {
	"https://github.com/rainygirl/renkuos-rtelegram"
}
PI
mkdir -p "$OUT"
( cd "$W" && "$TOOLS/package/package" create -q -z zlib "$OUT/$NAME-$VER-$PKGARCH.hpkg" )
"$TOOLS/package/package" list -a "$OUT/$NAME-$VER-$PKGARCH.hpkg" | grep -A3 "R Telegram" | head -8
rm -rf "$W"
