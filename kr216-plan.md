# kr216 — Architecture Plan

| | |
|---|---|
| Status | Approved — all 34 decisions accepted; no open questions |
| Version | 0.11 |
| Date | 2026-11-21T20:54:12-08:00 |
| Scope | CPU architecture and the C++ platform it has to support |
| Follow-on documents | `kr216-isa.md` (normative encodings), `docs/abi-kr216.md` (in the compiler repo), `kr216-cpp.md`, `kr216-toolchain.md` |

## Revision history

| Version | Date | Change |
|---|---|---|
| 0.1 | 2026-11-21T11:24:59-08:00 | Initial architecture baseline |
| 0.2 | 2026-11-21T11:36:11-08:00 | R10 designated `this`; four binary64 FP registers and their instruction family; `PUSH`/`POP` take a 16-bit register mask; addressing modes made uniform across all registers and made the canonical ISA form; byte loads split into signed (`LDBS`) and unsigned (`LDBU`) |
| 0.3 | 2026-11-21T12:59:41-08:00 | `ENTER`/`LEAVE` frame instructions; argument against a landing-pad operand in `ENTER`; exception tables described in more detail |
| 0.3.1 | 2026-11-21T13:04:04-08:00 | D23 confirmed by the owner: `ENTER`/`LEAVE` accepted as specified, landing pads stay table data. No technical change. |
| 0.4 | 2026-11-21T13:34:56-08:00 | Open questions Q1–Q18 answered and recorded; compiler path replaced: the existing retargetable `krcompiler` grows a kr216 backend and `krdbg` supplies the debugger (no C-first stage, no LLVM backend); D7, D13, D14, D21 accepted; D24, D25 added; Q19 (krcompiler's target contract) opened |
| 0.5 | 2026-11-21T14:02:40-08:00 | New Register class: `LD Rd,#imm16`, `LD Rd,[addr16]` and `ST [addr16],Rd` become 3 bytes; the 4-byte Long class is retired (byte forms and LEA d16 move to the `F1` escape at the same 4-byte cost); `CLR` and `JMP Rd` demoted to 2 bytes to pay for the three opcode rows; length map becomes a first-byte table (D26, proposed) |
| 0.5.1 | 2026-11-21T14:11:15-08:00 | D26 accepted by the owner: the 3-byte Register class and the range-table length map are the encoding baseline. D4 follows from it. No technical change. |
| 0.6 | 2026-11-21T14:24:17-08:00 | D5, D6, D8–D12, D16 and D22 accepted as specified. All 26 decisions are now Accepted; Q19 (krcompiler's target contract) is the only open question. |
| 0.7 | 2026-11-21T19:53:51-08:00 | Read `..\krcompiler\plan.md` and the target guide: Q19 answered; section 12 rewritten around the five target layers (profile, descriptor, assembler/KRO, backend, runtime/VM); object format moves from ELF32 to KRO with target ID 4; vtable/RTTI aligned to ADR 0025 scaled to 2-byte words; exception integration and its 16-bit gap recorded; D27–D29 added; Q20–Q22 opened |
| 0.8 | 2026-11-21T20:08:53-08:00 | Read `..\krdbg\docs\new-dbg-target-guide.md`: new section 11.8 on the debugger/VM interface; FP made mandatory for the KDBG frame convention; console reshaped to krdbg's out/in/status COM contract; the fixed-instruction-alignment gap recorded as Q23; D30 added; krdbg adapter added to the component table |
| 0.9 | 2026-11-21T20:16:10-08:00 | D29 accepted; Q20–Q23 resolved: long double aliases double, the descriptor gains a `this_register`, landing pads use R2/R3, descriptors exist only for functions with protected regions, and krdbg gains variable-length disassembly. D31 (KDBG in-image, matching KR32) and D32 (compiler-side work lives in the krcompiler repo) added. All 32 decisions accepted |
| 0.10 | 2026-11-21T20:25:25-08:00 | Q24/Q25 resolved: float arithmetic is strict binary32 (D21 changes; `FRND` added to the F3 family) and memory-class aggregates use ADR 0046's argument pointer with an R6 return destination. Q26–Q29 recorded: container image (D33), ISA/ABI documents next, library ladder accepted, upstream fixes before their milestones |
| 0.11 | 2026-11-21T20:54:12-08:00 | Owner approval after review: F0's operation field fixed at one byte plus two reserved specifier bits (D34); `long double`'s alias, `FRND`'s omission from the instruction overview, `call r3`'s byte and the virtual-call length corrected; §12.2 and the glossary aligned to the R6 aggregate destination; §9 numbered by generic mode with mode 5 = `d8` and mode 6 = `d16`; unary F3 operations fix `Fs = 0`; §11.8/§12.2 cite the live krcompiler and krdbg documents |

This is a plan, not a specification. It records what has been decided, why, and what is still
open. Exact encodings, operand-mode tables and relocation formats belong in `kr216-isa.md`;
this document fixes the shape those tables have to fit into.

---

## 1. Vision

kr216 is a 16-bit virtual CPU designed to be a good target for C++. It is a soft machine: the
reference implementation is an emulator, and the architecture is free to spend emulator
performance on code density and on features that make compiled C++ small and predictable.

Three ideas give the machine its character:

- **A uniform addressing model with a ladder of encodings.** Every address mode applies to
  every general register, and every operation has a canonical encoding; the short 1-, 2- and
  3-byte forms are compressions of that canonical form, chosen by the assembler. Density is
  bought without giving up orthogonality.
- **A C++-shaped register file.** R0 and R1 are constants, R10 is `this`, R11–R15 are flags,
  the exception frame, the frame pointer, the program counter and the stack pointer. The ABI
  is not an afterthought bolted onto a general-purpose ISA.
- **A real floating-point unit.** Four 64-bit IEEE-754 binary64 registers with the four basic
  operations, comparison, conversion, load/store and stack push/pop. Floating point is part of
  the machine, not a software library.

The project is deliberately architecture-first. The ISA, the ABI and the object model are
designed together, on paper, before an emulator or a compiler backend exists. Every stage of the
toolchain is then a check on the previous stage: the assembler validates the encodings, the
emulator validates the semantics, and the first program generated by `krcompiler`'s kr216
backend validates the ABI and the object model.

**Assumption A1.** "Program the system using C++" means C++ source compiled *to kr216* and run
on the kr216 platform, not host C++ driving the emulator. If that assumption is wrong, the
C++-facing sections of this plan shrink drastically and the ISA can be much simpler. This is
the first thing to confirm.

## 2. Goals

| ID | Goal | Measure |
|---|---|---|
| G1 | 16-bit, byte-addressable, little-endian machine | — |
| G2 | High code density | Average ≤ 2.5 bytes/instruction on compiled C++; 1-byte forms for the hottest ops |
| G3 | C++ is a first-class source language | ABI, object model and exceptions designed in, not retrofitted |
| G4 | 16 general registers, uniform addressing, compact encoding | Full register file reachable from every instruction; every mode usable with every register |
| G5 | Simple enough to implement correctly | A competent programmer can write a full emulator from the spec in ~2 weeks |
| G6 | Extensible | Reserved escape space for 32-bit ops, memory banking, more FP registers |
| G7 | Verifiable | Every instruction has encode/decode vectors and a conformance test |
| G8 | IEEE-754 binary64 floating point | 4 FP registers, four basic operations, exact binary64 results |

## 3. Non-goals (v1)

- A hardware implementation. The ISA should be implementable in an FPGA, but that is not the
  target and is not allowed to distort the design.
- More than 64 KiB of flat memory. See Q1 for the escape hatch.
- Threads, atomics, multiprocessing, memory protection, virtual memory.
- Self-hosting the toolchain. A 64 KiB machine cannot host a full C++ compiler.
- Binary compatibility with any existing platform.
- Real-time guarantees.
- An extended-precision `long double` (the type itself aliases `double`), decimal floating
  point, IEEE rounding modes other than round-to-nearest-even, and an FP status register. FP
  arithmetic produces IEEE default results and never raises an FP exception; the one
  FP-related trap is an invalid float↔integer conversion.
- Multiple privilege levels and an MMU.

## 4. Design principles

1. **The compiler is the customer.** Encoding decisions are justified by compiled-code
   frequency, not by orthogonality or beauty.
2. **One operation, several encodings.** The canonical integer form is complete; the short
   forms are compressions. The assembler always emits the shortest encoding that fits.
3. **A length rule the decoder can use.** The first byte selects the class through a small
   range table. The escape prefixes declare their own lengths, with one exception:
   `F4` reads the mode bits of its second byte to choose between 3 and 4 bytes.
4. **No hidden state.** Anything the ABI needs (flags, frame pointers, the exception frame,
   the FP file) is visible to the program.
5. **Fixed cost at the edges.** 1-byte short forms; 2-byte workhorse forms; escape hatches.
6. **Wrap, don't trap, on address overflow.** 16-bit address arithmetic is modulo 65536, as on
   every other 16-bit machine. Only the null page and unmapped I/O trap.

## 5. System overview

```
                        host process
   +-------------------------------------------------------+
   |  kr216-emu                                            |
   |   +-----------+   +-----------+   +----------------+  |
   |   |  decoder  |-->|  execute  |-->| memory + MMIO  |  |
   |   +-----------+   +-----------+   +----------------+  |
   |         ^              |                 |      |      |
   |         |              v                 v      v      |
   |   +-----------+   +-----------+      console  timer    |
   |   | debugger  |   | FPU F0-F3 |   (stdin/stdout)      |
   |   +-----------+   +-----------+                       |
   +-------------------------------------------------------+
                              |
                     program image (KRO-linked kr216 image)
```

The machine the runtime sees:

- one 64 KiB byte-addressable memory, little-endian;
- one 16-bit CPU with 16 general registers and a flags register;
- a floating-point unit with four 64-bit binary64 registers;
- memory-mapped console and timer;
- a `SYS` hypercall for host services (exit, file I/O, wall clock, debug output).

There is no MMU, no privilege level, and no kernel. The "OS" is a startup file and a small
runtime library.

## 6. Register model

### 6.1 General registers

| Reg | Name | Role | ABI | Saved by |
|---|---|---|---|---|
| R0 | `zero` | Reads as 0; writes discarded | constant | — |
| R1 | `one` | Reads as 1; writes discarded | constant | — |
| R2 | `a0` / `ret0` | Argument 0; return value (low word) | caller | caller |
| R3 | `a1` / `ret1` | Argument 1; return value (high word) | caller | caller |
| R4 | `a2` | Argument 2 | caller | caller |
| R5 | `a3` | Argument 3 | caller | caller |
| R6 | `t0` | Scratch / temporary; aggregate-return destination at call sites | caller | caller |
| R7 | `t1` | Scratch / temporary; indirect-call target | caller | caller |
| R8 | `s0` | General purpose, preserved across calls | callee | callee |
| R9 | `s1` | General purpose, preserved across calls | callee | callee |
| R10 | `this` | C++ `this` pointer; scratch in free functions | caller | caller |
| R11 | `flags` | Condition flags, interrupt enable, trace | — | exception entry |
| R12 | `efp` | Exception Frame Pointer (hardware traps) | — | exception entry |
| R13 | `fp` | Frame Pointer | callee | callee |
| R14 | `pc` | Program Counter | — | call / trap |
| R15 | `sp` | Stack Pointer | callee | callee |

Nine registers are allocatable (R2–R10). In a member function R10 holds `this`, leaving eight
for ordinary allocation; in a free function R10 is an ordinary caller-saved register. The ABI
section explains how the rest are spent.

### 6.2 Why R0 = 0 and R1 = 1

R0 = 0 buys the usual things: clear and compare-against-zero without an immediate, a zero base
register for absolute and generic addressing, a null pointer that reads as zero, and a sink for
discarded results. `push R0` also gives a 1-byte stack alignment adjustment.

R1 = 1 buys a constant that needs no immediate field: increment a pointer in a generic
addressing mode, materialise `true`, shift by one. On many 16-bit machines these are handled
by dedicated `INC`/`DEC`/shift-by-one opcodes, and kr216 has those too, so R1 is the weaker of
the two decisions.

**Cost:** two of sixteen registers are constants, and the allocatable file drops to nine (eight
in member functions). This is accepted for v1 with two mitigations: the register *roles* are
an ABI decision, not an encoding decision, so R1 can be reassigned later without changing a
single opcode; and we will revisit it against measured instruction frequencies once the C
compiler emits real code (Q2).

### 6.3 Flags (R11)

| Bit | Name | Meaning |
|---|---|---|
| 0 | Z | Result was zero |
| 1 | N | Result was negative (bit 15) |
| 2 | C | Carry out / no-borrow |
| 3 | V | Signed overflow; set by `FCMP` when the operands are unordered |
| 4 | I | Interrupts enabled |
| 5 | T | Single-step / trace trap |
| 6–15 | — | Reserved, read as 0, writes ignored |

- Arithmetic, logic, shift and compare instructions set flags; `MOV`, `LD`, `ST`, `PUSH` and
  `POP` do not.
- `CMP a, b` sets flags from `a - b` without writing a destination.
- Logical ops set N/Z and clear C/V. Shifts set C to the last bit shifted out.
- `FCMP Fd, Fs` sets `N = (Fd < Fs)`, `Z = (Fd == Fs)`, `V = unordered`, `C = 0`. A compiler
  tests ordinary less-than as `N=1 and V=0`, equality as `Z=1 and V=0`, and branches to an
  unordered handler on `V=1`.
- Branch conditions are derived from Z/N/C/V exactly as on ARM:

| # | Cond | Meaning | # | Cond | Meaning |
|---|---|---|---|---|---|
| 0 | EQ | Z=1 | 8 | HI | C=1 and Z=0 |
| 1 | NE | Z=0 | 9 | LS | C=0 or Z=1 |
| 2 | CS | C=1 | 10 | GE | N=V |
| 3 | CC | C=0 | 11 | LT | N≠V |
| 4 | MI | N=1 | 12 | GT | Z=0 and N=V |
| 5 | PL | N=0 | 13 | LE | Z=1 or N≠V |
| 6 | VS | V=1 | 14 | — | `CALL rel9` |
| 7 | VC | V=0 | 15 | — | `BRA rel9` |

Implicit flag setting (the x86 model) is chosen over an explicit "set flags" bit because it
saves an encoding bit on every ALU instruction, which is where density is won. The cost is
that a compiler cannot keep a live comparison across an intervening arithmetic instruction;
`LEA` exists as a flag-preserving add for address arithmetic.

### 6.4 Special registers

**PC (R14).** Reading PC yields the address of the next instruction. Only jump, call, return,
trap and `IRET` change control flow; arithmetic on PC is allowed but is just arithmetic.
PC-relative operands are relative to the address of the instruction that contains them.

**SP (R15).** Full-descending: SP points at the last occupied word. `push` stores at `SP-2`
and then decrements; `pop` loads and then increments. SP is always even under the ABI. The
stack grows toward lower addresses.

**FP (R13).** Conventional frame pointer, managed by the ABI, points at the saved-FP slot.
It exists because the C++ unwinder, the debugger and the backtrace all need a reliable frame
chain, and a 16-bit machine cannot afford DWARF CFI tables for every function.

**EFP (R12).** Head of the hardware exception frame chain; 0 when no trap is active. On a
trap the CPU pushes a frame `{previous EFP, faulting PC, flags}` and points EFP at it. Nested
traps chain through it, and `IRET` unwinds one frame. C++ exceptions are a *software* mechanism
and do not use EFP (section 11.4). The trap frame does **not** save the FP registers; a trap
handler that uses floating point must save F0–F3 itself.

**Reset state.** `PC = [0x0100]`, `SP = 0xFEF0`, `FP = 0`, `EFP = 0`, `R11 = 0`, other
registers undefined, FP registers undefined.

### 6.5 Floating-point registers

Four FP registers, `F0`–`F3`, each 64 bits wide and holding an IEEE-754 binary64 value. They
are a separate register file with a deliberately different profile:

| Property | General registers | FP registers |
|---|---|---|
| Count | 16 | 4 |
| Width | 16 bits | 64 bits |
| Values | integers, addresses | binary64 only |
| Addressable | yes, base of any address mode | no; only load/store |
| Encoding field | 4 bits | 2 bits |
| ABI | mixed caller/callee saved | all caller-saved |
| Instructions | the general set | the `F`-prefixed family (section 8.7) |
| Saved on trap | EFP/PC/flags by hardware | not saved; handler's responsibility |

The FP file is intentionally small. Four registers are enough for the expression trees that
dominate 16-bit code, they cost 32 bytes of state, and they keep the encoding fields narrow.
If profiling later shows spills dominating FP code, the count can grow to eight — the 2-bit
encoding field is the only thing that would have to change (Q17).

`float` values live in memory as 32 bits and are widened to binary64 on load (`FLDS`) and
narrowed on store (`FSTS`). All FP arithmetic is performed in binary64.

## 7. Memory model

### 7.1 Basics

- 64 KiB linear, byte-addressable, 16-bit addresses; arithmetic wraps modulo 65536.
- Little-endian for data and for immediate fields. Instruction bytes are fetched in order.
- 8-bit and 16-bit integer loads and stores; 32/64-bit integers are pairs of 16-bit words.
- An FP load or store moves 8 bytes as four consecutive little-endian words. `FLDS`/`FSTS`
  move 4 bytes and convert between binary32 and binary64.
- Unaligned access is allowed and executes correctly. The ABI still aligns multi-byte objects
  to 2 bytes; packed structures are supported.
- Instructions need not be aligned and may cross any byte boundary.
- No protection. The emulator traps accesses to the null page and to reserved regions; that is
  a debugging aid, not an architectural fault model.

### 7.2 Default memory map

| Range | Use |
|---|---|
| `0x0000`–`0x00FF` | Null guard. Any access traps (catches null dereference). |
| `0x0100`–`0x011F` | Reset vector and 15 exception vectors (16 × 16-bit) |
| `0x0120`–`0x01FF` | Boot information, runtime data, syscall trampoline |
| `0x0200`–`0xDFFF` | Program image (`.text`, `.rodata`, `.data`, `.bss`), then heap growing up |
| `0xE000`–`0xFEEF` | Stack, growing down from `0xFEF0` |
| `0xFF00`–`0xFF7F` | Memory-mapped I/O |
| `0xFF80`–`0xFFFF` | Reserved, traps |

The heap starts at the linker-defined `_end` and grows toward the stack. `sbrk` fails when it
would come within a 256-byte guard band of SP. A program with a 40 KiB image therefore gets
roughly 15 KiB of heap and 7.5 KiB of stack; small, but consistent with the machine class.

The map is the *default platform profile*. The linker script may place sections differently;
the emulator enforces only the null page, the I/O window and the reserved window.

### 7.3 Memory-mapped I/O

| Address | Device | Registers |
|---|---|---|
| `0xFF00` | Console | `OUT` (write a byte), `IN` (read a byte), `STATUS` (RX_READY, TX_READY, OVERRUN) |
| `0xFF10` | Timer | 16-bit counter, compare, control |
| `0xFF20` | System | halt, reset, free-running cycle counter (two words) |
| `0xFF30`–`0xFF7F` | Future | console cursor, RNG, block device |

The console is deliberately shaped like the debugger's three-operation COM contract (`out`,
`in`, `status`), with the status bits matching `krdbg::ComPortStatus` (`rx_ready`, `tx_ready`,
`overrun`). A four-register UART-style map would have worked, but the adapter would then have to
synthesise the handshake; this way the debug adapter is a thin mapping over the same device the
guest uses.

Polled I/O in v1. Interrupts are architecturally defined (I flag, vectors) but no device
raises one until there is a runtime that can use it.

### 7.4 Host services

`SYS` (1-byte system instruction) invokes the emulator directly. The call number is in R7,
arguments are in R2–R5 and the result is returned in R2; R7 is not preserved. This is a
virtual-machine hypercall, not an architectural trap: it does not go through the vector table
and does not push an exception frame. Initial calls: `exit`, `write(fd, buf, len)`, `read`,
`open/close/seek` (v2), `time`, `debug_write`.

## 8. Instruction encoding framework

### 8.1 Length classes

The first byte selects the length class. The map is a small range table rather than a
leading-bits count, because the short class is split: two of its rows were repurposed for the
Register class (section 8.5). A decoder resolves the length from the first byte alone, before
fetching the rest.

| First byte | Class | Length | Opcodes |
|---|---|---|---|
| `0x00`–`0x4F` | Short | 1 byte | 80 |
| `0x50`–`0x6F` | Register | 3 bytes | 32 |
| `0x70`–`0x7F` | Short (system) | 1 byte | 16 |
| `0x80`–`0xBF` | Compact | 2 bytes | 64 |
| `0xC0`–`0xDF` | Normal | 3 bytes | 32 |
| `0xE0`–`0xEF` | Register | 3 bytes | 16 |
| `0xF0`–`0xFF` | Escape | 2–6 bytes, prefix-declared | 16 prefixes |

Opcode fields are packed from the most significant bit of the first byte; immediates and
addresses are little-endian, which keeps them in natural memory order.

### 8.2 Short class (1 byte)

Layout: `0 OOO RRRR` — 3-bit operation, 4-bit register (any of R0–R15).

| OOO | Operation | Notes |
|---|---|---|
| 0 | `PUSH Rd` | Short form of `PUSH #(1<<Rd)` |
| 1 | `POP Rd` | Short form of `POP #(1<<Rd)` |
| 2 | `INC Rd` | Sets flags |
| 3 | `DEC Rd` | Sets flags |
| 4 | `CALL Rd` | Indirect call (function pointers, virtual calls) |
| 5–6 | — | Used by the Register class (section 8.5) |
| 7 | `SYS SSSS` | 16 zero-operand/system encodings |

System encodings (`0 111 SSSS`): `RET`, `NOP`, `BRK`, `SYS`, `EI`, `DI`, `HALT`, `IRET`,
`RDCYCLE` (result in R2), and 7 reserved.

`JMP Rd` is no longer a one-byte instruction: it is compact opcode 31 (2 bytes). `CLR Rd` is
an alias for `MOV Rd, R0` (2 bytes). Both were traded for the three Register-class rows that
promote the 16-bit-operand instructions (section 8.5).

`PUSH` and `POP` are architecturally mask operations (section 8.4); the 4-bit register field
is simply the single set bit of the mask. The assembler uses this form whenever the mask has
exactly one bit, and the mask form otherwise. Because PC and SP may not appear in a mask, the
short forms `PUSH R14`, `PUSH R15`, `POP R14` and `POP R15` are illegal instructions.

Keeping six one-byte operations with a full 4-bit register field, rather than shrinking the
register field to reach more operations, is deliberate. Single-register saves and restores
(`push fp`, `push r8`, `pop r8`, `pop fp`) are one byte each, which keeps leaf frames and
hand-written sequences compact; functions with a real frame use `ENTER`/`LEAVE` (section 8.8).

### 8.3 Compact class (2 bytes)

Two subfamilies, selected by bit 13:

```
Register form:  10 0 OOOOO Ra Rb        (5-bit op, full register file)
Branch form:    101 CCCC ddddddddd      (4-bit condition, 9-bit signed displacement)
```

**Register form** (32 operations, any two registers):

| Op | Name | Op | Name | Op | Name | Op | Name |
|---|---|---|---|---|---|---|---|
| 0 | `MOV` | 8 | `CMP` | 16 | `LDBU` | 24 | `LD-` |
| 1 | `ADD` | 9 | `MUL` | 17 | `LDBS` | 25 | `ST-` |
| 2 | `ADDC` | 10 | `MULH` | 18 | `STB` | 26 | `LDBU-` |
| 3 | `SUB` | 11 | `SHL` | 19 | `LD+` | 27 | `LDBS-` |
| 4 | `SUBB` | 12 | `SHR` | 20 | `ST+` | 28 | `STB-` |
| 5 | `AND` | 13 | `SAR` | 21 | `LDBU+` | 29 | `NEG` |
| 6 | `OR` | 14 | `LD` | 22 | `LDBS+` | 30 | `NOT` |
| 7 | `XOR` | 15 | `ST` | 23 | `STB+` | 31 | `JMP` |

`LD Rd, [Rb]` and `ST [Rb], Ra` are 2 bytes for any register pair. `+`/`-` are post-increment
and pre-decrement by the operand size (1 for byte forms, 2 for word forms). `LDBU` loads a
byte and zero-extends; `LDBS` loads a byte and sign-extends; `STB` stores the low byte.
Unary operations use `Rb = R0` and are still 2 bytes. `JMP Rd` uses `Rb = R0` as well.
`DIV`/`DIVS` are not here — they moved to the Normal class, where their lower frequency
justifies 3 bytes.

**Branch form** uses a 9-bit displacement relative to the next instruction, range −256…+255,
which covers nearly every branch a C++ compiler emits. Condition 15 is `BRA` (always) and
condition 14 is `CALL rel9`, so near calls and all branches are 2 bytes.

### 8.4 Normal class (3 bytes)

`110 OOOOO` followed by a 16-bit payload whose layout is defined per operation:

| Op | Form | Payload |
|---|---|---|
| 0 | `LD Rd, [Rb+d8]` | `RRRR BBBB`, then `d8` |
| 1 | `ST [Rb+d8], Rs` | `RRRR BBBB`, then `d8` |
| 2 | `LDBU Rd, [Rb+d8]` | `RRRR BBBB`, then `d8` |
| 3 | `LDBS Rd, [Rb+d8]` | `RRRR BBBB`, then `d8` |
| 4 | `STB [Rb+d8], Rs` | `RRRR BBBB`, then `d8` |
| 5 | `LD Rd, #s8` (alias) | `RRRR 0000`, then signed `imm8` |
| 6 | `LD Rd, #u8` (alias) | `RRRR 0000`, then unsigned `imm8` |
| 7 | `ADD Rd, #s8` | `RRRR 0000`, then `imm8` |
| 8 | `SUB Rd, #s8` | `RRRR 0000`, then `imm8` |
| 9 | `CMP Rd, #s8` | `RRRR 0000`, then `imm8` |
| 10 | `CMP Rd, #u8` | `RRRR 0000`, then `imm8` |
| 11 | `AND Rd, #u8` | `RRRR 0000`, then `imm8` |
| 12 | `OR Rd, #u8` | `RRRR 0000`, then `imm8` |
| 13 | `XOR Rd, #u8` | `RRRR 0000`, then `imm8` |
| 14 | `LEA Rd, [Rb+d8]` | `RRRR BBBB`, then `d8` |
| 15 | `JMP addr16` | `addr16` |
| 16 | `CALL addr16` | `addr16` |
| 17 | `LDBU Rd, [PC+d8]` | `RRRR 0000`, then `d8` |
| 18 | `LDBS Rd, [PC+d8]` | `RRRR 0000`, then `d8` |
| 19 | `SETcc Rd` | `RRRR CCCC 0000 0000` |
| 20 | `SHL Rd, #n` | `RRRR 0000`, then `imm8` |
| 21 | `SHR Rd, #n` | `RRRR 0000`, then `imm8` |
| 22 | `SAR Rd, #n` | `RRRR 0000`, then `imm8` |
| 23 | `PUSH #mask16` | `mask16` |
| 24 | `POP #mask16` | `mask16` |
| 25 | `DIV Rd, Rb` | `RRRR BBBB` |
| 26 | `DIVS Rd, Rb` | `RRRR BBBB` |
| 27 | `SXTB Rd` | `RRRR 0000 0000 0000` |
| 28 | `LEAVE #mask16` | `mask16` |
| 29–31 | reserved | — |

This class carries displacement addressing, small immediates, near absolute jumps and calls,
PC-relative byte literals, `setcc`, the register mask push/pop, and the rarer arithmetic.

**Mask push and pop.** `PUSH #mask` stores one word for each set bit: bits are processed in
ascending register order, R0 first and R15 last, so the highest-numbered register ends up at
the lowest address. `POP #mask` reads in descending order, R15 first and R0 last, which exactly
reverses a `PUSH` of the same mask. Each set bit moves SP by 2. Bits 14 (PC) and 15 (SP) are
reserved and must be zero; a mask with either bit set is an illegal instruction. Bits 0 and 1
are legal and push the constants 0 and 1. A mask instruction is atomic with respect to
interrupts: the emulator only delivers interrupts between instructions.

### 8.5 Register class (3 bytes)

A 3-byte instruction has 24 bits. With a 16-bit immediate or address, only 8 bits remain for
the operation and the register — exactly one byte. So the register moves into the first byte,
and each operation owns sixteen first-byte values, one per register:

| First byte | Form | Encoding |
|---|---|---|
| `0x50`–`0x5F` | `LD Rd, #imm16` | `0101 RRRR`, then `imm16` (LE) |
| `0x60`–`0x6F` | `LD Rd, [addr16]` | `0110 RRRR`, then `addr16` (LE) |
| `0xE0`–`0xEF` | `ST [addr16], Rd` | `1110 RRRR`, then `addr16` (LE) |

These are the three instructions whose 16-bit value is most common in compiled code: loading a
constant, reading a global, writing a global. All three accept all sixteen registers and any
16-bit value, and each costs one byte less than the retired 4-byte Long class.

The rows are paid for by `CLR` (now `MOV Rd, R0`) and `JMP Rd` (now compact opcode 31), two
one-byte rows, plus the old `0xE0` row. The other operations that carry a 16-bit value — byte
loads and stores to absolute addresses, and `LEA` with a 16-bit displacement — keep their
4-byte cost but move to the `F1` escape (section 8.6).

`LD Rd, #imm16` also makes the 8-bit `#s8`/`#u8` loads (Normal opcodes 5 and 6) redundant:
all three are 3 bytes. They remain legal aliases, and the assembler emits the Register form.

### 8.6 Escape class

`1111 SSSS` selects a prefix. The prefix declares the payload format and length:

| Prefix | Use | Length |
|---|---|---|
| `F0` | Generic two-operand form: a one-byte operation code, then two 7-bit operand specifiers (mode 3 bits + register 4 bits) in two bytes with two reserved bits, then extension words for an immediate or a 16-bit displacement. The canonical form of the integer ISA. | 4–6 |
| `F1` | Long-form integer operations: one byte of `OOOO RRRR`, then a 16-bit address or displacement. Ops 0–2 are `LDBU`/`LDBS`/`STB` with an absolute address, ops 3–4 are `LEA` with `PC+d16` or `SP+d16`. | 4 |
| `F2` | 32-bit and 64-bit integer operations | reserved |
| `F3` | FP register-register operations | 2 |
| `F4` | FP load/store (all address modes) | 3–4 |
| `F5` | FP ↔ integer conversions | 3 |
| `F6` | FP push/pop | 2 |
| `F7` | `ENTER` short: frame size 8 bits, saved-register mask 16 bits | 4 |
| `F8` | `ENTER` long: frame size 16 bits, saved-register mask 16 bits | 5 |
| `F9`–`FF` | Reserved; must trap until defined | — |

The four-byte generic form is `F0`, the operation byte and the two operand-specifier bytes; any
extension the operand modes need follows — an 8-bit immediate or displacement in the fifth byte,
a 16-bit one in the fifth and sixth. The operation byte gives the generic form its own
256-operation space instead of borrowing encoding space from the short classes, and the two bits
the 7-bit specifiers leave unused are reserved and must be zero.

The generic form is the escape hatch that keeps the short classes dense: anything not covered
by a short form is still encodable without consuming opcode space that the compiler's hot
paths need.

### 8.7 Floating-point instruction family

**Register-register (`F3`, 2 bytes).** `F3` followed by `OOOO Fd Fs` (4-bit operation, 2-bit
destination, 2-bit source):

| Op | Instruction | Op | Instruction |
|---|---|---|---|
| 0 | `FADD Fd, Fs` | 5 | `FABS Fd` |
| 1 | `FSUB Fd, Fs` | 6 | `FSQRT Fd` |
| 2 | `FMUL Fd, Fs` | 7 | `FMOV Fd, Fs` |
| 3 | `FDIV Fd, Fs` | 8 | `FCMP Fd, Fs` |
| 4 | `FNEG Fd` | 9 | `FRND Fd` |
| | | 10–15 | reserved |

For the single-operand operations (`FNEG`, `FABS`, `FSQRT`, `FRND`) the source field is unused:
the assembler encodes `Fs = 0` and the disassembler prints the one operand.

`FRND` rounds the binary64 value in `Fd` to the nearest binary32 value and widens it back, so a
`float` operation can run in the binary64 datapath and still produce a strict binary32 result
(section 11.1).

**Load/store (`F4`, 3–4 bytes).** `F4` followed by `OOOO Fd MM`, then `BBBB 0000`, then `d8`
only when `MM = 01`. `OOOO`: 0 `FLD` (8-byte load), 1 `FST` (8-byte store), 2 `FLDS` (float
load + convert), 3 `FSTS` (convert + float store), 4–15 reserved. `MM`: 00 `[Rb]`, 01
`[Rb+d8]`, 10 `[Rb]+` (post-increment by 8, or 4 for `FLDS`/`FSTS`), 11 `-[Rb]`
(pre-decrement). Every mode accepts every general register as base.

**Conversions (`F5`, 3 bytes).** `F5`, `OOOO Fd 00`, `RRRR 0000`. `OOOO`: 0 `I2F Fd, Rb`
(signed), 1 `U2F Fd, Rb` (unsigned), 2 `F2I Rb, Fd` (truncate toward zero), 3 `F2U Rb, Fd`.
A conversion of NaN or an out-of-range value traps as an illegal operand; C++ leaves the
behaviour undefined, and trapping makes the bug visible.

**Push/pop (`F6`, 2 bytes).** `F6`, `OOOOOO Fd`. `FPUSH Fd` stores 8 bytes at `SP-8` and
decrements; `FPOP Fd` loads 8 and increments. SP stays even.

FP arithmetic follows IEEE-754 binary64 with round-to-nearest-even, no traps, no status
register: overflow produces infinity, invalid operations produce a quiet NaN, division by zero
produces infinity. NaN payloads and sign of zero are not specified beyond IEEE requirements.
`FSQRT` is correctly rounded. All operands live in binary64; a `float` operation is computed in
binary64 and rounded to binary32 with `FRND`, so the FPU never needs a second datapath.

### 8.8 Frame instructions

`ENTER` and `LEAVE` make the ABI frame a pair of instructions instead of a compiler-chosen
sequence. Prologue and epilogue are the most predictable code in a program and a large share of
its bytes; one definition of the frame layout is also easier to document, test, unwind and
debug than whatever each compiler version happens to emit.

**`ENTER #frame, #mask`** — 4 bytes when `frame ≤ 255` (`F7`), 5 bytes otherwise (`F8`):

```
sp -= 2; [sp] = fp      ; save the caller's frame pointer
fp = sp                 ; FP points at the saved FP slot
push mask               ; ascending order, exactly as PUSH #mask
sp -= frame             ; locals and outgoing arguments
```

**`LEAVE #mask`** — 3 bytes (Normal class, opcode 28):

```
sp = fp - 2*popcount(mask)
pop mask                ; descending order, exactly as POP #mask
fp = [sp]; sp += 2      ; sp now points at the return address
```

`LEAVE` restores the frame and leaves the return address on top of stack; it does not pop it.
A normal epilogue is `LEAVE #mask; RET` (4 bytes). A tail call is `LEAVE #mask; JMP target`
(4–6 bytes), which hands the original return address to the callee.

Computing SP from FP rather than adding the frame size back means frames that are extended at
runtime (`alloca`, variable-length arrays) unwind correctly, and a function with several exit
paths pays only the 3-byte `LEAVE` at each.

Mask bits 13 (FP), 14 (PC) and 15 (SP) are illegal in both instructions: FP is saved and
restored implicitly, and the other two have no meaning in a frame mask. The frame size must be
even, and the assembler rejects an odd immediate. Neither instruction touches the flags or any
exception state (section 11.4).

A leaf function with no locals and nothing to save does not use these instructions: its
`push fp; mov fp, sp` (3 bytes) and `pop fp; ret` (2 bytes) are smaller than `ENTER #0, #0`
plus `LEAVE #0` plus `RET` (8 bytes).

For a function with a frame and two saved registers, prologue plus epilogue falls from
9 + 8 = 17 bytes to 4 + 4 = 8 bytes.

### 8.9 The encoding hierarchy

The integer ISA has one canonical form and a ladder of compressed encodings:

1. **Generic** (`F0`, 4–6 bytes): any two-register operation, any addressing mode, any
   register. This is the reference form; the ISA definition lives here.
2. **Register** (3 bytes): a full 16-bit immediate or address, with the destination register
   in the opcode byte — `LD Rd,#imm16`, `LD Rd,[addr16]`, `ST [addr16],Rd`.
3. **Normal** (3 bytes): displacement addressing, small immediates, mask stack ops, the
   remaining 16-bit forms under `F1`.
4. **Compact** (2 bytes): the workhorse — register-register ALU, `[Rb]` and auto-increment
   loads/stores, conditional branches, `JMP Rd`.
5. **Short** (1 byte): single-register push/pop/inc/dec/call and the system ops.

The assembler emits the shortest encoding whose fields cover the operands; the disassembler
always prints the architectural operation, not the encoding. Conformance tests must show that
every short form produces the same result as the equivalent generic form. `ENTER` and `LEAVE`
are multi-step frame operations, not operand-mode instructions, and sit outside this ladder.

### 8.10 Reserved encodings

Every unallocated encoding in every class is an illegal instruction and traps. This is both
a forward-compatibility rule and a decoder fuzzing target.

## 9. Addressing modes

Addressing modes are uniform: every mode below accepts any of the sixteen general registers,
both as the base and as the target. Integer loads and stores have the full set; FP loads and
stores use modes 0, 2, 3, 4 and 5, with the 8-bit displacement of section 8.7.

| Mode | Name | Syntax | Short forms | Extension |
|---|---|---|---|---|
| 0 | Register direct | `Rd` | all classes | — |
| 1 | Immediate | `#imm8`, `#imm16` | Normal, Register | `imm8` byte, `imm16` word |
| 2 | Register indirect | `[Rb]` | 2-byte `LD`/`ST`, 3-byte FP | — |
| 3 | Auto-increment | `[Rb]+` | 2-byte `LD+`/`ST+`, 3-byte FP | — |
| 4 | Auto-decrement | `-[Rb]` | 2-byte `LD-`/`ST-`, 3-byte FP | — |
| 5 | Indirect with 8-bit offset | `[Rb+d8]` | Normal `d8`, 4-byte FP | `d8` byte |
| 6 | Indirect with 16-bit offset | `[Rb+d16]` | generic only | `d16` word |
| 7 | (reserved) | `[Rb+Ri]` when defined | — | — |

Notes:

- Auto-increment/decrement step by the operand size: 1 for byte loads/stores, 2 for word,
  8 for double, 4 for `FLDS`/`FSTS`.
- Absolute addressing has its own 3-byte form in the Register class (`LD Rd,[addr16]`,
  `ST [addr16],Rd`); in the generic form it is `[R0 + d16]`, since R0 reads as zero.
  `[SP + d8]` is the natural stack-relative form, again with no special case in the encoding.
- Indexed addressing (`[Rb+Ri]`) is reserved as generic mode 7. Until it is defined, the
  compiler synthesizes it with a shift and an add; this is the most likely candidate for a
  later revision (Q14).
- The mode table is a *uniform* guarantee: there is no mode that only works for R0–R7, and no
  register that cannot be a base. The conformance suite tests the cross product.

## 10. Instruction set overview

| Category | Instructions |
|---|---|
| Move | `MOV`, `LD`/`ST` (word), `LDBU`/`LDBS`/`STB` (byte), `LEA`, `SXTB`, `PUSH`/`POP` (mask), `CLR` |
| ALU | `ADD`, `ADDC`, `SUB`, `SUBB`, `NEG`, `CMP`, `AND`, `OR`, `XOR`, `NOT`, `INC`, `DEC` |
| Shift | `SHL`, `SHR`, `SAR` by register (2 B) or immediate (3 B) |
| Multiply | `MUL` (low word), `MULH` (high word); 32×32 built from 16×16 partial products |
| Divide | `DIV`/`DIVS` (3 B); divide by zero raises the illegal-operand trap (vector 3) |
| Branch | `Bcc rel9`, `BRA rel9`, `CALL rel9`, `JMP Rd`, `CALL Rd`, `JMP addr16`, `CALL addr16`, `RET` |
| Stack | `PUSH`/`POP` mask, FP `FPUSH`/`FPOP`, stack-relative addressing |
| Frame | `ENTER #frame, #mask` (4–5 B), `LEAVE #mask` (3 B) |
| System | `SYS`, `BRK`, `HALT`, `EI`, `DI`, `IRET`, `RDCYCLE`, `NOP` |
| FP | `FADD`, `FSUB`, `FMUL`, `FDIV`, `FNEG`, `FABS`, `FSQRT`, `FMOV`, `FCMP`, `FRND`, `FLD`/`FST`, `FLDS`/`FSTS`, `I2F`, `U2F`, `F2I`, `F2U`, `FPUSH`/`FPOP` |
| Bit | `BTST`/`BSET`/`BCLR` in the Normal class if frequency justifies it (Q7) |

`ADDC`/`SUBB` plus the carry flag are the 32- and 64-bit arithmetic story. `MULH` exists so
that `long` multiplication can be synthesized without a 32-bit multiplier: 32×32 products are
built from 16×16 partial products. That is the standard cost on a 16-bit machine and is why
64-bit division lives in the runtime library rather than in the opcode table.

## 11. The C++ platform

### 11.1 Data model

| Type | Size | Align | Notes |
|---|---|---|---|
| `bool` | 1 | 1 | 0 or 1 |
| `char`, `signed/unsigned char` | 1 | 1 | 8-bit bytes |
| `short` | 2 | 2 | |
| `int` | 2 | 2 | `INT_MAX` = 32767; this is the big source-compatibility constraint |
| `long` | 4 | 2 | |
| `long long` | 8 | 2 | 64-bit division is a runtime helper |
| `float` | 4 | 2 | stored as binary32; widened to binary64 on load; f32 arithmetic rounds via `FRND` |
| `double` | 8 | 2 | binary64; lives in F0–F3 for arithmetic |
| `long double` | 8 | 2 | alias for `double` (Q20) |
| pointer, reference, member pointer | 2 | 2 | 64 KiB address space |
| `size_t`, `ptrdiff_t` | 2 | 2 | 16-bit; container sizes max 65535 |
| `intmax_t` | 8 | 2 | follows `long long` |
| `wchar_t` | 2 | 2 | |
| `char16_t` / `char32_t` | 2 / 4 | 2 / 2 | |

Plain `char` is signed (the KRCompiler profile flag `char_unsigned = false`). That matches
KR32 and the desktop expectation; code that needs bytes rather than small numbers uses
`unsigned char` or `std::uint8_t` explicitly.

The 16-bit `int` is non-negotiable on a 16-bit machine: making `int` 32 bits would double
the size of the most common arithmetic and make every loop counter a two-register value.
Code that assumes a 32-bit `int` needs `long` or `std::int32_t` explicitly.

This is not one of KRCompiler's existing data models (`Ilp32`, `Ip16`, `Lp64`): it is an
IP16-shaped model with a 32-bit `long` and a 64-bit `long long`, so the target adds a new
profile value and an ADR in the compiler (section 12.2). The compiler's narrow-target literal
typing defect — `long x = 32768;` is currently rejected on a 16-bit `int` target — must be
fixed upstream before kr216 arithmetic tests can be trusted.

`float` arithmetic is strict binary32 (`FLT_EVAL_METHOD = 0`): the value is widened to binary64
on load, the operation runs in binary64, and `FRND` rounds the result back to binary32 precision
before it is used. That is one extra instruction per f32 arithmetic operation, which is the
right trade on a machine where floating point is rare and a second datapath would be waste; the
alternative, evaluating `float` in binary64 (x87-style excess precision), would require the
compiler's front end to promote float expressions and would diverge from the f32 operations its
IR already emits (Q24).

### 11.2 Calling convention

| Role | Registers |
|---|---|
| `this` | R10 |
| Integer/pointer arguments 0–3 | R2, R3, R4, R5 |
| FP arguments 0–1 | F0, F1 |
| Return (integer/pointer) | R2 (low word), R3 (high word for 32-bit) |
| Return (floating point) | F0 |
| Return of aggregate | Memory-class destination in R6, loaded just before CALL; R2–R5 stay arguments |
| Aggregate argument | Invisible pointer to a caller-owned copy, one integer argument word; callee copies |
| Caller-saved | R2–R7, R10, F0–F3 |
| Callee-saved | R8, R9, FP, SP |
| Not preserved | Flags, EFP |

The integer and FP argument files are independent: a function may take four integer arguments
and two doubles without conflict. Arguments that do not fit their register file go on the
stack, each integer slot 2 bytes and each FP slot 8 bytes.

Aggregates are memory-class on kr216 (`aggregate_rules = MemoryOnly`, ADR 0046). An aggregate
argument is an invisible pointer to a caller-owned copy and consumes one integer argument word;
the callee copies the object into its own frame, so modifying the parameter leaves the caller's
copy untouched. An aggregate return destination is loaded into R6 just before the call; R6 is an
ordinary caller-saved scratch at all other times, and R2–R5 keep their argument roles. This
mirrors KR32's R4 destination rather than shifting the argument registers (Q25).

`this` in R10 is caller-saved, so a member function that calls another member and then still
needs its own `this` must preserve it — in R8/R9 or a stack slot. That is the price of freeing
the first argument register; it is accepted (Q13), and the alternative is recorded there. The
compiler descriptor gains a `this_register` field (Q21), so R10 is a declared part of the
target's calling convention rather than an informal convention.

Variadic and unprototyped functions take *all* arguments on the stack (the old ARM APCS
rule), including floating point. This keeps `va_start` trivial — a pointer to the first stack
argument — at the cost of one extra push per variadic call, which is the right trade for
`printf`-style code.

Frame layout:

```
        higher addresses
   +----------------------+
   | caller's frame       |
   +----------------------+
   | arg 4                |  <- FP + 4      first stack argument
   | arg 5                |  <- FP + 6
   +----------------------+
   | return address       |  <- FP + 2      pushed by CALL
   +----------------------+
   | saved FP             |  <- FP           saved by ENTER (or by push fp)
   +----------------------+
   | saved R8, R9         |  (ascending push order)
   | locals, spills       |
   | outgoing args        |
   +----------------------+  <- SP
        lower addresses
```

Prologue: `ENTER #frame, #mask` — 4 bytes for frames up to 255, 5 bytes otherwise. It saves
FP, points FP at the saved slot, pushes the registers named by the mask and subtracts the frame
size, producing exactly the layout above. Epilogue: `LEAVE #mask` (3 bytes) followed by `RET`
(1 byte); `LEAVE` restores SP from FP, pops the masked registers and the saved FP, and leaves
the return address on top for `RET` (or for a tail-call `JMP`). The mask is the same immediate
in both instructions.

A leaf function with no locals and no callee-saved registers does not use `ENTER`/`LEAVE`:
`push fp; mov fp, sp` and `pop fp; ret` are smaller. Everything else uses the pair (section
8.8).

**FP is mandatory in every function.** There is no `-fomit-frame-pointer` profile, because the
unwinder and the debugger both read `[FP]`: KDBG's frame model records the offset from the frame
pointer to the saved caller frame, and `ENTER` makes that offset 0 with the return PC at `FP+2`
(section 11.8). A leaf function that keeps `push fp` still has a valid frame record.

Return: `R2` for 16-bit scalars and pointers, `R2:R3` for 32-bit, `F0` for floating point, and
memory-class aggregates through the R6 destination. Register returns for small aggregates need
an ADR 0046 extension and are out of scope for now.

Name mangling is KRCompiler's Itanium-like `_Z` scheme, including its `name__tag` form for
template instances and `_ZTV`/`_ZTI`/`_ZTS` for metadata; `extern "C"` is unmangled. There is no
per-target mangling.

### 11.3 Object model

The object model is not ours to invent: KRCompiler fixes class layout (ADR 0010), dynamic
dispatch and RTTI (ADR 0025), and aggregate rules (ADR 0046). kr216 scales the same scheme to
2-byte words:

| Concern | Design |
|---|---|
| `vptr` | First word of every polymorphic object; 2 bytes overhead |
| `_ZTV<C>` | Weak global: `[type_info address, virtual fn 0, virtual fn 1, ...]` |
| vptr value | Address of `_ZTV<C> + 2`, so slot *k* is `vptr + 2k` and the typeinfo is at `vptr - 2` |
| `_ZTI<C>` | Weak global: `[0, name address, flags, base typeinfo address]` — four 2-byte words |
| `_ZTS<C>` | NUL-terminated class name |
| Inheritance | Single inheritance only: base at offset 0, so upcasts are free |
| Multiple inheritance | Later compiler milestone (v1.2); needs thunks and secondary vtables |
| Virtual call | `ld r7,[r10]`; `ld r7,[r7+2i]`; `call r7` — 5 bytes for slot 0, 6 once the offset needs a `d8` byte |
| RTTI | Always emitted for polymorphic classes (the compiler has no `-fno-rtti` yet); `dynamic_cast` calls `__krc_dynamic_cast` |
| Static init | `.init_array` walked by startup; `__cxa_atexit` for destructors |
| Local statics | 1-byte guard + `__cxa_guard_acquire/release`; single-threaded, so the guard is a byte test |
| `new`/`delete` | Runtime allocator over the bump/free-list heap |
| `operator new` failure | Throws `std::bad_alloc` once exceptions exist; aborts before that |

The important change from the earlier draft is the placement of the typeinfo: it sits *before*
the vptr (`vptr - 2`, Itanium "slot -1"), not at slot 0, and the vptr points at the first
virtual function slot. That removes the `+2` from every virtual call and makes `typeid` a load
from `[vptr-2]`. The `_ZTV`/`_ZTI`/`_ZTS` globals are weak, so the linker keeps one copy per
class and type identity holds across translation units; krld's weak/COMDAT merging is what makes
that work.

Because `this` lives in R10, the virtual call sequence uses R7 as the scratch register and
needs no argument shuffling. Constructors and destructors receive `this` in R10 like any other
member function; a deleting destructor is called through the vtable slot by the runtime.

### 11.4 Exceptions

C++ needs two things when a `throw` happens: find the innermost matching `catch`, and run the
destructors of every fully-constructed local object in every frame the unwinder crosses. The
compiler knows, for each instruction that can throw, which objects are live and which handlers
are in range; the runtime has to act on that.

Two candidate mechanisms:

| | EFP chain (`setjmp`-style) | Table-driven (recommended) |
|---|---|---|
| Entry cost | 2–4 words stored per frame that needs cleanup or catch | Zero per frame |
| Throw cost | Walk the chain | Walk the FP chain, look up the per-function table |
| Destructors in frames without `try` | Not run unless every such frame registers | Run correctly |
| Several `try` regions in one function | Handler pointer must be updated per region | PC ranges encode it |
| Compiler complexity | Low | High (call-site ranges, landing pads, type tables) |
| Front-end fit | Needs custom EH lowering | Matches the Itanium model most C++ front-ends use |
| State for partial construction | Still needed (a selector) | Still needed (a selector) |

**Recommendation: table-driven unwinding**, using the FP chain as the frame walk and a
per-function table mapping PC ranges to landing pads and cleanups. The FP chain already exists
for the debugger and gives the unwinder the caller's FP and return address; each function's
table entry records its frame size, so the caller's SP is recoverable exactly. `throw`
allocates the exception object on the heap, calls the unwinder, and the unwinder runs cleanups
frame by frame until a matching `catch` is found.

**Why `ENTER` does not take a landing-pad pointer.** It is the obvious design — one handler
address per frame, a linked chain through EFP, `throw` follows the chain — and it is what small
embedded C++ runtimes do. It does not fit C++ well enough:

1. **One frame can have several landing pads.** A function with two `try` blocks, or with code
   before and after a `try`, needs a different action depending on which call site threw. A
   single pointer installed at `ENTER` is wrong for code outside the try, and updating it at
   every region entry and exit turns "zero cost when not throwing" into per-region stores.
2. **Cleanup depends on how far execution got.** A landing pad must destroy only the objects
   whose construction finished; the compiler emits a selector variable and the landing pad
   switches on it. The handler pointer does not remove that state.
3. **Frames without handlers still have to be walked.** The unwinder must restore SP and FP and
   continue past them. The FP chain plus a frame-size entry does that with no per-frame word.
4. **The committed C++ path is `krcompiler`** (D14), and it already implements table-driven
   exception descriptors for KR32 (ADR 0026/0027). A table format the compiler can emit is far
   cheaper than a frame-resident handler pointer it would have to be taught.

So the landing pad is data in the function's unwind entry, not a field in the frame. An entry
records the function's PC range, its frame size and saved-register mask, a call-site table
(PC range → landing pad, cleanup flag, action), and a type table for `catch` matching. The
unwinder walks the FP chain, looks up each frame's PC (the throw site for the innermost frame,
the return address for callers), restores the frame's saved registers from the entry, and
either transfers to a landing pad (cleanup or catch) or moves on. A landing pad receives the
exception object in **R2** and its typeinfo in **R3** (Q22), so it starts with both values in
registers, and it runs with its own frame still intact, which is what lets it destroy locals in
place. Entries exist only for functions with protected regions; the FP chain walks everything
else.

