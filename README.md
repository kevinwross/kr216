# kr216

kr216 is a 16-bit little-endian virtual CPU designed as a first-class C++ target. It has 16
general registers, uniform addressing with a variable-length encoding (1–6 bytes per
instruction), an IEEE-754 binary64 FPU, and a 64 KiB address space. This repository owns the
ISA, the reference emulator, the disassembler, the image loader, the KDBG debug payload and the
integration tests that prove the compiler and the machine agree. The compiler and runtime live
in [krcompiler](../krcompiler); the debugger is [krdbg](../krdbg).

The machine runs C++ classes, single inheritance and virtual dispatch, `new`/`delete`,
templates, `double`, RTTI, exceptions with destructor cleanup, and a `std::string`/`std::vector`
subset. The benchmark corpus measures **2.4286 bytes per static instruction**, below the 2.5
target.

## Prerequisites

- Windows: Visual Studio 2022 or later with the x64 C++ workload, CMake 3.31+ and Ninja. The
  build entry point finds them; it does not require a Developer PowerShell.
- Linux: GCC 12+ or Clang 15+, CMake 3.31+ and Ninja. The presets assume they are on `PATH`.
- Python is not required. The only third-party components are in `krcompiler/third_party`.

The compiler is a separate checkout. Keep the two repositories as siblings:

```text
repos/
  krcompiler/
  kr216/
  krdbg/        optional, for debugging
```

`krc` and `krld` are discovered under `../krcompiler/build/<preset>/host/bin/Debug/`. Build
krcompiler first. Its build is:

```sh
# Linux (WSL or BitWhiz)
cd ../krcompiler
cmake --preset linux-gcc-debug
cmake --build --preset linux-gcc-debug
```

```powershell
# Windows
cd ..\krcompiler
cmake --preset windows-msvc-debug
cmake --build --preset windows-msvc-debug
```

## Build and test

```powershell
# Windows: doctor, configure, build, ctest
build.cmd
```

```sh
# Linux
cmake --preset linux-gcc-debug
cmake --build --preset linux-gcc-debug
ctest --preset linux-gcc-debug
```

`build.cmd arm64` does the same on the ARM64 machine over SSH. The suite includes the
instruction/encoding/frame/trap/FP/fuzz tests, guest programs compiled by `krc` and run on the
emulator, the exception and container fixtures, and the density benchmark. All tests currently
pass on Windows, WSL and BitWhiz.

## Build and run a C++ program

The examples are self-contained. Each script compiles the runtime, compiles the example with
the kr216 sysroot, links a `.kr216` image and runs it on the emulator:

```powershell
examples\build.cmd virtual.cpp
examples\build.cmd exceptions.cpp
examples\build.cmd containers.cpp
```

```sh
sh examples/build.sh virtual.cpp
sh examples/build.sh exceptions.cpp
sh examples/build.sh containers.cpp
```

`virtual.cpp` defines an abstract `Shape` and a `Square` override returning `double`, calls the
override through a base pointer and prints the area scaled by 100. `exceptions.cpp` throws a
class inside a `try`, verifies that a local destructor ran during unwinding, and catches it by
reference. `containers.cpp` builds a `std::string` and a `std::vector<int>`.

### Packaged release

`release.cmd` stages `dist/kr216-toolchain/` (binaries, runtime, sysroot, examples and this
README) and writes `dist/kr216-toolchain.zip` plus `dist/SHA256SUMS-windows.txt`. On Linux,
`scripts/package-release.sh` writes the `.tar.gz` and `SHA256SUMS-linux.txt`. The extracted tree
is self-contained: run `examples\build.cmd virtual.cpp` from it without either source checkout.
`VERSION.txt` records the version and the compiler/emulator revisions; the release tag is
`kr216-v0.1.2`.

By hand, the steps are:

```sh
krc -c --target=kr216 runtime/crt0.s -o crt0.kro
krc -c --target=kr216 runtime/syscalls.s -o syscalls.kro
krc -c --target=kr216 runtime/eh.s -o eh.kro
krc -c --target=kr216 runtime/libc.c -o libc.kro
krc -c --target=kr216 hello.cpp -o hello.kro
krld --gc-sections crt0.kro syscalls.kro eh.kro libc.kro hello.kro -o hello.kr216
kr216-emu hello.kr216
```

Each example prints its result and returns 42 from `main`. `kr216-bench` reports the guest exit
code together with code size and instruction counts.

## Debugging

The `krdbg` adapter loads a `.kr216` image, breaks on functions and source lines, shows the kr216
registers, walks the variable-length instruction stream and exposes KDBG locals and frames.
From a krdbg build with `-DKRDBG_KR216_ADAPTER=ON`:

```sh
krdbg --kr216 examples/build/example.kr216
```

## Layout

| Path | Contents |
|---|---|
| `src/` | host code: memory, decoder, machine, FPU, disassembler, image loader, debug target |
| `src216/` | guest C/C++ programs used by the integration tests |
| `include/kr216/` | public host headers |
| `tests/` | CTest harness, encoder/semantics/frame/FP/fuzz suites and guest-program gates |
| `examples/` | small C++ programs and the build scripts above |
| `benchmarks/` | the fixed density corpus (string copy, printf, container, recursion, FP loop) |
| `tools/` | `kr216-emu`, `kr216-dis` and `kr216-bench` |
| `cmake/` | guest build and density-benchmark drivers |

`kr216-isa.md` is the encoding and semantics reference; `docs/abi-kr216.md` in krcompiler is the
ABI. `kr216-plan.md` records the architecture decisions and milestones; `kr216-impl.md` tracks
their state and the verification gates.


