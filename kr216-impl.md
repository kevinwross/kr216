# kr216 — Implementation Tracker

**Status:** Plan approved 2026-11-21 (`kr216-plan.md` v0.11); ISA frozen at 1.0; ABI appendix accepted (`../krcompiler/docs/abi-kr216.md`, ADR 0053). **Current milestone: M2 — M1 complete 2026-11-22.**
**Design contracts:** [`kr216-plan.md`](kr216-plan.md) (scope, decisions, milestones) and [`kr216-isa.md`](kr216-isa.md) (encodings, semantics, traps, SYS).
**Compiler-side tracker:** [`../krcompiler/implementation.md`](../krcompiler/implementation.md). Work that belongs to the compiler is recorded there (D32) and only referenced here.

This file is the live tracker. It owns tasks, evidence, gates and the running log; it does not restate the design. A task is checked only when the command that proves it has run and its result is in the running log.

## 1. Rules

- **Milestone scope** is owned by `kr216-plan.md` §16; **encodings and semantics** by `kr216-isa.md`. Changing a milestone's scope is a plan revision; changing an encoding or a §15 decision is an ISA revision.
- **Check a box only with evidence.** The same change that ticks a task adds a running-log entry naming the command and its result. A box ticked without a log entry is a claim, not a record.
- **Log format.** One entry per task or event, in `## 9. Running log`, newest at the bottom:
  `- **<ISO timestamp>** — <what was done, the command that proves it, and the result.>`
  A failure is logged as loudly as a success: what was run, what it printed, and whether it was fixed or left open.
- **Timestamps come from the clock, not from memory.** Every entry's ISO timestamp is read from `local-datetime` at the moment the entry is written. Backfilled entries say so and name where the timestamp came from.
- **Gates name every configuration that ran**, with counts. "All configurations" is not a record.
- **Cross-repo work is tracked once.** The compiler-side task list lives in `../krcompiler/implementation.md`; this file adds a task only when this repository has work in it (an image to load, a vector to match, an ABI fact to execute).
- **Portable by construction.** The emulator, disassembler and loader build on Windows, Linux x64 and ARM64; a change that cannot be built on the Windows dev machine and the ARM64 host is not done.
- **Two trees, two compilers.** `src/` is host code built by CMake and the host toolchain; `src216/` is guest code built by `krc --target=kr216`. Neither includes the other's headers and the host build never compiles `src216/` (see `AGENTS.local.md`'s architecture boundaries and `src216/README.md`).

## 2. Definition of done

**Code** (a change under `src/`, `include/`, `tools/` or `tests/`):

- [ ] Builds with no warnings on the configuration(s) the change reaches: MSVC `/W4 /WX` on Windows, `-Wall -Wextra -Wpedantic -Werror` on GCC/Clang in WSL.
- [ ] `ctest` green on the host rows the change reaches, with the counts recorded in the log.
- [ ] Encoding vectors are deterministic: the same bytes decode to the same instruction, run to run.
- [ ] A change to an encoding or to instruction semantics updates `kr216-isa.md` in the same change.
- [ ] An architecture change (a §15 decision, a new encoding, a trap or SYS contract) is an ISA revision, not a code comment.
- [ ] No secrets, debug output, or unrelated edits in the diff.

**Documentation** (a change under `*.md`):

- [ ] Every section, file and decision the changed text cites exists.
- [ ] The counts the document states match its own tables.
- [ ] A review is reported with `BLOCKS` / `FIX-BEFORE` / `ERRATA` labels and evidence.

**ARM64.** A change that is portable by construction still needs the ARM64 row at least once per milestone: build and test on BitWhiz over SSH, record the revision and counts. When BitWhiz is unreachable, the gate block names the row **not verified** rather than dropping it. A row absent from CI is untested, not supported.

## 3. Platforms, toolchain and hosts

| Target | Host | Toolchain | Used for |
|---|---|---|---|
| Windows x64 | the dev machine | MSVC 19.52.37029 for x64 (Visual Studio 18 Community); CMake 4.3.1-msvc1 and Ninja ship with that Visual Studio | primary development; the Windows gate row |
| Linux x64 | WSL 2 on the dev machine | GCC and Clang, Ninja; versions recorded at the first build | the Linux gate rows, sanitizer row |
| Linux ARM64 | **BitWhiz** (Raspberry Pi 5, Debian 13), reached over SSH | GCC 14.2.0 and Clang 19.1.7; CMake 3.31.6 and Ninja 1.12.1 | the ARM64 gate row, mandatory once per milestone |

**ARM64 access.** `ssh BitWhiz` works with key authentication as user `kevinro`; the sibling repositories are at `/home/kevinro/repos` (`kr16`, `kr32`, `krcompiler`, `krdbg` and others), and this repository is cloned there at `/home/kevinro/repos/kr216` (cloned 2026-11-21, `origin` = `https://github.com/kevinwross/kr216.git`). BitWhiz reaches GitHub over **HTTPS with a stored credential**: `credential.helper=store` and one line in `~/.git-credentials` (not read; refreshed 2026-11-21 23:01), and a push dry-run authenticates. An encrypted helper is configured but not installed there (`credential.credentialstore=secretservice` with no `git-credential-manager`, no `git-credential-libsecret` and no `secret-tool`); GitHub over SSH is not configured either (`~/.ssh` holds only `authorized_keys`, no agent).

**Deploy key (pending registration).** An ed25519 key `bitwhiz-kr216-deploy` was generated on BitWhiz on 2026-11-21 and GitHub's host key was trusted on first use (ED25519). The public half must be added to `kevinwross/kr216` as a deploy key **with write access** — the owner's one web action; I cannot register it, since `gh` is not installed here and reading the stored token is not permitted. Once it is registered, the switch is `git remote set-url origin git@github.com:kevinwross/kr216.git` and a fetch test; until then `origin` stays HTTPS on BitWhiz.

Verified 2026-11-21: Linux 6.18.39+rpt-rpi-2712 aarch64, 4 cores, 16 GiB RAM, git 2.47.3.

