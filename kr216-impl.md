# kr216 — Implementation Tracker

**Status:** Plan approved 2026-11-21 (`kr216-plan.md` v0.11); ISA at draft 0.2 with its §15 decision register approved; ABI appendix accepted (`../krcompiler/docs/abi-kr216.md`, ADR 0053). **Current milestone: M1 — not started.**
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

**ARM64.** A change that is portable by construction still needs the ARM64 row at least once per milestone: build and test on BitWhiz, record the revision and counts. A row absent from CI is untested, not supported.

## 3. Platforms, toolchain and hosts

| Target | Host | Toolchain | Used for |
|---|---|---|---|
| Windows x64 | the dev machine | MSVC 19.52.37029 for x64 (Visual Studio 18 Community); CMake 4.3.1-msvc1 and Ninja ship with that Visual Studio | primary development; the Windows gate row |
| Linux x64 | WSL 2 on the dev machine | GCC and Clang, Ninja; versions recorded at the first build | the Linux gate rows, sanitizer row |
| Linux ARM64 | **BitWhiz** (Raspberry Pi 5, Debian 13), reached remotely | GCC and Clang; versions recorded at the first build | the ARM64 gate row, at least once per milestone |

**Discovered on 2026-11-21, on the Windows dev machine** (command and output in the running log):

- `vswhere.exe` reports the Visual Studio installation at `C:\Program Files\Microsoft Visual Studio\18\Community`, product version 18.12.1.
- `cl` is 19.52.37029 for x64 once `VC\Auxiliary\Build\vcvars64.bat` has run.
- CMake is **not on `PATH`**. The usable one is the Visual Studio copy,
  `...\Common7\IDE\CommonExtensions\Microsoft\CMake\CMake\bin\cmake.exe`, version 4.3.1-msvc1, with
  `...\Common7\IDE\CommonExtensions\Microsoft\CMake\Ninja\ninja.exe` beside it.
- That CMake's newest Visual Studio generator is **`Visual Studio 17 2022`**, so it cannot generate a project for the installed Visual Studio 18. The Windows build therefore runs **`Ninja` inside the `vcvars64` environment**; the build entry point must set that environment itself rather than assume a developer prompt.

**Build entry point (to exist from the first M1 commit).** `build.cmd [preset] [--test]` resolves the toolchain the way the discovery above did — `vswhere`, then `vcvars64.bat`, then the bundled CMake and Ninja — configures the preset, builds, and with `--test` runs `ctest`. It never assumes `cmake`, `cl`, `ninja` or a generator name on `PATH`. On Linux the equivalent is the preset's plain `cmake --preset <preset>`.

**Presets (to exist from the first M1 commit):** `windows-msvc-debug`, `windows-msvc-release`, `linux-gcc-debug`, `linux-clang-debug`, `linux-sanitize`. The ARM64 row is the Linux presets run on BitWhiz.

## 4. Milestones

Milestone scope and exit criteria are `kr216-plan.md` §16. This table tracks this repository's share.

| Milestone | This repository's deliverable | Exit criteria (from the plan) | Status |
|---|---|---|---|
| **M0** | plan, compiler contract mapped, ABI appendix started | D1–D34 resolved; Q1–Q29 answered | **complete** 2026-11-21 |
| **M1** | `kr216-isa.md`, emulator including FPU, disassembler, KRO image loader, test harness | hand-written programs run and pass; every encoding has a vector; FP vectors are bit-exact | not started |
| **M2** | VM wiring for krld output; loader against real krcompiler images; `kr216-emu`/`kr216-dis` CLIs | `krc --target=kr216` output links and runs here; round-trip and determinism pass | not started |
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

- [ ] `CMakeLists.txt`: C++20, `/W4 /WX` on MSVC and `-Wall -Wextra -Wpedantic -Werror` elsewhere, CTest enabled.
- [ ] `CMakePresets.json` with the five presets in §3; each preset's binary directory separate.
- [ ] `build.cmd` as specified in §3, resolving `vswhere` → `vcvars64.bat` → bundled CMake/Ninja.
- [ ] `.agents/doctor.msvc.ps1` (from `.agents/templates/`) reporting MSVC, CMake and Ninja with versions.
- [ ] `.gitignore` covering `build/`, `out/`, and CMake user files.
- [ ] One placeholder test registered with CTest so the entry point proves green from the first commit.
- [ ] First build + `ctest` green on `windows-msvc-debug`, logged with counts.

**M1.2 Encoding layer** (`include/kr216/encoding.h`, `src/encoding.cpp`)

