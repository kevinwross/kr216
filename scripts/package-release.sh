#!/bin/sh
# Stages a self-contained Linux kr216 toolchain and writes a tar.gz archive.
set -eu
root=$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd)
compiler=${KRCOMPILER:-"$root/../krcompiler"}
preset=${PRESET:-linux-gcc-debug}
out="$root/dist/kr216-toolchain-linux"
rm -rf "$out"
mkdir -p "$out/bin" "$out/runtime" "$out/examples"

host="$compiler/build/$preset/host/bin/Debug"
cp "$host/krc" "$host/krld" "$out/bin/"
cp "$root/build/$preset/tools/kr216-emu/kr216-emu" "$out/bin/"
cp "$root/build/$preset/tools/kr216-dis/kr216-dis" "$out/bin/"
cp "$root/build/$preset/tools/kr216-bench/kr216-bench" "$out/bin/"

runtime="$compiler/targets/kr216/runtime"
cp "$runtime/crt0.s" "$runtime/syscalls.s" "$runtime/eh.s" "$runtime/libc.c" "$out/runtime/"
cp -R "$compiler/build/$preset/stage/sysroot" "$out/sysroot"
cp "$root"/examples/*.cpp "$root/examples/build.sh" "$out/examples/"
cp "$root/README.md" "$out/README.md"
{
    echo "kr216 toolchain"
    echo "version 0.1.1"
    echo "krcompiler $(git -C "$compiler" rev-parse --short HEAD)"
    echo "kr216 $(git -C "$root" rev-parse --short HEAD)"
} >"$out/VERSION.txt"

archive="$root/dist/kr216-toolchain-linux.tar.gz"
rm -f "$archive"
tar -czf "$archive" -C "$root/dist" kr216-toolchain-linux
(cd "$root/dist" && sha256sum "$(basename "$archive")") >"$root/dist/SHA256SUMS-linux.txt"
echo "staged $out"
echo "archive $archive"
cat "$root/dist/SHA256SUMS-linux.txt"