**Installed compiler on BitWhiz.** `krcompiler` is built and installed for the other projects: source at `/home/kevinro/repos/krcompiler` (`4b96bca`), preset `linux-gcc-release`, installed with `cmake --install build/linux-gcc-release --prefix $HOME/.local`. That puts `krc` and `krld` (both 0.2.0) in `~/.local/bin`, the target sysroot in `~/.local/share/krc/sysroot`, and the CMake packages in `~/.local/lib/cmake` (`krc-toolchain`, `krc-debug`). `~/.local/bin` is on the login `PATH` (a login shell resolves `krc`), but not on the non-login `PATH` an SSH command gets, so scripts must name the absolute path or export it. The release suite is **915/915** in 81.5 s.

Recipe that works with the installed tree (verified end to end: a C program compiled, linked and ran to exit 42 under the KR32 VM):

```
krc --sysroot ~/.local/share/krc/sysroot --target=kr32 -c hello.c -o hello.kro
krld hello.kro ~/.local/share/krc/sysroot/kr32/lib/libkrcrt.kro \
     .../kr32/lib/fileio.s .../kr32/lib/eh.s .../kr32/lib/crt0.s -o hello.k32
```

`krc -o image.k32 hello.c` is not a path: for KR32 it prints "code generation is not implemented yet" and exits 2, because the image writer is the linker's. KR16 differs — it writes a `.kex` directly or a KRO object with `--emit-object`.

**Sibling tools on BitWhiz.** Verified 2026-11-21; `~/.local/bin` is the shared install prefix.

| Project | State |
|---|---|
| `kr32` | Built and **installed as 0.3.0** (`~/.local/bin/kr32`, `kr32-cxx`). Its configure now requires the `krc-toolchain` package, so it must be configured with `-DCMAKE_PREFIX_PATH=$HOME/.local`; without that it fails at `CMakeLists.txt:13`. The installed copy was stale at 0.1.0 until this rebuild. |
| `kr16` | Built and current (`ninja: no work to do`); **copied to `~/.local/bin/kr16`** as 1.0.0, because the repository has no `install()` rule. |
| `krdbg` | Built headless: `krdbg_demo`, `krdbg_settings`, `krdbg_tests`, and **3/3 tests pass**. The tree has no `krdbg` executable target yet — `krdbg_demo` is the demo shell — and no `install()` rule, so nothing was installed. |

The `kr32` install also drops `cxx-gap-signoff.md` and the `OFL-*.txt` font licences at the prefix root, which is untidy but harmless.

**This host, both platforms.** Verified 2026-11-21. Windows prefix `C:\Users\kevin\krc` (release; the older `C:\Users\kevin\krc-debug` still holds the Debug install the presets were written against); WSL prefix `~/.local`.

| Project | Windows x64 (MSVC 19.52) | WSL (GCC 15.2, CMake 4.2.3) |
|---|---|---|
| `krcompiler` | `windows-msvc-release` built and installed → `krc`/`krld` 0.2.0 | `linux-gcc-release` built and installed → 0.2.0 |
| `kr32` | Release built, installed → `kr32` 0.3.0 | Debug built, installed → 0.3.0 |
| `kr16` | Release built, copied to the prefix → `kr16` 1.0.0 | Debug built, copied → 1.0.0 |
| `krdbg` | Built `krdbg_demo`, `krdbg_settings`, `krdbg_tests`; no `krdbg` executable target and no install rule | Same three targets built; same absence |

**Three traps this pass found.**

1. **`find_package` caches the package directory.** kr32's build directory already carried `CMAKE_PREFIX_PATH=C:\Users\kevin\krc-debug` from an earlier configure, so a Release build linked the **Debug** `krc-toolchain.lib` and failed at link with `unresolved external symbol __imp__CrtDbgReport`. Changing `CMAKE_PREFIX_PATH` alone does not re-find it: override `krc-toolchain_DIR` (`-Dkrc-toolchain_DIR=C:/Users/kevin/krc/lib/cmake/krc-toolchain`) or delete the build directory. The configuration of the consumer must match the configuration of the installed `krc-toolchain`, which is a Windows-only consequence of the debug CRT.
2. **The Visual Studio generator needs `--config`.** `build/windows-msvc` is multi-config and defaults to Debug; `--install` defaults to Release, so installing after a default build fails with a missing `Release/kr32.exe`. Both commands need the same explicit `--config`.
3. **WSL has no clang**, so `linux-clang-debug` and `linux-sanitize` cannot run there yet — only the GCC rows. And `~/.local` is not searched by CMake on Linux, so consumers need `-DCMAKE_PREFIX_PATH=$HOME/.local`, exactly as the Windows presets need their prefix.

**Discovered on 2026-11-21, on the Windows dev machine** (command and output in the running log):

- `vswhere.exe` reports the Visual Studio installation at `C:\Program Files\Microsoft Visual Studio\18\Community`, product version 18.12.1.
- `cl` is 19.52.37029 for x64 once `VC\Auxiliary\Build\vcvars64.bat` has run.
- CMake is **not on `PATH`**. The usable one is the Visual Studio copy,
  `...\Common7\IDE\CommonExtensions\Microsoft\CMake\CMake\bin\cmake.exe`, version 4.3.1-msvc1, with
  `...\Common7\IDE\CommonExtensions\Microsoft\CMake\Ninja\ninja.exe` beside it.
- That CMake's newest Visual Studio generator is **`Visual Studio 17 2022`**, so it cannot generate a project for the installed Visual Studio 18. The Windows build therefore runs **`Ninja` inside the `vcvars64` environment**; the build entry point must set that environment itself rather than assume a developer prompt.

**Build entry point.** `build.cmd [preset|arm64]` runs the doctor, resolves the toolchain
(`vswhere` → `vcvars64.bat` → the Visual Studio CMake and ctest), then configures, builds and
tests the preset; `build.cmd arm64` does the same on BitWhiz over SSH. On Linux the preset is
the entry point: `cmake --preset linux-gcc-debug && cmake --build --preset linux-gcc-debug &&
ctest --preset linux-gcc-debug`. Never compose a configure or build command from memory: the
`windows-msvc` presets need no vcvars because the Visual Studio generator finds its own tools,
but `build.cmd` sets that environment anyway so one entry point covers both generators.

