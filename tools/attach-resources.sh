#!/bin/sh
# Attaches RTelegram.rdef's resources (signature, version, icon) to a built
# binary with the host rc and xres of a Haiku build tree. Haiku's resource
# format is the same for 32- and 64-bit ELF, so the arm64 tree's tools serve
# the x86 binary as well.
#
#   sh tools/attach-resources.sh <binary> <rdef>
set -e
BIN="$1"
RDEF="$2"
GEN="${GEN:-/root/renku-arm64-work/generated.arm64}"
TOOLS="$GEN/objects/linux/arm64/release/tools"
export LD_LIBRARY_PATH="${LD_LIBRARY_PATH:-/root/rretina-lib}"
"$TOOLS/rc/rc" -o "$BIN.rsrc" "$RDEF"
"$TOOLS/xres" -o "$BIN" "$BIN.rsrc"
rm -f "$BIN.rsrc"
