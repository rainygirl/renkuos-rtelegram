#!/bin/sh
# Runs inside the build container, in a directory holding src/ and
# RTelegram.rdef, after tools/build-tdlib.sh has built TDLib for the arch.
# Leaves ./RTelegram; resources are attached when the container has Haiku's
# host rc and xres (the arm64 build tree does; for x86 run
# tools/attach-resources.sh in that container afterwards).
#
#   sh tools/cross-compile.sh arm64|x86
set -e
ARCH="${1:-arm64}"
TD=/root/tdlib
SRC=$TD/td
B=$TD/$ARCH/build
case "$ARCH" in
arm64)
	GEN="${GEN:-/root/renku-arm64-work/generated.arm64}"
	CXX="$GEN/cross-tools-arm64/bin/aarch64-unknown-haiku-g++ --sysroot=$TD/$ARCH/sysroot"
	STRIP="$GEN/cross-tools-arm64/bin/aarch64-unknown-haiku-strip"
	EXTRA=""
	;;
x86)
	X86="${X86:-/build/generated.x86only/cross-tools-x86}"
	CXX="$X86/bin/i586-pc-haiku-g++"
	STRIP="$X86/bin/i586-pc-haiku-strip"
	EXTRA="-I$X86/i586-pc-haiku/headers/x86"
	;;
*) echo "usage: $0 arm64|x86" >&2; exit 1 ;;
esac

CXXFLAGS="-O2 -std=c++17 -fPIC -Wall -Wno-multichar -Wno-unused-parameter
	$EXTRA -I$SRC -I$SRC/td/generate/auto -I$SRC/tdutils -I$B/tdutils"
for f in src/*.cpp; do
	echo "  $f"
	$CXX $CXXFLAGS -c "$f" -o "${f%.cpp}.o" &
done
wait
TDLIBS="$B/libtdclient.a $B/libtdcore.a $B/libtdapi.a $B/libtdmtproto.a
	$B/tde2e/libtde2e.a $B/tdnet/libtdnet.a $B/tddb/libtddb.a
	$B/sqlite/libtdsqlite.a $B/tdactor/libtdactor.a $B/tdutils/libtdutils.a"
$CXX -o RTelegram src/*.o \
	-Wl,--start-group $TDLIBS -Wl,--end-group \
	-lssl -lcrypto -lz -lbe -ltranslation -lnetwork -lstdc++ \
	-Xlinker -soname=_APP_
# Before the resources: they sit after the ELF data, and strip drops them.
$STRIP RTelegram
if [ -n "$GEN" ] && [ -x "$GEN/objects/linux/arm64/release/tools/rc/rc" ]; then
	sh "$(dirname "$0")/attach-resources.sh" RTelegram RTelegram.rdef
fi
ls -l RTelegram