`ENTER` is therefore a pure frame instruction: it never touches EFP, and a function with no
cleanup and no `catch` needs no exception table entry at all — the FP chain is enough to walk
past it.

EFP is then free to be exactly what its name says: the *hardware* exception frame chain for
traps, interrupts and `BRK`. C++ exceptions never touch it. This split keeps the hardware
mechanism small and testable and keeps the C++ mechanism correct.

FP registers are caller-saved, so an exception crossing a frame may destroy F0–F3; that is
consistent with the ABI and with C++ (no FP value is required to survive a throw).

Single-phase unwinding (run cleanups while searching) is acceptable for v1 and much simpler
than two-phase; it changes only when `std::terminate` is called relative to cleanups. Recorded
as part of D13.

**The compiler's side of the contract.** KRCompiler has already implemented table-driven
exceptions for KR32 (ADR 0026/0027): a `.cpp_except` section of per-function descriptors
(`KREH` magic, function range, frame size, call-site regions → landing pad), a naked
`__cxa_throw`, an unwinder that walks the saved-R7 chain, sixteen-byte exception headers, and
cleanup landing pads. That is the model this section describes, but the concrete encoding is
32-bit-specific: 32-bit descriptor fields, an R7 frame walk, the return address at
`R7 + frame + 4`, the exception pointer and typeinfo in R0/R1, and a frame size in every
descriptor because that chain cannot be walked without it.

