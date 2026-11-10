#!/bin/sh
# Builds TDLib (https://github.com/tdlib/td) as static libraries for Haiku,
# inside a container that holds a Haiku cross toolchain. Leaves
# /root/tdlib/<arch>/build with the libtd*.a the app links.
#
#   arm64: the haiku-builder container (a Haiku arm64 build tree)
#   x86:   the container with the Haiku x86 (gcc 13) cross tools of the
#          x86_gcc2 hybrid's secondary architecture
#
#   docker cp tools/build-tdlib.sh <container>:/tmp/
#   docker cp tools/patch-tdlib.py <container>:/tmp/
#   docker exec <container> sh /tmp/build-tdlib.sh arm64|x86
#
# TDLib generates its TL schema sources with tools it builds first, so a
# cross build is two passes: a native build that only generates the sources,
# then the Haiku build with CMAKE_CROSSCOMPILING set. A source tree that
# already has the generated sources skips the first.
#
# Env: CMAKE (cmake binary), JOBS (parallel jobs; TDLib's largest files take
# 3-4 GB each to compile, so the default is the CPU count halved).
set -e
ARCH="${1:-arm64}"
TD_REF="${TD_REF:-master}"
CMAKE="${CMAKE:-cmake}"
JOBS="${JOBS:-$(( $(nproc) / 2 ))}"
TOP=/root/tdlib
SRC=$TOP/td
BUILD=$TOP/$ARCH/build

case "$ARCH" in
arm64)
	GEN="${GEN:-/root/renku-arm64-work/generated.arm64}"
	TRIPLE=aarch64-unknown-haiku
	TOOLS="$GEN/cross-tools-arm64/bin"
	SYSROOT=$TOP/$ARCH/sysroot
	DEV="$SYSROOT/boot/system/develop"
	INCLUDE="$DEV/headers"
	LIBS="$DEV/lib"
	EXTRA=""
	;;
x86)
	X86="${X86:-/build/generated.x86only/cross-tools-x86}"
	TRIPLE=i586-pc-haiku
	TOOLS="$X86/bin"
	SYSROOT=""
	# The toolchain finds Haiku's headers and libraries itself; OpenSSL's
	# headers sit in the secondary architecture's directory.
	INCLUDE="$X86/$TRIPLE/headers/x86"
	LIBS="$X86/$TRIPLE/lib"
	EXTRA="-I$INCLUDE"
	;;
*)
	echo "usage: $0 arm64|x86" >&2; exit 1 ;;
esac

mkdir -p "$TOP"
if [ ! -d "$SRC" ]; then
	git clone --depth 1 --branch "$TD_REF" https://github.com/tdlib/td.git "$SRC"
fi
# Haiku support (platform detection, sqlite's mmap header).
python3 "$(dirname "$0")/patch-tdlib.py" "$SRC"

# The arm64 sysroot: haiku_devel's headers, the haiku package's libraries,
# gcc_syslibs, and OpenSSL/zlib extracted from their packages.
if [ "$ARCH" = arm64 ] && [ ! -f "$SYSROOT/.done" ]; then
	P="$GEN/objects/haiku/arm64/packaging/packages_build/minimum"
	DL="$GEN/download"
	PKG="$GEN/objects/linux/arm64/release/tools/package/package"
	SYSLIBS="$(ls -d "$GEN"/build_packages/gcc_syslibs-*-arm64/lib | head -1)"
	export LD_LIBRARY_PATH="${LD_LIBRARY_PATH:-/root/rretina-lib}"
	rm -rf "$SYSROOT"; mkdir -p "$SYSROOT/boot/system"
	cp -a "$P/hpkg_-haiku_devel.hpkg/contents/develop" "$SYSROOT/boot/system/"
	cp -a "$P/hpkg_-haiku.hpkg/contents/lib" "$SYSROOT/boot/system/"
	cp -a "$SYSLIBS"/. "$SYSROOT/boot/system/lib/"
	for p in openssl3 openssl3_devel zlib zlib_devel; do
		f=$(ls "$DL"/${p}-[0-9]*-$ARCH.hpkg | head -1)
		$PKG extract -C "$SYSROOT/boot/system" "$f"
	done
	rm -f "$SYSROOT/boot/system/.PackageInfo"
	# gcc_syslibs ships only the versioned libraries; the linker looks for
	# the development names in develop/lib.
	for l in libstdc++ libgcc_s; do
		v=$(ls "$SYSROOT/boot/system/lib/$l.so."* 2>/dev/null | head -1)
		[ -n "$v" ] && ln -sfn "../../lib/$(basename "$v")" "$DEV/lib/$l.so"
	done
	touch "$SYSROOT/.done"
fi

# Pass 1: generate the TL sources natively.
if [ ! -f "$SRC/td/generate/auto/td/telegram/td_api.h" ]; then
	mkdir -p "$TOP/native" && cd "$TOP/native"
	$CMAKE -DCMAKE_BUILD_TYPE=Release -DTD_GENERATE_SOURCE_FILES=ON "$SRC" >/dev/null
	$CMAKE --build . -j "$JOBS" >/dev/null
fi

# Pass 2: the Haiku build. A Haiku executable is a shared object (ET_DYN),
# so everything is -fPIC; CMake adds -fPIE after any flags it is given, and
# a wrapper that puts -fPIC last is the reliable way to win.
mkdir -p "$TOP/$ARCH/bin"
for t in gcc g++; do
	printf '#!/bin/sh\nexec %s %s "$@" -fPIC\n' "$TOOLS/$TRIPLE-$t" "$EXTRA" \
		> "$TOP/$ARCH/bin/$t"
	chmod +x "$TOP/$ARCH/bin/$t"
done
{
	echo "set(CMAKE_SYSTEM_NAME Haiku)"
	echo "set(CMAKE_SYSTEM_PROCESSOR $ARCH)"
	[ -n "$SYSROOT" ] && echo "set(CMAKE_SYSROOT $SYSROOT)"
	echo "set(CMAKE_C_COMPILER $TOP/$ARCH/bin/gcc)"
	echo "set(CMAKE_CXX_COMPILER $TOP/$ARCH/bin/g++)"
	echo "set(CMAKE_AR $TOOLS/$TRIPLE-ar)"
	echo "set(CMAKE_RANLIB $TOOLS/$TRIPLE-ranlib)"
	echo "set(CMAKE_FIND_ROOT_PATH_MODE_PROGRAM NEVER)"
} > "$TOP/$ARCH/toolchain.cmake"
mkdir -p "$BUILD" && cd "$BUILD"
$CMAKE -DCMAKE_TOOLCHAIN_FILE="$TOP/$ARCH/toolchain.cmake" \
	-DCMAKE_CROSSCOMPILING=ON -DCMAKE_BUILD_TYPE=MinSizeRel \
	-DOPENSSL_INCLUDE_DIR="$INCLUDE" \
	-DOPENSSL_SSL_LIBRARY="$LIBS/libssl.so" \
	-DOPENSSL_CRYPTO_LIBRARY="$LIBS/libcrypto.so" \
	-DZLIB_INCLUDE_DIR="$INCLUDE" \
	-DZLIB_LIBRARY="$LIBS/libz.so" \
	"$SRC"
$CMAKE --build . --target tdclient -j "$JOBS"
echo "built: $(find "$BUILD" -name '*.a' | wc -l) static libraries in $BUILD"