- [ ] Decoded instruction model: opcode, operands (register, FP register, immediate, displacement, mask, condition, branch target), length, validity.
- [ ] First-byte length map: every byte classified, reserved bytes rejected.
- [ ] Short class: `PUSH`/`POP`/`INC`/`DEC`/`CALL Rd`, the nine system encodings, `PUSH R14/R15` and `POP R14/R15` rejected.
- [ ] Register class: `LD Rd,#imm16`, `LD Rd,[addr16]`, `ST [addr16],Rd`.
- [ ] Compact class: 32 register ops including auto-increment/decrement and `JMP`; branch form with `disp9` relative to the next instruction.
- [ ] Normal class: all 29 opcodes, payload layouts, `SETcc` rejecting conditions 14–15.
- [ ] Escape class: `F0` operation byte 0x00–0x1A plus specifiers and extensions; `F1` ops 0–4; `F3`–`F6`; `F7`/`F8`; `F2` and `F9`–`FF` rejected.
- [ ] Reserved fields must be zero: unary `F3` `Fs`, unary Compact `Rb`, immediate specifier register field, `F4` low nibble.
- [ ] Disassembler prints the form the ISA document's Appendix A uses (`ld r4, [r5+3]`, `push #0x0300`, `enter #8, #0x0300`).
- [ ] Encoding-vector harness runs all Appendix A vectors and fails with the offset and the expected/actual text.

**M1.3 Execution core** (`include/kr216/machine.h`, `src/machine.cpp`, `src/memory.cpp`)

- [ ] Machine state: 16 GPRs with R0/R1 semantics, flags, EFP/FP/PC/SP, F0–F3, cycle and retired counters; reset state from ISA §2.4.
- [ ] Memory: 64 KiB little-endian, unaligned accesses, modulo-65536 addressing, null-page and reserved-window traps, MMIO dispatch.
- [ ] Trap entry (`{EFP, PC, flags}`, `I` cleared), vector table, `IRET`, trace flag.
- [ ] Integer ALU: `MOV`, `ADD`/`ADDC`, `SUB`/`SUBB`, `CMP`, `AND`/`OR`/`XOR`/`NOT`/`NEG`, `INC`/`DEC`, `SXTB`, with the flags in ISA §10.2.
- [ ] Shifts `SHL`/`SHR`/`SAR` with the count and carry rules; `MUL`/`MULH` and the unsigned correction sequence; `DIV`/`DIVS`.
- [ ] Loads and stores across every mode, including auto-increment/decrement steps and `Ra == Rb`.
- [ ] Branches (`Bcc`, `BRA`), `CALL`/`JMP` in all forms, `RET`.
- [ ] `PUSH`/`POP` mask in both orders; `ENTER`/`LEAVE`, including a runtime `SP` adjustment between them.
- [ ] System: `NOP`, `EI`, `DI`, `HALT`, `SYS`, `RDCYCLE`, `BRK`.
- [ ] FPU: F0–F3 as binary64, `FADD`–`FDIV`, `FNEG`/`FABS`/`FSQRT`/`FMOV`/`FCMP`/`FRND`, `FLD`/`FST`/`FLDS`/`FSTS`, `I2F`/`U2F`/`F2I`/`F2U` with traps, `FPUSH`/`FPOP`.
- [ ] SYS host calls 0–4 (`exit`, `write`, `read`, `time`, `debug_write`) with the reserved-number trap.
- [ ] `kr216-emu` CLI: load an image, run with a budget, report halted/fault/exit status.

**M1.4 Image loader** (`include/kr216/image.h`, `src/image.cpp`)

- [ ] Parse the KRCompiler image container (D33): magic, target/ABI version, sections, entry, image base, embedded KDBG payload; reject a wrong target ID or ABI version.
- [ ] Reject out-of-range sections and overlapping placements; never write outside the 64 KiB map.
- [ ] `reset()` reloads the last image and clears state.

**M1.5 `kr216-dis`**

- [ ] Read an image (or a raw byte range) and print address, bytes, mnemonic and operands, advancing by the decoded length.
- [ ] A reserved encoding prints as a trap with its bytes, never as a guess.

**M1.6 Conformance harness** (`tests/`)