**Presets.** `windows-msvc-debug`, `windows-msvc-release`, `linux-gcc-debug`,
`linux-clang-debug`, `linux-sanitize`. The ARM64 row is the Linux presets run on BitWhiz.

## 4. Milestones

Milestone scope and exit criteria are `kr216-plan.md` §16. This table tracks this repository's share.

| Milestone | This repository's deliverable | Exit criteria (from the plan) | Status |
|---|---|---|---|
| **M0** | plan, compiler contract mapped, ABI appendix started | D1–D34 resolved; Q1–Q29 answered | **complete** 2026-11-21 |
| **M1** | `kr216-isa.md`, emulator including FPU, disassembler, KRO image loader, test harness | hand-written programs run and pass; every encoding has a vector; FP vectors are bit-exact; `kr216-isa.md` frozen at 1.0 | **complete** 2026-11-22, `11/11` on Windows, WSL and BitWhiz |
| **M2** | VM wiring for krld output; loader against real krcompiler images; `kr216-emu`/`kr216-dis` CLIs | `krc --target=kr216` output links and runs here; round-trip and determinism pass | **complete** 2026-11-22 — assemble → link → load → disassemble → run and determinism pass on Windows, WSL and BitWhiz; see the gate block in §9 |
| **M3** | execution of compiler-generated C: ABI (frames, calls, ENTER/LEAVE), differential runs | C programs run and match host output | not started |
| **M4** | machine-level debug interface (step, run, memory, unwind), KDBG payload handling, console/COM mapping | krdbg drives the emulator; backtraces match expected frames | not started |
| **M5** | execution of 16-bit exception descriptors, landing pads, unwinding tests | `try`/`catch` runs destructors correctly through the emulator | not started |
| **M6** | density and performance measurement for the benchmark corpus | ≤ 2.5 B/instruction; benchmarks tracked | not started |
| **M7** | examples and README so an external user can build and run a program | an external user builds and runs from the README | not started |

## 5. Milestone checklists

### M0 — documents and the gate (complete)

- [x] `kr216-plan.md` v0.11 approved by the owner; D1–D34 and Q1–Q29 settled.
- [x] `kr216-isa.md` draft written: length map, all five classes, F0–F8 payloads, semantics, traps, SYS, MMIO, 60 encoding vectors.
- [x] ISA §15 decision register approved by the owner, and the approval recorded in the ISA header.
- [x] Compiler contract mapped in `../krcompiler`: ADR 0053, `docs/abi-kr216.md`, schema v2, D-050.
- [x] This tracker created.

### M1 — ISA, decoder/disassembler, emulator, loader, harness

**M1.1 Project skeleton**

- [x] `CMakeLists.txt`: C++20, `/W4 /WX` on MSVC and `-Wall -Wextra -Wpedantic -Werror` elsewhere, CTest enabled.
- [x] `src/` created for host code; `src216/` already exists with its rules; `CMakeLists.txt` adds only `src/` and excludes `src216/` explicitly.
- [x] A boundary check target that fails if a `src/` file includes a `src216/` header or the reverse.
- [x] A guest build target that runs `krc --target=kr216` over `src216/` and fails loudly (naming the missing backend) until the kr216 backend exists.
- [x] `CMakePresets.json` with the five presets in §3; each preset's binary directory separate.
- [x] `build.cmd` as specified in §3, resolving `vswhere` → `vcvars64.bat` → bundled CMake/Ninja.
- [x] `build.cmd arm64`: SSH to BitWhiz, run `linux-gcc-debug` and `ctest` there, capture the revision and counts for the gate block.
- [x] Clone this repository on BitWhiz at `~/repos/kr216` — cloned 2026-11-21 at `b74af66`, `origin` over HTTPS.
- [x] `.agents/doctor.msvc.ps1` (from `.agents/templates/`) reporting MSVC, CMake and Ninja with versions.
- [x] `.gitignore` covering `build/`, `out/`, and CMake user files.
- [x] A real test registered with CTest at the first commit — the length map below; no placeholder was needed.
- [x] First build + `ctest` green on `windows-msvc-debug`, logged with counts.

**M1.2 Encoding layer** (`include/kr216/encoding.h`, `src/encoding.cpp`)

- [x] First-byte length map: every byte classified, reserved bytes rejected, minimum lengths pinned by `tests/length_map_test.cpp`.
- [x] Decoded instruction model: opcode, operands (register, FP register, immediate, displacement, mask, condition, branch target), length, validity.
- [x] Short class: `PUSH`/`POP`/`INC`/`DEC`/`CALL Rd`, the nine system encodings, `PUSH R14/R15` and `POP R14/R15` rejected.
- [x] Register class: `LD Rd,#imm16`, `LD Rd,[addr16]`, `ST [addr16],Rd`.
- [x] Compact class: 32 register ops including auto-increment/decrement and `JMP`; branch form with `disp9` relative to the next instruction.
- [x] Normal class: all 29 opcodes, payload layouts, `SETcc` rejecting conditions 14–15.
- [x] Escape class: `F0` operation byte 0x00–0x1A plus specifiers and extensions; `F1` ops 0–4; `F3`–`F6`; `F7`/`F8`; `F2` and `F9`–`FF` rejected.
- [x] Reserved fields must be zero: unary `F3` `Fs`, unary Compact `Rb`, immediate specifier register field, `F4` low nibble.
- [x] Disassembler prints the form the ISA document's Appendix A uses (`ld r4, [r5+3]`, `push #0x0300`, `enter #8, #0x0300`).
- [x] Encoding-vector harness runs all Appendix A vectors and fails with the expected/actual text.

**M1.3 Execution core** (`include/kr216/machine.h`, `src/machine.cpp`, `src/memory.cpp`)

