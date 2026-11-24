#!/bin/sh
# Build and run one example: ./build.sh [virtual.cpp|exceptions.cpp|containers.cpp]
set -eu
root=$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd)
source=${1:-virtual.cpp}
if [ ! -f "$root/examples/$source" ]; then
    echo "unknown example: $source" >&2
    exit 2
fi
work="$root/examples/build"
mkdir -p "$work"

if [ -x "$root/bin/krc" ]; then
    krc="$root/bin/krc"
    krld="$root/bin/krld"
    emu="$root/bin/kr216-emu"
    runtime="$root/runtime"
    sysroot="$root/sysroot"
else
    compiler=${KRCOMPILER:-"$root/../krcompiler"}
    krc="$compiler/build/linux-gcc-debug/host/bin/Debug/krc"
    krld="$compiler/build/linux-gcc-debug/host/bin/Debug/krld"
    emu="$root/build/linux-gcc-debug/tools/kr216-emu/kr216-emu"
    runtime="$compiler/targets/kr216/runtime"
    sysroot="$compiler/build/linux-gcc-debug/stage/sysroot"
fi
if [ ! -x "$krc" ]; then
    echo "krc not found at $krc" >&2
    echo "Build krcompiler first, or set KRCOMPILER to its checkout." >&2
    exit 1
fi

"$krc" -c --target=kr216 "$runtime/crt0.s" -o "$work/crt0.kro"
"$krc" -c --target=kr216 "$runtime/syscalls.s" -o "$work/syscalls.kro"
"$krc" -c --target=kr216 "$runtime/eh.s" -o "$work/eh.kro"
"$krc" -c --target=kr216 "$runtime/libc.c" -o "$work/libc.kro"
"$krc" -c --target=kr216 "$root/examples/$source" --sysroot "$sysroot" -o "$work/example.kro"
"$krld" --gc-sections "$work/crt0.kro" "$work/syscalls.kro" "$work/eh.kro" \
    "$work/libc.kro" "$work/example.kro" -o "$work/example.kr216"
"$emu" "$work/example.kr216"