- [ ] `vectors/` — every encoding in ISA Appendix A, both directions where an assembler exists.
- [ ] `semantics/` — one behaviour per test, full post-state including flags.
- [ ] `canonical/` — every short form against its `F0` equivalent, every register and mode.
- [ ] `modes/` — mode × register × load/store × byte/word matrix, failures named per cell.
- [ ] `fp/` — bit-exact vectors: four operations, `FSQRT`, conversions, NaN, infinities, signed zero, subnormals, ties.
- [ ] `frames/` — `ENTER`/`LEAVE` for frame sizes 0–255 and every legal mask, nesting, `alloca`, tail call.
- [ ] `fuzz/` — random byte streams decode or trap, never crash, never read out of bounds.
- [ ] `traps/` — every vector, the frame layout, `IRET`, trace.

### M2 — the compiler's images run here

- [ ] Load a `krc`-built image whose format the compiler writes (blocked on the compiler's M2 image writer).
- [ ] Run it to a known exit status under `kr216-emu`; log the revision on both sides.
- [ ] Reproduce a fixed image byte-for-byte through a reset/reload cycle (`loader` determinism).
- [ ] `kr216-dis` matches the assembler's listing for the same image.

### M3 — C programs run here

- [ ] Execute a compiler-built C program (loops, pointers, aggregates) and compare output and exit status with the host build.
- [ ] Check the ABI the emulator can observe: argument registers, stack argument placement, frame layout, callee-saved registers, aggregate destination in R6.
- [ ] Verify the literal-typing regression (`long x = 32768;`) on the kr216 profile before arithmetic fixtures.

### M4 — debugger interface and unwinding

- [ ] Machine-level interface for `krdbg` `IDebugTarget`: step, budgeted run, halted/fault states, layout, register and memory access, disassembly, unwind, retired-instruction count, reset.
- [ ] Console maps to the `out`/`in`/`status` COM contract; `video()` reports absent.
- [ ] Backtrace through a known call chain matches expected frames (`frame = 0`, return PC at `FP + 2`).
- [ ] krdbg's variable-length disassembly change is a prerequisite, owned upstream (Q23).

### M5 — exceptions execute

- [ ] Execute the 16-bit exception descriptors once the compiler-side ADR (0054) and runtime exist; `has_eh` turns on there, not here.
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

## 8. Open items and risks

- **ISA draft, not frozen.** The encodings are normative from draft 0.2, but the emulator will disagree with the document somewhere in the first thousand vectors. When it does, decide which is right, fix both, and log it.
- **Image format dependency.** The loader (M1.4) is written against D33's container, which the compiler's image writer produces at its M2. Until then the loader is tested against hand-built images.
- **krdbg's variable-length change** (Q23) gates M4 and lives upstream in `..\krdbg`.
- **16-bit EH descriptor** (ADR 0054) is not written; `has_eh` is false and M5 is blocked until the compiler side lands.
- **BitWhiz reachability.** The ARM64 row is remote; if the machine is unreachable when a milestone closes, the gate records it as not verified rather than skipping silently.
- **Toolchain path fragility.** CMake and Ninja are not on `PATH` and the installed Visual Studio is newer than any generator this CMake knows; `build.cmd` exists so that no task composes a build command from memory.

## 9. Running log

- **2026-11-21T20:54:12-08:00** — *(backfilled; timestamp from `kr216-plan.md`'s 0.11 revision row)* Plan v0.11 approved and committed as `23e4686`: D34 fixes F0's canonical layout, and the review's corrections landed (aggregate destination, `long double`, `FRND`, `call r3`, mode numbering, unary F3). M0's decision and question gates closed.
- **2026-11-21T21:48:36-08:00** — *(backfilled; timestamp from commit `bbcaf6f`)* `kr216-isa.md` draft 0.1 written and committed: length map, five classes, `F0`–`F8`, semantics, traps, `SYS`, MMIO, 60 vectors, and the 16-decision register in §15. Not run: no build exists.
- **2026-11-21T21:48:41-08:00** — *(backfilled; timestamp from commit `4b96bca`)* Compiler-side registration pushed: ADR 0053 (renumbered from 0052 after the rebase), `docs/abi-kr216.md`, schema v2, D-050, and the indexes. Linux/ARM64 rows not applicable: documentation only.
- **2026-11-21T22:04:42-08:00** — §15's sixteen decisions approved by the owner and the approval recorded in the ISA header; this tracker created. Toolchain discovery on the Windows dev machine: `vswhere` found Visual Studio 18 Community (18.12.1), `cl` is 19.52.37029 for x64 after `vcvars64.bat`, CMake is the bundled 4.3.1-msvc1 (not on `PATH`, newest Visual Studio generator `Visual Studio 17 2022`), and Ninja ships beside it. Consequence recorded in §3: the Windows build uses Ninja inside `vcvars64`. No build or test run — there is no source tree yet.