- [x] Machine state: 16 GPRs with R0/R1 semantics, flags, EFP/FP/PC/SP, F0–F3, cycle and retired counters; reset state from ISA §2.4.
- [x] Memory: 64 KiB little-endian, unaligned accesses, modulo-65536 addressing, null-page and reserved-window traps, MMIO dispatch.
- [x] Trap entry (`{EFP, PC, flags}`, `I` cleared), vector table, `IRET`, trace flag.
- [x] Integer ALU: `MOV`, `ADD`/`ADDC`, `SUB`/`SUBB`, `CMP`, `AND`/`OR`/`XOR`/`NOT`/`NEG`, `INC`/`DEC`, `SXTB`, with the flags in ISA §10.2.
- [x] Shifts `SHL`/`SHR`/`SAR` with the count and carry rules; `MUL`/`MULH`; `DIV`/`DIVS`.
- [x] Loads and stores across every mode, including auto-increment/decrement steps and `Ra == Rb`.
- [x] Branches (`Bcc`, `BRA`), `CALL`/`JMP` in all forms, `RET`.
- [x] `PUSH`/`POP` mask in both orders; `ENTER`/`LEAVE`, including a runtime `SP` adjustment between them.
- [x] System: `NOP`, `EI`, `DI`, `HALT`, `SYS`, `RDCYCLE`, `BRK`.
- [x] FPU: F0–F3 as binary64, `FADD`–`FDIV`, `FNEG`/`FABS`/`FSQRT`/`FMOV`/`FCMP`/`FRND`, `FLD`/`FST`/`FLDS`/`FSTS`, `I2F`/`U2F`/`F2I`/`F2U` with traps, `FPUSH`/`FPOP`.
- [x] SYS host calls 0–4 (`exit`, `write`, `read`, `time`, `debug_write`) with the reserved-number trap.
- [x] `kr216-emu` CLI: load an image, run with a budget, report halted/fault/exit status, with console output on stdout and `--dump-disassembly`.

**M1.4 Image loader** (`include/kr216/image.h`, `src/image.cpp`)

- [x] Parse the KRCompiler image container against its written layout (ADR 0054): magic, target/ABI version, sections, entry, image base, embedded KDBG payload; reject a wrong target ID or ABI version.
- [x] Reject out-of-range sections and overlapping placements; never write outside the 64 KiB map.


**M1.5 `kr216-dis`**

- [x] Read an image and print address, bytes, mnemonic and operands, advancing by the decoded length (`tools/kr216-dis`); a raw byte range waits for the debugger's memory view.
- [x] A reserved encoding prints as `.byte` with its bytes, never as a guessed instruction.

**M1.6 Conformance harness** (`tests/`)

- [x] `vectors/` — every encoding in ISA Appendix A, in a tight and an over-long buffer, with the reserved and illegal-field sets pinned (`tests/vector_test.cpp`).
- [x] `semantics/` — one behaviour per test, full post-state including flags (`tests/machine_test.cpp`).
- [x] `canonical/` — 40 short forms against their `F0` equivalents, comparing every register but PC, the flags and the data and stack windows (`tests/canonical_test.cpp`); the full register × mode sweep is the `modes/` item.
- [x] `modes/` — mode × register × load/store × byte/word matrix, failures named per cell (`tests/modes_test.cpp`), plus absolute and PC-relative bases.
- [x] `fp/` — bit-exact vectors: four operations over thirteen values, `FSQRT`/`FNEG`/`FABS`/`FRND`, `FCMP`, `F2I`/`F2U` traps, NaN by `isnan`, infinities, signed zero and subnormals (`tests/fp_test.cpp`).
- [x] `frames/` — `ENTER`/`LEAVE` for every even frame size 0–254 and three masks, nesting, `alloca` (`tests/frames_test.cpp`).
- [x] `fuzz/` — 200k random streams from a fixed seed decode deterministically, never overrun the buffer, trap on reserved bytes, and execute for a bounded budget without hanging (`tests/fuzz_test.cpp`).
- [x] `traps/` — every vector, the frame layout, `IRET`, trace, nested traps and the no-room fault (`tests/traps_test.cpp`).
- [x] `kr216-isa.md` frozen at 1.0 with a change log, once every Appendix A vector passed.

### M2 — the compiler's images run here

- [x] Load a `krc`-built image whose format the compiler writes: `tests/fixtures/kr216/hello.k216` is the committed fixture, assembled and linked by `krc`/`krld` at `4da265a` and parsed by `Image::parse`.
- [x] Run it to a known exit status under `kr216-emu`: the fixture exits **42**; `tests/k216_image_test.cpp` also runs it in-process.
- [x] Reproduce a fixed image byte-for-byte through a reset/reload cycle (`loader` determinism): two loads/reset runs compare the full `CpuState` and all 64 KiB of memory.
- [x] `kr216-dis` matches the assembler's listing for the same image: `k216_disassembly` runs the CLI on the committed fixture and checks the `.text` header, `0200: 52 2A 00 ld r2, #0x2A` and `0203: 73 sys`.

### M3 — C programs run here

- [ ] Execute a compiler-built C program (loops, pointers, aggregates) and compare output and exit status with the host build.
- [ ] Check the ABI the emulator can observe: argument registers, stack argument placement, frame layout, callee-saved registers, aggregate destination in R6.
- [ ] Verify the literal-typing regression (`long x = 32768;`) on the kr216 profile before arithmetic fixtures.

### M4 — debugger interface and unwinding