kr216 uses a 16-bit sibling of that format (Q22 resolved): 16-bit descriptor fields, an FP-chain
walk (`[FP]` is the caller's FP, `[FP+2]` the return address), SP restoration from the frame size
and saved-register mask, **R2 = exception object and R3 = typeinfo** in a landing pad, an
eight-byte exception header (four 2-byte words) instead of sixteen, and descriptors only for
functions with protected regions. That is a new ADR in KRCompiler plus `docs/abi-kr216.md`.
Until it exists, `has_eh` is false for the kr216 profile and the compiler refuses
`try`/`catch`/`throw` on the target, exactly as it does for KR16 today.

One inherited assumption does not survive: ADR 0026 emits a descriptor for *every* function
because the KR32 unwinder needs its frame size. With an FP chain, kr216 can walk without one,
so the 16-bit format omits descriptors for functions with no protected region; the unwinder
treats a missing entry as "no landing pad, keep walking".

If `krcompiler` nonetheless needs a frame-chain model (Q18), `ENTER` would grow a handler
operand and link an EFP frame, and the tables would disappear. The ISA keeps `F9`–`FF` reserved
so that form could be added without disturbing anything.

### 11.5 Runtime and startup

`crt0` (linked first, at `0x0200`) does: set SP, zero `.bss`, walk `.init_array`, call
`main(argc, argv)`, then `exit(R2)`. `exit` runs `__cxa_atexit` handlers and calls `SYS exit`.

The heap is a boundary pointer starting at `_end`, growing up, with a first-fit free list on
top. The allocator has to be small: 1–2 KiB of code, not dlmalloc.

There is no kernel, no processes, no filesystem in v1; `SYS` provides console and a handful
of host services so that `printf`, `malloc` and `exit` work.

### 11.6 Language and library subset

| Supported from the start | Deferred / unsupported |
|---|---|
| Classes, access control, constructors/destructors | Multiple inheritance, virtual bases |
| References, overloading, default arguments | `thread_local`, threads, atomics |
| Single inheritance, virtual functions | Full RTTI graph, `dynamic_cast` across libraries |
| Templates (front-end only, no ISA impact) | `<iostream>`, locales, `<filesystem>` |
| `constexpr`, `enum class`, lambdas (closures need a trampoline or a static thunk) | Coroutines (compiler support, no ISA blocker) |
| `new`/`delete`, exceptions, RTTI, templates, lambdas | `std::string`/`vector`/`algorithm` (compiler v1.2), `map`/`iostream` (v1.3) |
| `float`/`double` arithmetic and conversions; `long double` as an alias for `double` | Extended-precision `long double`, decimal floating point |

The target is a **freestanding C++17 implementation with a curated `libc++` subset**: the
containers and algorithms above, no `<iostream>` at v1.0, no locales, no filesystem, no
threads. This is a deliberate scope decision (D25), not a temporary limitation: a hosted
implementation does not fit in 64 KiB.

Delivery follows KRCompiler's library ladder, which is the real schedule: v1.0 freestanding
headers (`type_traits`, `limits`, `new`, `exception`, `initializer_list`, `utility`), v1.1
`memory`/`array`, v1.2 `string`/`vector`/`algorithm`, v1.3 `iostream`/`map`. D25 is the
target's scope; the compiler's tiers decide when each piece exists.

### 11.7 What C++ asks of the ISA

| C++ feature | ISA requirement | Where it is met |
|---|---|---|
| Pointers, references, arrays | 16-bit addresses, load/store, `LEA` | Register/Normal/Compact classes |
| `char` strings, `memcpy` | Byte load/store, signed and unsigned, post-increment | `LDBU`/`LDBS`/`STB+`, 2 bytes |
| Structs, member access | Base + displacement addressing | Normal class `[Rb+d8]` |
| `switch` | Indirect jump, absolute/PC-relative tables | `JMP Rd`, `JMP addr16` |
| Function pointers, virtual calls | Indirect call, vtable load, `this` in a fixed register | `CALL Rd`, R10 |
| `long`, `long long` | Carry arithmetic, `MULH`, multiword shift sequences | `ADDC`/`SUBB`, shift sequences |
| `float`, `double` | binary64 register file, load/store, conversion, `FRND` rounding | F3–F6 family |
| Exceptions | Frame chain, PC→landing-pad table, heap | FP chain + tables |
| RTTI, `dynamic_cast` | Data sections only | No ISA impact |
| Templates, `constexpr` | Compile-time only | No ISA impact |
| `bool` results | `SETcc` | Normal class |
| Stack unwinding/backtrace | FP chain | ABI |
| Register saves | Mask push/pop | Normal class |
| Debugging | Breakpoint trap, single-step flag | `BRK`, T flag |

### 11.8 Debugger and VM interface

The debugger is `..\krdbg`, and it drives any VM through one adapter implementing
`krdbg::IDebugTarget` (`include/krdbg/target.h`, documented in `..\krdbg\PLAN.md` §4.4). The
adapter is the only kr216 code in that tree; `krdbg_core` and `krdbg_ui` must never learn about
kr216. The VM-facing surface is small, but it constrains the machine in several places:

| krdbg primitive | What kr216 must provide | Architectural consequence |
|---|---|---|
| `step_instruction()`, `run(budget)`, `halted()` | one instruction per step, a budgeted run, and distinguishable `Halted`/`Fault` states | `HALT`, traps and end-of-image are distinct stop reasons; multiregister `PUSH`/`POP` and `ENTER`/`LEAVE` are single instructions, so stepping is unambiguous |
| `pc()` / `set_pc()` | a 16-bit PC through a 64-bit interface | the adapter truncates; no machine change |
| `layout()` | groups, flags, memory regions; `address_bits = 16`, `pointer_bits = 16`, `little_endian = true` | one GPR group, one flags group, one FP group |
| `read_value` / `write_value`, `read_flags` | 16-bit GPRs, 64-bit FP values, packed flag bits | R0/R1 are read-only in the layout; EFP/FP/PC/SP carry their roles |
| `read_memory` / `write_memory` | byte-addressed and all-or-nothing | the null page and the reserved window fail a whole range, not part of it |
| `disassemble()` | length in bytes, raw bytes, mnemonic and operands | the emulator's disassembler is the single source |
| `unwind()` | `RawFrame{pc, frame_base}`, innermost first | the FP chain; KDBG supplies function names |
| `instructions_executed()` | a monotonic retired-instruction count since load | a counter separate from the free-running cycle counter |
| `reset()` | reload the last image and clear state | the loader retains the image bytes |

**The frame convention.** krdbg's KDBG model says `function.frame` is the offset from the
frame-pointer register to the saved caller frame, and the return PC is at `+frame` plus the
pointer size. kr216's `ENTER` puts the saved FP at `[FP]` and the return address at `[FP+2]`, so
every function's record says `frame = 0`, the return PC is at `FP+2`, and locals are negative
offsets from `FP`. That only works if **FP is maintained in every function**: no
`-fomit-frame-pointer` profile. This is a hard ABI rule, not an optimisation preference — the
unwinder and the debugger both read `[FP]`.

**The variable-length problem.** `TargetLayout::instruction_alignment` is a single number and
the disassembly surface advances rows by it; `InstructionView::length` is used only to size the
byte column. That contract is correct for KR32 and KR16 and wrong for kr216, whose instructions
are 1–6 bytes. The adapter cannot fix it: the change belongs in `krdbg` (`TargetLayout` plus the
surface's row advance, so it steps by the returned length). Q23 resolves it that way, and the
change is a prerequisite for the M4 debug milestone.

**Debug information and loading.** The kr216 image carries a KDBG payload (target ID 4), in the
image by default, matching KR32; `krld --debug-file` may still write an optional sidecar, which
is worth it only for a very small image. The vendored reader recognises only the KR32 `.kr32`
and KR16 `.kr16` containers; for the kr216 container the adapter extracts the payload and calls
`read_payload`, checking `image_base` itself. The image writer must therefore define the base
the payload was built for. Container support belongs upstream in KRCompiler and is re-vendored,
not patched in krdbg.

**What is not required.** krdbg sets breakpoints by comparing `pc()` against an address set and
single-steps by calling `step_instruction`; it does not use a breakpoint instruction or a trap
flag, and it does not patch code. `BRK` and the T flag stay as guest and debug aids, not as
debugger dependencies. There is no framebuffer, so `video()` returns null; the console maps to
`IComPort` directly (section 7.3). Source-level loading (build-on-load) is KR32/KR16-only in
krdbg today, so kr216 loads images.

## 12. Toolchain plan

### 12.1 Components

| Component | Purpose | Where | Milestone |
|---|---|---|---|
| `kr216-isa.md` | Normative encoding and semantics | this repo | M0 |
| `docs/abi-kr216.md` | The target ABI appendix the compiler freezes | krcompiler | M0 |
| kr216 profile + descriptor | Data model, capabilities, register roles, calling convention | krcompiler (`src/target/`, `src/backend/`) | M3 |
| kr216 assembler + encoder + KRO writer | Syntax, encoding tables, relocations, object emission | krcompiler (`src/asm/`) | M3 |
| kr216 backend | Selection, frames, assembly text, debug information | krcompiler (`src/backend/`) | M3–M4 |
| `kr216-emu` | Interpreter, FPU, MMIO devices, machine-level debug interface, the kr216 VM | this repo | M1 |
| `kr216-dis` | Disassembler (shared tables with the emulator) | this repo | M1 |
| `targets/kr216/runtime/` | `crt0`, `libc`, exception runtime, host-call stubs | krcompiler | M4 |
| `sysroot/kr216/include/` | Generated `bits/*` headers | krcompiler | M3 |
| `krdbg` | Source-level debugger over the machine interface, built on `libkrcdebug` | krcompiler | M4 |
| `krdbg` kr216 adapter | The `IDebugTarget` implementation over the emulator; the only kr216 code in the krdbg tree | krdbg repo (`adapters/kr216/`) | M4 |
| `libc++` subset | `string`, `vector`, `map`, `algorithm`, `memory` | krcompiler ladder | M5 |

There is no separate assembler or linker for kr216: the compiler's assembler/encoder writes KRO
objects, `krld` links them, and a kr216 image writer emits the loadable image. The emulator needs
a loader for that image and the machine-level debug interface that `krdbg` drives.

Object format: **KRO**, KRCompiler's target-tagged container (kr216 takes the next target ID, 4;
KR32 is 1, KR16 is 2, KR64 is 3), little-endian, with 64-bit canonical header and relocation
fields and 16-bit target addresses. Sections: `.text`, `.rodata`, `.data`, `.bss`,
`.init_array`, `.fini_array`, and `.cpp_except` for the exception descriptors; debug information
travels in KDBG (also target ID 4). Relocations are the common absolute and PC-relative
families, plus kr216 branch relaxation between `CALL rel9` and `CALL addr16`. The image the
emulator loads is a KRCompiler image container (D33) — magic, target/ABI version, sections,
entry, image base and the KDBG payload — not ELF. Like KR32's `.kr32`, it carries the sections
and a KDBG payload, so the debugger gets debug information from the same file it runs (D31).
`krld --debug-file` can still split the payload into a sidecar for a very small image.

Because the vendored KDBG reader recognises only the KR32 `.kr32` and KR16 `.kr16` containers, the
kr216 debug adapter extracts the payload from the kr216 container, calls `read_payload`, and
checks `image_base` itself; container support belongs upstream in KRCompiler, not in a patched
reader (section 11.8).

### 12.2 kr216 as a KRCompiler target

`..\krcompiler\plan.md` is approved (2026-11-09) and implementation is well under way: the
toolchain is `krc` (front end + IR + per-target backend) and `krld` (linker), with KR32 and
KR16 backends, KRO objects, KDBG debug information, a shared `libkrcdebug` reader, and a
per-target runtime/sysroot. `docs/targets/schema.md` with `plan.md` §4.2, §9 and §10 is the
contract for adding a backend. A target is five layers and only one of them is the code
generator:

| Layer | What it decides | kr216 work |
|---|---|---|
| Profile | widths, data model, capability flags | new `TargetProfile` row |
| Descriptor | register roles, calling convention, alignment | new `Kr216Descriptor` |
| Assembler + object writer | syntax, encoding, relocations, KRO | new assembler/encoder module |
| Backend | selection, frames, assembly text, debug info | new `Kr216Backend` |
| Runtime + sysroot + VM | `crt0`, `libc`, EH, host stubs; `bits/*`; the emulator | new `targets/kr216/`, `sysroot/kr216/`, this repo's emulator |

The governing rule in that project — fixed by ADR 0002 (descriptor schema), ADR 0005 (KRO),
ADR 0009 (KDBG) and `plan.md` §4.3 — is that profile, descriptor, ABI, object encoding and
KDBG target ID are frozen contracts: the first commit for a new target is an ADR, a `plan.md`
decision-log entry and `docs/abi-kr216.md`, plus a failing test — not a backend. That is the
sequence kr216 has to follow.

The compiler-side work — the data-model ADR, the `this_register` schema change, the 16-bit EH
descriptor, `docs/abi-kr216.md` and the backend — lives in the krcompiler repository (D32).
This repository owns the ISA, the emulator and the integration tests that prove the two halves
agree.

**Descriptor values the target will declare.**

| Field | Value |
|---|---|
| pointer width / endianness | 16 / little |
| data model | IP16 with `long` 32 and `long long` 64 (a new profile value, not `Ip16`) |
| `char` | 8, signed |
| `short` / `int` / `long` / `long long` | 2 / 2 / 4 / 8; align 2 |
| `float` / `double` / `long double` | 4 / 8 / 8 (alias) |
| `size_t` / `ptrdiff_t` / `intmax_t` / `wchar_t` | 2 / 2 / 8 / 2 |
| integer arguments / return | R2–R5 / R2 (R2:R3 for 32-bit) |
| FP arguments / return | F0–F1 / F0 |
| frame pointer / thread pointer | R13 / none |
| callee-saved | R8, R9, FP, SP |
| stack alignment / public-call alignment | 2 / 2 |
| aggregate rules | `MemoryOnly`; an aggregate argument is ADR 0046's hidden pointer, and an aggregate return destination travels in R6 |
| capabilities | `has_fpu` and `has_cpp`; no matrix, atomics, threads or TLS; `has_eh`/`has_rtti` only after the 16-bit EH format exists |
| `this` | R10, declared by the descriptor's new `this_register` field |

**Upstream changes that must land before the target is complete.**

1. **Data-model enum.** `data_model` is `Ilp32`/`Ip16`/`Lp64`; kr216's IP16-with-32-bit-`long`
   model is a fourth value. That is an ADR 0002 change (D29).
2. **The `this` register.** The descriptor gains a `this_register` field (Q21 resolved);
   `RegisterInfo` still needs the field and the backend must set R10 at member call sites.
3. **Exception descriptors.** ADR 0026's format is 32-bit-specific: 32-bit fields, an R7 frame
   walk, the return address at `R7 + frame + 4`, exception pointer and typeinfo in R0/R1, and a
   frame size in every descriptor. kr216 uses 16-bit fields, an FP-chain walk, R2/R3 for the
   landing pad, an eight-byte header, and descriptors only for functions with protected regions
   (Q22 resolved). That is a new ADR plus the kr216 ABI appendix, and it is what turns `has_eh`
   on.
4. **Narrow-target literal typing.** ADR 0034 records the defect: with a 16-bit `int`,
   `long x = 32768;` is rejected even though `long` is 32 bits. kr216 must pass that
   regression on its own profile before arithmetic fixtures are meaningful.
5. **Vtable word size.** ADR 0025's `_ZTV`/`_ZTI` layout is written in 4-byte words. Scaling it
   to 2-byte words is mechanical (vptr = `_ZTV+2`, slot *k* at `vptr+2k`, typeinfo at
   `vptr-2`), but `docs/abi-kr216.md` must say so explicitly.
6. **Target IDs.** KRO and KDBG both take the next ID (4), with a `kROTargetId` case, a
   `docs/symbols.md` row, and a linker that validates the tag.

**What the backend will reuse.** The concrete `Kr<Name>Backend` shape from `plan.md` §9.4 —
`emit()` producing assembly text, `debug_info()`, `frame_bytes()` — with the compiler's
assembler/encoder and KRO writer doing the encoding. The backend should build the shared
`machine::Instruction` records from the start; keeping the shared machine form is what makes
register assignments checkable. `-O0` only at bring-up; `-O1` is measured, not assumed (D-049).

**Scale.** The KR32 backend is 2,555 lines and the KR16 backend 1,306; kr216 sits between them
and closer to KR32, because it has a real FPU and 64-bit integers but no matrix, threads or
atomics. The assembler/encoder, runtime, sysroot and VM add several thousand lines more, and
the assembler and backend are where the time goes.

| The target needs | kr216 provides |
|---|---|
| Register classes and allocation constraints | 9 GPRs (R2–R10) plus 4 FP registers; R0/R1 constants; R10 `this` |
| Calling convention and frame layout | Section 11.2, `ENTER`/`LEAVE` |
| Integer sizes and arithmetic | 8/16/32/64-bit model with carry chains and `MULH` |
| Floating point | binary64 F0–F3, `F3`–`F6` family |
| Exception descriptors | 16-bit format and FP-chain unwinder (Q22) |
| Debug information | KDBG target ID 4; frame records must match `ENTER`/`LEAVE` |
| Instruction selection | Appendix A, `kr216-isa.md`; branch relaxation rel9 ↔ addr16 |

**Sequencing.** The compiler's own milestones gate the work: C++ core is M5, templates and
C++11/14/17 M6, KR16 M7, v1.0 M8, and exceptions/RTTI are v1.1 (M10). The library ladder is
v1.0 freestanding headers, v1.1 `memory`/`array`, v1.2 `string`/`vector`/`algorithm`, v1.3
`iostream`/`map`. A kr216 backend can begin once the backend framework is stable (after M2/M3);
C++ feature coverage follows M5/M6, and try/catch follows the 16-bit EH format. The ISA,
emulator and `kr216-isa.md` work in this repository is independent of those dates — it is the
thing the compiler target will consume.

The remaining risk is no longer "what is the contract" — it is documented and under
implementation — but scheduling: the kr216 target is the first 16-bit C++ target in a compiler
whose only 16-bit target is C-only, and it will expose every place where the IR, ABI, EH
runtime or debug format quietly assumed 32-bit words.

## 13. Verification strategy

1. **Encoding vectors.** Every opcode has a table of `asm → bytes` and `bytes → asm` cases,
   checked by both assembler and disassembler. Round-trip is enforced.
2. **Semantic tests.** Per-instruction tests that set up registers/memory, execute one
   instruction, and assert the full post-state including flags.
3. **Canonical-equivalence tests.** Every integer short encoding must produce exactly the same
   state as the equivalent generic (`F0`) encoding, for every mode and register.
4. **Decoder fuzzing.** Random byte streams must decode to a valid instruction sequence or
   trap cleanly; no crashes, no out-of-bounds fetches. Every reserved encoding traps.
5. **Addressing-mode matrix.** Every mode × every GPR × load/store × byte/word/double is
   executed; failures are collected per cell.
6. **FP conformance.** Bit-exact vectors for the four operations, `FSQRT`, conversions, NaN,
   infinities, signed zero, subnormals, and rounding ties. Results are compared as bit
   patterns, not as approximate values.
7. **Differential execution.** Programs built by `krcompiler` for kr216 run on the emulator;
   the same source built for the host must produce the same output and checksums. This is the
   real ABI test.
8. **ISA conformance suite.** Hand-written assembly programs covering carry chains, shifts,
   addressing modes, stack discipline, mask push/pop, traps and `IRET`.
9. **Frame tests.** `ENTER`/`LEAVE` round-trips for every frame size 0–255 and every legal
   mask, nested frames, a runtime `SP` adjustment in the middle of a frame, tail calls through
   `LEAVE; JMP`, and a mismatched mask pair (which the assembler should reject when it can).
10. **Unwinding tests.** A `throw` crossing one to five frames runs every destructor exactly
    once and in the right order, catches at the right frame, and leaves SP/FP correct in the
    handler.
11. **Density benchmark.** A fixed corpus (string copy, `printf`, a small container, a
    recursive function, an FP loop) compiled and measured in bytes; the ≤ 2.5 B/instruction
    target is tracked per commit.
12. **Debugger self-test.** Backtraces through a known call chain must match expected frames.

## 14. Decision log

| ID | Decision | Status |
|---|---|---|
| D1 | 16-bit, byte-addressable, little-endian, 64 KiB, wrapping addresses | Accepted |
| D2 | R0 = 0, R1 = 1, writes discarded | Accepted |
| D3 | R11 flags, R12 EFP, R13 FP, R14 PC, R15 SP | Accepted |
| D4 | Variable-length encoding; the first byte selects one of five classes (short, compact, normal, register, escape) through a range table | Accepted (via D26) |
| D5 | 1-byte short forms; mask forms for multi-register push/pop | Accepted |
| D6 | 2-byte register-register ALU and 2-byte conditional branches (−256…+255) | Accepted |
| D7 | Arithmetic sets flags implicitly; `LEA` does not | Accepted |
| D8 | Unaligned accesses are legal | Accepted |
| D9 | Full-descending stack; SP points at the last occupied word | Accepted |
| D10 | Integer args in R2–R5; R8–R9 callee-saved; variadic calls stack-only | Accepted |
| D11 | `int` = 16 bits, `long` = 32, `long long` = 64, pointers = 16 | Accepted |
| D12 | Itanium-style object model; single inheritance first | Accepted |
| D13 | Table-driven exceptions on the FP chain; EFP is for hardware traps | Accepted |
| D14 | C++ compiler: the existing retargetable `krcompiler` grows a kr216 backend; `krdbg` supplies the debugger. No C compiler stage, no LLVM backend | Accepted |
| D15 | Hardware IEEE binary64 FPU: 4 registers, correctly rounded, no arithmetic traps, no status register | Accepted |
| D16 | Default memory map as in section 7.2, overridable by linker script | Accepted |
| D17 | R10 is the C++ `this`; it is caller-saved; callee-saved shrinks to R8–R9 | Accepted |
| D18 | `PUSH`/`POP` take a 16-bit mask; 1-byte forms are single-bit masks; bits 14–15 illegal | Accepted |
| D19 | Addressing modes are uniform across all 16 registers and are the canonical ISA form; short classes are compressions | Accepted |
| D20 | Byte loads are explicitly signed (`LDBS`) or unsigned (`LDBU`) | Accepted |
| D21 | `float` is 4-byte storage with strict binary32 arithmetic (`FLT_EVAL_METHOD = 0`); the binary64 datapath rounds after each f32 operation via `FRND` | Accepted |
| D22 | FP arguments in F0/F1, FP return in F0, all FP registers caller-saved | Accepted |
| D23 | `ENTER #frame, #mask` and `LEAVE #mask` define the frame; `ENTER` carries no landing-pad operand — landing pads are table data | Accepted |
| D24 | Load-store ISA: memory operands appear only on `LD`/`ST`; ALU operations take registers and immediates | Accepted |
| D25 | Freestanding C++17 with a curated `libc++` subset (`string`, `vector`, `map`, `algorithm`, `memory`, `type_traits`); no `<iostream>`, locale, threads or filesystem | Accepted |
| D26 | Register class: `LD Rd,#imm16`, `LD Rd,[addr16]`, `ST [addr16],Rd` are 3 bytes with the register in the opcode byte; paid for by demoting `CLR` (alias of `MOV Rd,R0`) and `JMP Rd` (compact op 31); the 4-byte Long class is retired and its remaining ops move to the `F1` escape | Accepted |
| D27 | kr216 is a KRCompiler target: a profile row, descriptor, assembler/encoder + KRO writer, backend, `targets/kr216` runtime, `sysroot/kr216`, and this repository's emulator as the VM (target ID 4). No separate assembler or linker | Accepted |
| D28 | Vtable/typeinfo layout follows ADR 0025 scaled to 2-byte words: `_ZTV` = `[typeinfo, slot 0, ...]`, vptr = `_ZTV+2`, typeinfo at `vptr-2`, weak globals, RTTI always emitted for polymorphic classes | Accepted |
| D29 | Data-model profile: `int` 16, `long` 32, `long long` 64, pointer 16, plain `char` signed, align 2, aggregates memory-class; this is a new profile value (IP16 with 32-bit `long`) and needs an ADR 0002 change upstream | Accepted |
| D30 | Debug/VM contract: FP is maintained in every function (KDBG `frame = 0`, return PC at `FP+2`, locals negative); the image carries a KDBG payload with a defined `image_base`; the console maps to krdbg's `out`/`in`/`status` COM contract; the VM exposes a monotonic instruction counter | Accepted |
| D31 | KDBG payloads are embedded in the image by default, matching KR32's in-image pattern; `krld --debug-file` may write an optional sidecar, which is the preferred form only for very small images | Accepted |
| D32 | Compiler-side work — the data-model ADR, `this_register` schema change, 16-bit EH descriptor, `docs/abi-kr216.md` and the backend — lives in the krcompiler repository; this repository owns the ISA, emulator and integration tests | Accepted |
| D33 | The kr216 image is a KRCompiler container with magic, target/ABI version, sections, entry, image base and the embedded KDBG payload, like KR32's `.kr32`; the emulator loader parses it | Accepted |
| D34 | `F0`'s canonical layout: a one-byte operation code, then two 7-bit operand specifiers in two bytes with two reserved bits, then extension words for an immediate or a 16-bit displacement; 4–6 bytes, giving the generic form its own 256-operation space | Accepted |

## 15. Resolved questions

| ID | Question | Resolution |
|---|---|---|
| Q1 | Stay at 64 KiB, or add banking/segmentation for >64 KiB? | Stayed at 64 KiB (D1). Banking is deferred to an escape-class extension; pointers stay 16-bit. |
| Q2 | Is R1 = 1 worth a register? | Kept (D2). Revisit with M3 instruction-frequency data; register roles are ABI-only. |
| Q3 | Implicit flags, or an explicit flag-setting bit? | Implicit (D7). |
| Q4 | Table-driven exceptions vs an EFP `setjmp` chain? | Table-driven (D13, D23). |
| Q5 | How do we get to real C++? | `krcompiler` grows a kr216 backend; `krdbg` supplies the debugger. No C-first stage, no LLVM backend (D14). |
| Q6 | `long double`: alias to `double`, or unsupported? | Superseded by Q20: `long double` is an 8-byte alias for `double`. |
| Q7 | Bit instructions (`BTST`/`BSET`/`BCLR`)? | Deferred until MMIO or driver code needs them. |
| Q8 | Block move/fill instructions for `memcpy`/`memset`? | Deferred; post-increment loops are adequate. |
| Q9 | Interrupt model: vectors, priority, nesting? | 16 vectors and the I flag are defined; no interrupt source exists until there is an OS. Priority and nesting detail deferred with it. |
| Q10 | Hosted vs freestanding C++? | Freestanding with a curated `libc++` subset (D25). |
| Q11 | 32-bit extension? | Reserved in the escape class; not v1. |
| Q12 | Self-hosting the toolchain? | No. |
| Q13 | Is `this` in R10 worth losing a callee-saved register? | Kept (D17). Measure spill rates at M3 and revisit if member-heavy code suffers. |
| Q14 | Should ALU instructions take memory operands (PDP-11 style)? | Load-store only (D24); memory operands live on `LD`/`ST`. |
| Q15 | `float` evaluation model: widening, or `float` = `double`? | Superseded by Q24: strict binary32 (D21). |
| Q16 | Are mask bits 14–15 (PC, SP) really illegal? | Yes (D18); `RET` covers return and nothing needs to push SP. |
| Q17 | Four FP registers enough, or eight? | Four (D15); the 2-bit field is frozen with the ISA. |
| Q18 | If the table-driven model proves impractical, switch to a frame-chain EH? | Table-driven stands (D23); the frame chain remains a documented fallback only. |
| Q19 | What does `krcompiler` need from a new target — IR, ABI hooks, debug-info format, backend interface? | **Answered.** The contract is `plan.md` §4/§9/§10 plus `docs/targets/schema.md`, ADR 0002 (descriptor schema), ADR 0025 (vtables/RTTI), ADR 0026/0027 (exceptions) and `docs/abi.md`: a profile row, a descriptor, an assembler/encoder + KRO writer, a backend class, a runtime/sysroot, and a VM. Section 12.2 maps kr216 onto it; Q20–Q23 record the follow-ups, all now resolved. |
| Q20 | `long double`: keep it absent (`long_double_bits = 0`) or alias it to `double` like KR32? | **Alias to `double`** (8 bytes, 2-byte align). The C++ front end keeps the type, the FPU already produces binary64, and no new datapath is needed. Supersedes the earlier "unsupported" answer. |
| Q21 | The compiler's descriptor has no dedicated `this` register: extend the schema, or pass `this` as argument 0? | **Extend the descriptor with `this_register`** (ADR 0002 change). R10 stays `this` (D17) and member calls keep the first argument register; the backend sets it at call sites. |
| Q22 | What is the 16-bit exception descriptor and landing-pad convention? | **Resolved**: 16-bit descriptor fields, a frame-size + saved-mask entry, an FP-chain walk, **R2 = exception object / R3 = typeinfo** in a landing pad, an 8-byte exception header, and descriptors only for functions with protected regions. New KRCompiler ADR plus `docs/abi-kr216.md`; `has_eh` turns on for the profile when it lands. |
| Q23 | krdbg advances disassembly rows by a fixed `instruction_alignment`; kr216 is variable-length. Who changes? | **krdbg changes**: `TargetLayout` gains a variable-length mode and the surface advances rows by `InstructionView::length`. The adapter cannot compensate. Prerequisite for the M4 debug milestone. |
| Q24 | How should `float` arithmetic be evaluated on a binary64-only FPU? | **Strict binary32** (D21): the backend computes in binary64 and rounds after each f32 op with the new `FRND` instruction; `FLT_EVAL_METHOD = 0`, one extra instruction per f32 arithmetic op, no compiler front-end change. |
| Q25 | How do memory-class aggregates cross calls on kr216? | **ADR 0046 pointer + R6 destination**: an aggregate argument is an invisible pointer to a caller-owned copy (one argument word, callee copies); a memory-class return destination is loaded into R6 before CALL, so R2–R5 stay argument registers. |
| Q26 | What is the kr216 loadable image? | **A KRCompiler container** (D33): magic, target/ABI, sections, entry, image base and the embedded KDBG payload, like KR32's `.kr32`; the emulator loader parses it. |
| Q27 | What do we write next? | **`kr216-isa.md` and `docs/abi-kr216.md` in parallel.** They define the instruction semantics, trap/boot/SYS detail and the ABI, after which implementation planning has stable contracts. |
| Q28 | D25 promises `string`/`vector`/`map`, but KRCompiler ships them at v1.2/v1.3. | **Accept the compiler ladder**: `string`/`vector` at v1.2, `map` at v1.3; the roadmap wording says so and no extra work is scheduled. |
| Q29 | When do the literal-typing fix and krdbg's variable-length change land? | **Before their dependent milestones**: literal typing before M3 arithmetic tests, krdbg before M4 debug. Both are upstream blockers, not workarounds. |

## 16. Roadmap

| Milestone | Deliverable | Exit criteria |
|---|---|---|
| **M0** | This plan, the compiler contract mapped, `docs/abi-kr216.md` started | D1–D34 resolved or explicitly deferred; Q1–Q29 answered |
| **M1** | `kr216-isa.md`, emulator (including FPU), disassembler, KRO image loader, test harness | Hand-written programs run and pass; every encoding has a vector; FP vectors are bit-exact |
| **M2** | kr216 profile, descriptor, assembler/encoder, KRO writer, `krld` target ID 4, image writer, VM wiring | `krc --target=kr216` emits assembly and objects; `krld` links them; the image runs on the emulator; round-trip and determinism tests pass |
| **M3** | Backend: integer and pointer code, calls, `ENTER`/`LEAVE` frames; sysroot `bits`; `crt0`; C subset | C programs run on the emulator; `docs/abi-kr216.md` frozen |
| **M4** | C++ core: classes, single inheritance, virtual dispatch, `new`/`delete`, templates (as the compiler's M5/M6 land); FP; KDBG and `krdbg` (after the krdbg variable-length change) | A C++ program with classes, virtual calls and `double` runs and debugs |
| **M5** | Exceptions/RTTI (after the 16-bit EH ADR and the compiler's v1.1), cleanup pads; library tiers v1.1/v1.2 | `try`/`catch` runs destructors correctly; `std::string`/`std::vector` work as the compiler provides them |
| **M6** | Density and performance pass; benchmark corpus | ≤ 2.5 B/instruction, benchmark suite tracked |
| **M7** | Polish: docs, examples, release | An external user can build and run a C++ program from the README |

The compiler's own milestones gate M3–M5: its C++ core is M5, templates M6, v1.0 M8, and
exceptions/RTTI v1.1 (M10). The ISA and emulator work in M1–M2 is independent and is what the
compiler target consumes.

## 17. Risks

| Risk | Likelihood | Impact | Mitigation |
|---|---|---|---|
| KRCompiler's frozen contracts need changes: the data-model enum, a `this` register, the 16-bit EH format | High | High | ADRs for each before code; D29–D32 record the resolutions; keep the ISA stable so only compiler-side contracts move |
| Narrow-target literal typing rejects `long` constants (`long x = 32768;`) | High | Medium | Fix upstream before arithmetic fixtures; add the regression test |
| The first 16-bit C++ target exposes 32-bit assumptions in the IR, ABI, EH runtime or debug format | High | High | Vertical slices: C subset → classes → exceptions; differential tests; KDBG frame invariants |
| krdbg's fixed `instruction_alignment` misrenders variable-length disassembly | High | Medium | Upstream krdbg change before M4 (Q23); the adapter cannot compensate |
| KDBG payload/container mismatch: the vendored reader knows only `.kr32`/`.kr16` | Medium | Medium | Embed the payload, use `read_payload`, check `image_base`; container support belongs upstream in KRCompiler |
| Backend effort or schedule outside this repository | Medium | High | Budget roughly 2k backend lines plus assembler and runtime; the ISA and emulator remain independently useful |
| `krcompiler` needs an ISA feature the plan lacks (32-bit ops, different addressing) | Medium | Medium | The escape class can absorb additions under the ISA change policy (ADR + feature flag + golden vectors) |
| 64 KiB is too small for real C++ | Medium | Medium | Accept for v1; escape-class banking later; measure `_end` on the corpus |
| Host FP results differ across machines | Medium | High | binary64 via SSE2/NEON, not x87; `-ffp-contract=off`; bit-exact conformance vectors in CI |
| Exceptions are harder than expected | Medium | Medium | Ship `-fno-exceptions` C++ first; add table-driven EH when the 16-bit format exists |
| ABI churn after the first backend output | Medium | High | Freeze at M3 with `docs/abi-kr216.md`; changes require a version bump |
| Decoder bugs from variable length | Low | High | Fuzzing + round-trip + canonical-equivalence + reserved-encoding traps |
| Motivation/scope | Medium | High | Milestones are independently useful: an emulator and ISA are a finished retro machine even if C++ never lands |

## 18. Glossary

| Term | Meaning |
|---|---|
| binary64 | IEEE-754 double precision (1 sign, 11 exponent, 52 fraction bits) |
| Compact / Normal / Register | The 2- and 3-byte instruction classes |
| EFP | Exception Frame Pointer, R12; chains hardware trap frames |
| ENTER / LEAVE | Frame setup and teardown instructions; see section 8.8 |
| FLT_EVAL_METHOD | C/C++ macro describing the precision used to evaluate floating expressions; 1 = evaluate in a wider type |
| FP chain | Linked list of saved frame pointers used for unwinding and backtraces |
| `krc` / `krld` | KRCompiler's compiler driver and target-neutral linker |
| KDBG | KRCompiler's symbol/debug container; the shared reader is `libkrcdebug` |
| KRO | KRCompiler's target-tagged object container (`KRO\0`), target ID 4 for kr216 |
| `krdbg` | Source-level debugger generated by the krcompiler toolchain |
| `krcompiler` | The existing retargetable C++ compiler in `..\krcompiler`; kr216 is one of its targets |
| Mask push | `PUSH #mask`: one word stored for every set bit of a 16-bit register mask |
| MMIO | Memory-mapped I/O |
| OS | Operand specifier: 7-bit `mode:reg` pair used by the generic form |
| `sret` | Structure-return: the destination pointer through which a memory-class aggregate is returned; kr216 loads it into R6 before the call |
| `_end` | Linker symbol marking the end of the program image; start of the heap |

---

## Appendix A — Encoding summary card

```
Short     0 OOO RRRR                     1 byte    (rows 0-4 and 7)
Register  0101 RRRR | imm16              3 bytes   (LD Rd,#imm16)
          0110 RRRR | addr16             3 bytes   (LD Rd,[addr16])
          1110 RRRR | addr16             3 bytes   (ST [addr16],Rd)
Compact   10 0 OOOOO Ra Rb               2 bytes   (ALU, LD/ST, JMP)
          101 CCCC ddddddddd             2 bytes   (Bcc, BRA, CALL rel9)
Normal    110 OOOOO pppppppp pppppppp    3 bytes   (payload per opcode)
Escape    1111 SSSS ...                  2-6 bytes (prefix-declared)
  F0      op8 | spec1 | spec2 | extensions            4-6 bytes
  F1      long integer: OOOO RRRR | addr16              4 bytes
  F3      FP reg-reg:  OOOO Fd Fs        2 bytes
  F4      FP load/store: OOOO Fd MM | BBBB 0000 | d8   (d8 only for mode 01)
  F5      FP convert:  OOOO Fd 00 | RRRR 0000          3 bytes
  F6      FP push/pop: OOOOOO Fd                        2 bytes
  F7      ENTER: frame8 | mask16                        4 bytes
  F8      ENTER: frame16 | mask16                       5 bytes
Normal op 28  LEAVE: mask16                             3 bytes
```

Worked encodings:

| Assembly | Bytes | Length |
|---|---|---|
| `push fp` | `0D` | 1 |
| `pop fp` | `1D` | 1 |
| `ret` | `70` | 1 |
| `inc r2` | `22` | 1 |
| `call r3` | `43` | 1 |
| `mov r3, r0` | `80 30` | 2 |
| `jmp r3` | `9F 30` | 2 |
| `mov fp, sp` | `80 DF` | 2 |
| `add r3, r4` | `81 34` | 2 |
| `cmp r4, r0` | `88 40` | 2 |
| `ldbu r4, [r2]` | `90 42` | 2 |
| `ldbs r4, [r2]` | `91 42` | 2 |
| `beq +6` | `A0 06` | 2 |
| `bra -9` | `BF F7` | 2 |
| `fadd f0, f1` | `F3 01` | 2 |
| `ld r4, [r5+3]` | `C0 45 03` | 3 |
| `call 0x0200` | `D0 00 02` | 3 |
| `push #0x0300` | `D7 00 03` | 3 |
| `pop #0x0300` | `D8 00 03` | 3 |
| `ld r2, #0x1234` | `52 34 12` | 3 |
| `ld r6, [0x4000]` | `66 00 40` | 3 |
| `st [0x4000], r6` | `E6 00 40` | 3 |
| `enter #8, #0x0300` | `F7 08 00 03` | 4 |
| `leave #0x0300` | `DC 00 03` | 3 |
| `fld f0, [r2+8]` | `F4 01 20 08` | 4 |

A complete function, `int sum(const char *s)` (argument in R2, result in R2):

```
sum:
    push fp          0D
    mov  fp, sp      80 DF
    mov  r3, r0      80 30       ; n = 0
loop:
    ldbu r4, [r2]    90 42       ; c = *s
    cmp  r4, r0      88 40       ; c == 0 ?
    beq  done        A0 05
    add  r3, r4      81 34       ; n += c
    inc  r2          22          ; ++s
    bra  loop        BF F5
done:
    mov  r2, r3      80 23
    pop  fp          1D
    ret              70
```

20 bytes of code. Branch displacements are relative to the next instruction: `beq` at offset
9 jumps +5 to `done` at offset 16, and `bra` at offset 14 jumps −11 back to `loop` at offset 5.
The same shape of function on a machine with only fixed 16-bit instructions and no 1-byte
push/pop/inc/dec would be roughly 2.5× larger.

A function that needs a frame and two saved registers uses the frame instructions instead:

```
f:
    enter #8, #0x0300    F7 08 00 03   ; save fp, r8, r9; 8 bytes of locals
    ...
    leave #0x0300        DC 00 03      ; restore r9, r8, fp; sp -> return address
    ret                  70
```

Eight bytes of prologue and epilogue instead of seventeen, and one instruction at each end.

## Appendix B — Alternatives considered

| Alternative | Why rejected |
|---|---|
| Fixed 16-bit instructions (RISC style) | Loses the density goal outright; prologue/epilogue alone doubles |
| Accumulator or stack machine | Excellent density for small programs, terrible for C++ expression code and register allocation |
| 32-bit registers with a 16-bit bus | Doubles the size of pointers and of every spill; contradicts "16-bit" |
| Segmented 20-bit addressing (8086 style) | Forces near/far pointers into the C++ type system; the worst possible outcome for a C++ target |
| Soft-float only | Slower and larger code for the FP that C++ numeric code needs; the emulator makes a real FPU nearly free |
| x87-style 8 FP registers | More state and wider fields for little measured benefit at this scale (Q17) |
| `this` as hidden argument 0 | Consumes an argument register on every member call; R10 frees it (Q13) |
| Landing-pad pointer in `ENTER` | One pointer cannot describe several try regions, cleanup state, or frames with no handler; costs a word per frame and diverges from the Itanium model (section 11.4, Q18) |
| Bytecode VM instead of a CPU | Would be simpler, but the point of the project is an ISA |
| Register windows (SPARC style) | Too much state for 16 registers; no benefit at this scale |
| Delayed branches | Only helps pipelined hardware; costs density in the emulator |
