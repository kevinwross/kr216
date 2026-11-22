# src216 — guest code

Everything under this directory is compiled **for kr216**, by `krc` configured with
`--target=kr216` and the target sysroot. It is never compiled by the host toolchain and
never linked into the emulator, the disassembler, the loader or the test harness.

## What belongs here

- Hand-written assembly programs and their sources, the ones the emulator's conformance
  suites run and whose expected output the host tests check.
- C programs written to exercise the ABI, the frame instructions and (from M5) exceptions.
- Guest-side test scaffolding: a guest program's own `main`, and any helper it needs.

## What does not belong here

- The emulator, FPU, decoder, disassembler, image loader, debug interface, CLI tools or any
  host test — those are `src/` and `tests/`, compiled by the host toolchain.
- The kr216 runtime: `crt0`, libc and the exception runtime live in
  `../../krcompiler/targets/kr216/` (D32). A guest program here links against that runtime;
  it does not define or copy it.
- Host headers. A `#include` of anything under `src/` or `include/` is a boundary violation
  and the boundary check fails on it.

## How it is built

A host CMake target invokes `krc` for each guest source and writes the object or image into
the build tree; CTest then loads those images into the emulator. The exact command is
recorded in `../kr216-impl.md` once the kr216 backend exists — until then the guest build
target fails with a message naming the missing backend rather than silently doing nothing.