- [ ] Machine-level interface for `krdbg` `IDebugTarget`: step, budgeted run, halted/fault states, layout, register and memory access, disassembly, unwind, retired-instruction count, reset.
- [ ] `reset()` reloads the last image and clears state (moved here from M1.4: it needs the debug interface's image handle).
- [ ] Console maps to the `out`/`in`/`status` COM contract; `video()` reports absent.
- [ ] Backtrace through a known call chain matches expected frames (`frame = 0`, return PC at `FP + 2`).
- [ ] krdbg's variable-length disassembly change has landed upstream; it was raised there in 2026-11 rather than waiting for this milestone (§8, Q23).

### M5 — exceptions execute

- [ ] The 16-bit EH descriptor ADR (0055) exists on the compiler side before any M5 test runs.
- [ ] Execute the 16-bit exception descriptors once that ADR and the runtime exist; `has_eh` turns on there, not here.
- [ ] Unwinding test: a throw across one to five frames runs every destructor once, in order, and lands in the right handler with SP/FP correct.

### M6 — density and performance

- [ ] Fixed corpus (string copy, `printf`, a small container, a recursive function, an FP loop) compiled and measured in bytes.
- [ ] Track bytes/instruction against the ≤ 2.5 target; record the corpus and the revision.

### M7 — polish

- [ ] README: what kr216 is, how to build the emulator, how to run a program.
- [ ] Examples that build and run with the committed toolchain state.
- [ ] An external user's build-and-run path recorded from a clean checkout.

## 6. Test strategy

The plan's §13 is the strategy; this is how each item is realised here.

| Plan §13 item | Realised as | Proves |
|---|---|---|
| 1 Encoding vectors | `vectors/`, driven by ISA Appendix A | the decoder and disassembler agree with the normative table |
| 2 Semantic tests | `semantics/`, full post-state per instruction | instruction behaviour and flags |
| 3 Canonical equivalence | `canonical/` | every compression equals its `F0` form |
| 4 Decoder fuzzing | `fuzz/` | no crash, no out-of-bounds fetch, every reserved encoding traps |
| 5 Addressing-mode matrix | `modes/` | every mode × register × size |
| 6 FP conformance | `fp/` | bit-exact IEEE-754 binary64 results |
| 7 Differential execution | M3 task with the compiler's output | the ABI is the same on both sides |
| 8 ISA conformance suite | `semantics/` + hand-written assembly programs | carry chains, shifts, stack discipline, traps |
| 9 Frame tests | `frames/` | `ENTER`/`LEAVE` round-trips and runtime frame growth |
| 10 Unwinding tests | M5 | destructors run once, in order, with correct SP/FP |
| 11 Density benchmark | M6 | bytes/instruction |
| 12 Debugger self-test | M4 | backtraces match expected frames |

**Floating point on the host.** The emulator computes in the host's IEEE-754 binary64. Builds use no fast-math, no contraction, and no x87 excess precision (SSE2 on x64, NEON on ARM64); a build that cannot guarantee that fails the FP suite rather than producing approximate results. Results are compared as bit patterns.

**What a failing vector reports.** The encoding offset, the bytes, the expected and actual disassembly, and the ISA section that decides it — so a failure points at the document or at the code, never at "something is wrong".

## 7. Gates and evidence

A gate is evidence for a revision, not a tax on every commit. Run one before a milestone closes and at least once per session.

**Windows + WSL gate block**, recorded in the running log:

```
<ISO timestamp> — host gate (Windows + WSL 2)
Revision: <short sha>

windows-msvc-debug     PASS <n>/<n>
windows-msvc-release   PASS <n>/<n>   (build only unless the milestone allows)
linux-gcc-debug        PASS <n>/<n>
linux-clang-debug      PASS <n>/<n>
linux-sanitize         PASS <n>/<n>
not verified: Linux ARM64 (BitWhiz)
```

**ARM64 gate block**, recorded when BitWhiz runs:

```
<ISO timestamp> — ARM64 gate (BitWhiz, Raspberry Pi 5, Debian 13)
Revision: <short sha>
linux-gcc-debug     PASS <n>/<n>
linux-clang-debug   PASS <n>/<n>
linux-sanitize      PASS <n>/<n>
```

A row that does not run is named as not verified. A count that differs from another row is explained in prose beside the block.

## 8. Resolved items and remaining risks

Each item was open in this tracker's first draft; the owner resolved all seven on 2026-11-21. The resolution is now a rule above or a task in §5.

| Item | Resolution | Lands in |
|---|---|---|
| When the ISA stops being a draft | **Freeze 1.0 when the M1.6 vector harness is green.** After that a change is a numbered ISA revision under the plan's change policy. | M1 exit criteria, §5 M1.6 |
| Who writes the image container's byte layout | **The compiler does, now:** a krcompiler ADR (0054) plus a `docs/object.md` section; this repository's loader implements it. | §5 M1.4; external task |
| krdbg's fixed disassembly alignment | **Raise the krdbg change upstream now**, not at M4. It is a `TargetLayout` plus row-advance change, and M4 must not begin on an unfixed dependency. | §5 M4; external task |
| The 16-bit exception descriptor | **ADR 0055 after M1–M2, before M5**; `has_eh` stays false until then. Renumbered from 0054 because the image container takes the next free number. | §5 M5; external task |
| How binding the ARM64 row is | **Mandatory once per milestone**; when BitWhiz is unreachable the gate names the row not verified rather than dropping it. | §2, §7 |
| How this machine reaches BitWhiz | **SSH**, host and user recorded in §3; the build script runs the Linux presets there and the gate block is written from its output. | §3, §5 M1.1 |
| How the Windows build finds its tools | **`build.cmd`** resolves `vswhere` → `vcvars64` → bundled CMake and Ninja. | §3, §5 M1.1 |

**External tasks.** Owned in another repository, tracked in its tracker; this table records the dependency.

| Task | Repository | Status |
|---|---|---|
| Image container ADR 0054 plus a `docs/object.md` section, writing D33's byte layout | `..\krcompiler` | not started |
| 16-bit exception descriptor ADR 0055 | `..\krcompiler` | deferred to after M1–M2 |
| Variable-length disassembly rows (`TargetLayout` plus the surface's row advance) | `..\krdbg` | not started; raise in 2026-11 |

**Remaining risks.**

- **ISA drift while M1 is built.** The encodings are normative from draft 0.2, but the emulator will disagree with the document somewhere in the first thousand vectors. When it does, decide which is right, fix both, and log it.
- **BitWhiz availability** at a milestone close. The gate records the row as not verified; it never silently drops it.
- **Toolchain fragility.** A Visual Studio upgrade can move the bundled CMake or change the MSVC version; the doctor reports versions so a gate names what actually ran.

## 9. Running log

- **2026-11-21T20:54:12-08:00** — *(backfilled; timestamp from `kr216-plan.md`'s 0.11 revision row)* Plan v0.11 approved and committed as `23e4686`: D34 fixes F0's canonical layout, and the review's corrections landed (aggregate destination, `long double`, `FRND`, `call r3`, mode numbering, unary F3). M0's decision and question gates closed.
- **2026-11-21T21:48:36-08:00** — *(backfilled; timestamp from commit `bbcaf6f`)* `kr216-isa.md` draft 0.1 written and committed: length map, five classes, `F0`–`F8`, semantics, traps, `SYS`, MMIO, 60 vectors, and the 16-decision register in §15. Not run: no build exists.
- **2026-11-21T21:48:41-08:00** — *(backfilled; timestamp from commit `4b96bca`)* Compiler-side registration pushed: ADR 0053 (renumbered from 0052 after the rebase), `docs/abi-kr216.md`, schema v2, D-050, and the indexes. Linux/ARM64 rows not applicable: documentation only.
- **2026-11-21T22:04:42-08:00** — §15's sixteen decisions approved by the owner and the approval recorded in the ISA header; this tracker created. Toolchain discovery on the Windows dev machine: `vswhere` found Visual Studio 18 Community (18.12.1), `cl` is 19.52.37029 for x64 after `vcvars64.bat`, CMake is the bundled 4.3.1-msvc1 (not on `PATH`, newest Visual Studio generator `Visual Studio 17 2022`), and Ninja ships beside it. Consequence recorded in §3: the Windows build uses Ninja inside `vcvars64`. No build or test run — there is no source tree yet.
- **2026-11-21T22:28:10-08:00** — Open items resolved by the owner: the ISA freezes 1.0 at M1 exit; the image container's byte layout becomes compiler ADR 0054 (so the EH descriptor is renumbered 0055); the krdbg change is raised upstream now; the ARM64 row is mandatory per milestone with BitWhiz reached over SSH; the Windows build uses `build.cmd`. §8 now records the resolutions and the external tasks; §5 gained the corresponding tasks in M1.1, M1.4, M1.6, M4 and M5. Not run: no build exists.
- **2026-11-21T22:47:55-08:00** — BitWhiz reached over SSH with key authentication: `ssh -o BatchMode=yes BitWhiz` returned `BitWhiz`, `Linux 6.18.39+rpt-rpi-2712 aarch64`, user `kevinro`, and `~/repos` exists. It contains the sibling checkouts (`kr16`, `kr32`, `krcompiler`, `krdbg` and others) but **not `kr216`**. Toolchain on the host: CMake 3.31.6, ctest 3.31.6, Ninja 1.12.1, GCC 14.2.0, Clang 19.1.7, git 2.47.3, 4 cores and 16 GiB RAM. Recorded in §3; the clone is an open M1.1 task. No build run — there is no source tree on either machine yet.
- **2026-11-21T22:49:14-08:00** — `kr216` cloned on BitWhiz at `/home/kevinro/repos/kr216`; the clone's HEAD was `b74af66` before this record and `origin` is the GitHub HTTPS URL. The three local commits ahead of the remote (`683c290`, `1e80b9b`, `b74af66`) were pushed first so the clone carries the tracker. BitWhiz reaches GitHub over HTTPS only: `git ls-remote git@github.com:kevinwross/kr216.git` fails with host key verification. M1.1's clone task is ticked; the ARM64 row still has no build to run.
- **2026-11-21T22:51:55-08:00** — *Correction to the entry above's push implication.* BitWhiz does hold a GitHub credential: `credential.helper=store` is set globally, `~/.git-credentials` exists with one line (presence and line count checked; the file was not read), and `GIT_TERMINAL_PROMPT=0 git push --dry-run origin main` exits 0. What is absent is not credentials but an encrypted credential manager — `store` keeps the token in plaintext, and GitHub SSH is unconfigured. §3 corrected; the ARM64 host can push a gate record if a future task needs it.
- **2026-11-21T23:02:06-08:00** — Re-checked BitWhiz after the owner's credential fix. Effective state unchanged in mechanism, refreshed in substance: `~/.git-credentials` was rewritten at 23:01:34 (1 line, 124 bytes; contents not read) and the HTTPS push dry-run still exits 0, so pushes from BitWhiz work. The owner also added `credential.credentialstore=secretservice`, but it has no backend on that host: there is no `git-credential-manager`, `/usr/lib/git-core` carries only `git-credential-store` and `-cache`, `secret-tool` is absent although `libsecret-1-0` and `gnome-keyring` are installed, and `credential.helper` is still `store`. GitHub over SSH remains unavailable: `~/.ssh` holds only `authorized_keys` and no agent is running.
- **2026-11-21T23:04:20-08:00** — Deploy-key path set up on BitWhiz: generated `~/.ssh/id_ed25519` (ed25519, no passphrase, comment `bitwhiz-kr216-deploy`), trusted GitHub's host key on first use (`ssh -o StrictHostKeyChecking=accept-new -T git@github.com` added the ED25519 key to `known_hosts`), and tried the SSH remote. `git@github.com` still answers `Permission denied (publickey)` because the public key is not registered on GitHub yet, so the remote was rolled back to the HTTPS URL automatically. Remaining action is the owner's: add the key as a read/write deploy key on `kevinwross/kr216`, after which the switch and fetch test are one command.
- **2026-11-21T23:13:43-08:00** — Built and installed `krcompiler` on BitWhiz for the other projects. Pulled the checkout to `4b96bca`, configured `linux-gcc-release`, built (80/80 targets, no warnings in the tail), ran `ctest --preset linux-gcc-release` → **915/915 passed in 81.51 s**, and installed with `cmake --install build/linux-gcc-release --prefix $HOME/.local`. `krc --version` and `krld --version` report 0.2.0; `krc --list-targets` shows kr32 and kr16 with backends and kr64 without. End-to-end proof: `hello.c` compiled with the installed `krc -c`, linked with the installed `krld` plus the sysroot's `libkrcrt.kro`, `fileio.s`, `eh.s` and `crt0.s`, and ran under the ARM64 KR32 VM to `exit=42`. Two facts recorded in §3: `krc -o image.k32` is not a KR32 path (it prints the not-implemented message), and `~/.local/bin` is only on the login `PATH`.
- **2026-11-21T23:18:23-08:00** — Checked the three sibling projects on BitWhiz against the installed `krc-toolchain`. `kr32` had a working 0.1.0 binary but its configure failed at `CMakeLists.txt:13` (`krc-toolchain` not found); reconfiguring with `-DCMAKE_PREFIX_PATH=$HOME/.local` built 39/39 and installed **0.3.0**, which is current. `kr16` was already up to date (`ninja: no work to do`) and has no install rule, so `build/linux-gcc/kr16` (1.0.0) was copied to `~/.local/bin`. `krdbg` has no `krdbg` executable target in this tree — only `krdbg_demo`, `krdbg_settings` and `krdbg_tests` — and its suite passes 3/3; no install rule, so nothing was installed. §3 records the table and the `CMAKE_PREFIX_PATH` requirement.
- **2026-11-21T23:31:50-08:00** — Proved the same build/install sequence on this host in both platforms. Windows: configured all four presets (the bundled CMake does accept `Visual Studio 18 2026`), built and installed `krcompiler` release to `C:\Users\kevin\krc`, built kr32 Release and installed it, built kr16 Release and copied it, and built krdbg's three targets. WSL: built and installed `krcompiler` release and kr32 Debug to `~/.local`, built kr16 and krdbg. Installed and verified: `krc`/`krld` 0.2.0, `kr32` 0.3.0, `kr16` 1.0.0 on both. Three traps recorded in §3: a cached `krc-toolchain_DIR` made a Release consumer link the Debug library (`__imp__CrtDbgReport`), the VS generator needs matching `--config` on build and install, and WSL lacks clang so the clang and sanitizer rows cannot run there.
- **2026-11-21T23:41:36-08:00** — Split the source trees by compiler, at the owner's direction: `src/` is host code built by the host toolchain, `src216/` is guest code built by `krc --target=kr216`, and the boundary is stated in `AGENTS.local.md` and `src216/README.md`. Created `src216/README.md` with what may and may not live there; `src/` appears with the first host code in M1.1. The rule is in §1 and M1.1 gained three tasks: create `src/`, exclude `src216/` from the host build, add a cross-tree include check, and wire a guest build target that fails loudly until the kr216 backend exists. No code written.
- **2026-11-21T23:54:00-08:00** — **M1.1 complete, M1.2 started.** Code commit `35625b3`: CMake project (C++20, warnings-as-errors, CTest), five presets, `build.cmd` (doctor → `vswhere` → `vcvars64` → the Visual Studio CMake/ctest) with an `arm64` path over SSH, the doctor, `.gitignore`, a boundary check that fails both configure and the `boundary` test on a cross-tree include, and a `guest` target that fails loudly until the kr216 backend exists. First real unit: the ISA §3.1 first-byte length map with `tests/length_map_test.cpp` pinning all 256 bytes, the per-class counts (96 Short, 48 Register, 64 Compact, 32 Normal, 8 escapes, 8 reserved) and the reserved set. Green on all three hosts: `windows-msvc-debug` 2/2, WSL `linux-gcc-debug` 2/2, `build.cmd arm64` on BitWhiz 2/2. `AGENTS.local.md`'s Environment and platform sections are filled in and its `environment:unrecorded` marker removed.
- **2026-11-22T00:00:33-08:00** — **M1.2 complete.** `decode()` covers every class — short rows and the nine system encodings, the register class, compact register and branch forms, all 29 normal opcodes, and the escape prefixes `F0`–`F8` with reserved fields rejected — and `disassemble()` reproduces the ISA document's own Appendix A text. `tests/vector_test.cpp` runs the 60 vectors in a tight and an over-long buffer, checks truncation asks for more, walks all 256 first bytes against the 22 refused and 234 allocated encodings, and pins ten reserved-field and illegal-operand cases. **The harness found four defects on its first run:** my decoder read the compact branch form little-endian when it packs from the most significant bit; FP stores name the FP register first (`fst f1, [r2]`), not the address; and three Appendix A vectors were wrong — `f2i` is `F5 20 20` under the `OOOO Fd 00` layout, the generic addressing example must be `ld` not `add` because ALU operands are register-or-immediate only, and the generic immediate example must be `mov` not `ld` because `LD`'s second operand is address-only. The ISA document's vectors are corrected in code commit `95c16fa`. Green: Windows 3/3, WSL 3/3, BitWhiz 3/3.
- **2026-11-22T00:10:14-08:00** — **M1.3 nearly complete** at `908916f`: the machine core owns the memory and trap windows, the register file with R0/R1 constants, the trap frame and EFP chain, the console/timer/system MMIO window, the SYS host calls and the FPU over binary64. Execution covers the integer ALU with the ISA flag rules, shifts, `MUL`/`MULH`, `DIV`/`DIVS`, every load/store mode, branches, calls, mask push/pop, `ENTER`/`LEAVE`, and the whole FP family. `tests/machine_test.cpp` asserts full post-state per case, including trap frames and saved PCs; its first run found one systemic defect — `LD Rd,#imm` shares `Op::Ld` with the memory form and was executed as an indirect load from R0 — now fixed. Green: Windows 4/4, WSL 4/4, BitWhiz 4/4. Remaining for M1.3: the `kr216-emu` CLI, which waits on the image loader (M1.4).
- **2026-11-22T00:16:15-08:00** — **M1.3 complete and M1.4 mostly done.** Compiler-side ADR 0054 (`2e6f9f2`) fixes the `K216` container: 64-byte header with magic, container and ABI versions, target ID 4, entry, base, table offsets, file size and optional CRC32; 32-byte section records in the `.kex` shape; the `0x0100`–`0xFEFF` allocated window and overlap rejection. `src/image.cpp` validates every field before copying and writes the entry into the reset vector; `tests/image_test.cpp` builds the exact bytes the M2 writer must emit, runs a real image to exit 42 and pins seven rejection cases. The CLI (`tools/kr216-emu`) loads, runs with a budget and reports the stop reason. The tests' first run found the builder's missing trailing NUL in the string table — a good sign that the format is pinned byte for byte. Green: Windows 5/5, WSL 5/5, BitWhiz 5/5 at `4f7ef7d`.
- **2026-11-22T00:18:35-08:00** — Added the canonical-equivalence and decoder-fuzz suites at `bbd59e5`, both green on the first run on all three hosts (7/7). `canonical_test` runs 40 short forms against their `F0` equivalents on two identical machines and compares every register but PC, the flags, F0–F3 and the data and stack windows; `fuzz_test` decodes 200k random streams from a fixed seed, checks deterministic decoding, buffer bounds, the reserved-byte rule and six-byte completeness, and executes every hundredth stream for a bounded budget. GCC caught two unused symbols the MSVC build accepted (an unused typedef and an unused helper), which is why the Linux row matters. M1.6 now has vectors, semantics, canonical and fuzz ticked; modes, fp, frames, traps and the ISA freeze remain.
- **2026-11-22T00:23:05-08:00** — Added the addressing-mode, FP and frame suites at `bbdac30`, green on all three hosts (10/10). `modes_test` runs the full five-mode × six-register × byte/word × load/store matrix and checks the effective address, the transferred bytes and the base update separately, plus absolute and PC-relative bases; `fp_test` compares 676 arithmetic cases, ten unary cases, five compares and the conversion edge cases against the host's IEEE binary64, treating NaN by `isnan` because its payload differs between x86 and ARM; `frames_test` covers every even frame size 0–254 with three masks, `LEAVE` restoration, `alloca` and nesting. **The frame test found two mistakes of mine on its first run:** `F7` carries an 8-bit frame (four bytes, not five), and the mask block grows from the lowest address with descending register number — both now pinned. M1.6 has only `traps/` and the ISA 1.0 freeze left; the `kr216-dis` tool and the loader's reset-reload item also remain.

- **2026-11-22T00:25:00-08:00** — **M1 complete; ISA frozen at 1.0.** `traps_test` covers the four vectors (BRK, illegal encoding, unknown SYS and divide-by-zero, null page and reserved window), the six-byte frame, EFP chaining, `IRET`, the trace flag, a trap inside a handler, and the two no-room cases (a trap with SP too low and `IRET` with no frame), both of which stop the machine as `Faulted`. Writing it corrected three of my expectations, not the machine: a breakpoint saves the BRK's own address, and `IRET` restores `T`, so a traced `IRET` immediately re-enters the handler. `tools/kr216-dis` prints a real image (`0200: 52 2A 00  ld r2, #0x2A`), and both CLIs ran the same hand-built image end to end (`kr216-emu` exit 42). `kr216-isa.md` is frozen 1.0 with a change log: every Appendix A vector passes, the short forms match their `F0` equivalents, and the FP vectors are bit-exact. Green: Windows 11/11, WSL 11/11, BitWhiz 11/11 at `0e799be`.- **2026-11-22T00:51:00-08:00** — ERRATA: `tests/traps_test.cpp` labelled the `F1` op-0 reserved-window access as `ld r2, [0xFF80]`, but ISA 8.2 and the decoder both say `LDBU`; the test asserted only that the access traps, so its behaviour was right and its comment was wrong. Comment corrected to name the byte load. Found while writing the compiler's `F1` encoder, which read the table rather than trusting recollection. Green: 11/11 on Windows.
- **2026-11-22T01:32:41-08:00** — **M2's image path works end to end on Windows: assemble → KRO → link → K216 → load → disassemble → run.** The compiler-side changes are at `4da265a`: `.text`/`.global` in the kr216 assembler, the target-4 KRO writer, the ADR 0054 K216 writer with its `.reset` section, and the `krld` target-4 branch. This repository gained `tests/fixtures/kr216/hello.k216` (148 bytes, SHA-256 `D61FAEC3C8BCD5DBDF93AA016F627FCE6F2DF3FD4D573F8343398C0BB59304FF`) and `tests/k216_image_test.cpp`: it parses the committed fixture, checks the entry/base, text bytes, reset word and the disassembly of both instructions, runs it twice and compares every register, FP register, cycle/retired count and all 64 KiB of memory, then checks the reset state is deterministic across two loads. `build.cmd` is **12/12** green; the fixture image exits **42**. The image was produced twice from the same source and both the `.kro` and `.k216` hashes matched. The compiler was built at `4da265a`; this repository's commit is the one that adds the fixture and test. `kr216-dis`'s CLI output on the fixture is `0200: 52 2A 00 ld r2, #0x2A` / `0203: 73 sys`, but a compiler-listing comparison is still open. Linux and ARM64 rows were not run for this change.
- **2026-11-22T01:48:09-08:00** — **CORRECTION to the M2 gate entry below: the 13th test was added after the 12/12 runs, so all three rows were rerun at `87491ac`.** Windows `windows-msvc-debug` **13/13**, WSL `linux-gcc-debug` **13/13**, BitWhiz `linux-gcc-debug` **13/13**. The 12/12 Windows and BitWhiz runs at `85eb64b` were the same code without `k216_disassembly`; no failure was hidden. `AGENTS.local.md` now records the verified revision and count.
- **2026-11-22T01:47:10-08:00** — **M2 complete; the gate ran on Windows, WSL and BitWhiz.** `k216_disassembly` now runs the `kr216-dis` CLI on the committed fixture and checks the section header plus both instruction lines, so the last M2 checklist item is covered.
  ```
  windows-msvc-debug     PASS 13/13   (Windows 11, MSVC 19.52)
  linux-gcc-debug        PASS 13/13   (WSL 2, GCC 15.2.0)
  linux-gcc-debug        PASS 13/13   (BitWhiz, aarch64, GCC 14.2.0)
  ```
  The compiler side at `bb6ce58` produced the same 148-byte image on all three hosts (SHA-256 `D61FAEC3…04FF`), and each host's `kr216-emu` exited **42** on it. `krc_tests --test-case='kr216*'` is **37/37 cases, 486 assertions** on Windows, WSL and BitWhiz. Full compiler suites: WSL `linux-gcc-debug` **723/723**, BitWhiz `linux-gcc-debug` **919/919**; the Windows row is **912/915**, with the three pre-existing VM filesystem tests (`krc_run_kr16_file_layer`, `krc_run_kr16_shipped_fileio`, `krc_run_div_and_vsprintf`) still failing and outside this milestone. M2's round-trip and determinism criterion is met on all three hosts; the Windows full-suite failure is recorded as a FIX-BEFORE, not claimed green.
