# kr216 — Instruction Set Architecture

| | |
|---|---|
| Status | Draft 0.2 — normative for encodings, operand rules and trap/SYS detail; the §15 decision register is approved |
| Date | 2026-11-22T00:00:33-08:00 |
| Approval | §15's sixteen decisions approved by the owner, 2026-11-21; changing one is an ISA change under the plan's change policy |
| Authority | `kr216-plan.md` v0.11 (D1–D34) fixes the shape; this document fixes the exact encodings and semantics the plan delegates to it |
| Applies to | `kr216-emu`, `kr216-dis`, the krcompiler assembler/encoder, and the kr216 image loader |
| Open items | section 15 lists every decision this document records that the plan leaves to it, and section 16 what is deferred |

The key words MUST, MUST NOT, SHOULD and MAY are used as in RFC 2119.

## 1. Notation

- Addresses, registers and memory are 16 bits wide. A **word** is two bytes.
- Multi-byte fields are little-endian: the low byte is at the lower address.
- Bit 15 is the most significant bit of a 16-bit value; bit 0 the least.
- `d8`, `d16`, `imm8`, `imm16`, `disp9` are two's-complement values unless named `u8`.
- `Ra`, `Rb`, `Rd`, `Rs` are general-register numbers 0–15; `Fd`, `Fs` are 0–3.
- `mask16` is a 16-bit register mask; bit *n* selects R*n*.
- `^`, `&`, `|` are XOR, AND, OR; `<<` and `>>` are shifts; `sext8` sign-extends a byte.
- `PC` is the address of the instruction being executed unless stated otherwise.

## 2. Machine model

### 2.1 Registers

| Register | Role |
|---|---|
| R0 | constant 0; reads as 0, writes are discarded |
| R1 | constant 1; reads as 1, writes are discarded |
| R2–R5 | general; ABI argument/return registers |
| R6, R7 | general; scratch under the ABI |
| R8, R9 | general; callee-saved under the ABI |
| R10 | general; C++ `this` under the ABI |
| R11 | flags |
| R12 | exception frame pointer (`EFP`) |
| R13 | frame pointer (`FP`) |
| R14 | program counter (`PC`) |
| R15 | stack pointer (`SP`), full-descending, even under the ABI |

Writing R0 or R1 discards the value; reading either yields the constant. R11–R15 are
readable and writable as ordinary 16-bit registers, except where an instruction says
otherwise.

Writing R14 (PC) with any instruction other than a jump, a call, a return, a trap or `IRET`
is discarded: the instruction stream continues with the next instruction. That keeps PC
usable as a source for PC-relative materialisation — `MOV R2, PC` — without turning
arithmetic into control flow. R12, R13 and R15 take ordinary writes.

F0–F3 are 64-bit binary64 registers. They are not addressable by any address mode; only
the `F`-prefixed instructions read and write them.

### 2.2 Flags (R11)

| Bit | Name | Meaning |
|---|---|---|
| 0 | Z | result was zero |
| 1 | N | result bit 15 |
| 2 | C | carry out, or no-borrow |
| 3 | V | signed overflow; `FCMP` sets it when the operands are unordered |
| 4 | I | interrupts enabled |
| 5 | T | single-step trace |
| 6–15 | — | reserved, read as 0, writes ignored |

A condition is tested as on ARM:

| # | Cond | Test | # | Cond | Test |
|---|---|---|---|---|---|
| 0 | EQ | Z=1 | 8 | HI | C=1 and Z=0 |
| 1 | NE | Z=0 | 9 | LS | C=0 or Z=1 |
| 2 | CS | C=1 | 10 | GE | N=V |
| 3 | CC | C=0 | 11 | LT | N≠V |
| 4 | MI | N=1 | 12 | GT | Z=0 and N=V |
| 5 | PL | N=0 | 13 | LE | Z=1 or N≠V |
| 6 | VS | V=1 | 14 | — | `CALL rel9` |
| 7 | VC | V=0 | 15 | — | `BRA rel9` |

T is the single-step flag: while it is set, the machine raises a breakpoint trap (vector 1)
after each instruction completes, with the saved PC pointing at the instruction that would
run next (section 11.3). The v1 debugger steps without it (`kr216-plan.md` §11.8); the flag
exists for guest code and for a future trap-flag debugger.

### 2.3 Memory

64 KiB, byte-addressable, little-endian. All address arithmetic wraps modulo 65536.
8-bit and 16-bit accesses are architectural; unaligned accesses execute correctly.
Instructions may cross any byte boundary.

The default map is the platform profile in `kr216-plan.md` §7.2. The machine enforces
two windows: any access in `0x0000`–`0x00FF` and any access in `0xFF80`–`0xFFFF` raises an
address-error trap (vector 4). The I/O window `0xFF00`–`0xFF7F` is defined by section 14.

### 2.4 Reset

```
PC   = word at 0x0100
SP   = 0xFEF0
FP   = 0
EFP  = 0
R11  = 0
R2-R10 and F0-F3: undefined
```

R0 and R1 read as their constants and need no initialisation.

## 3. Encoding framework

### 3.1 First-byte length map

The first byte selects the length class. The decoder resolves the length from the first
byte before fetching the rest, except inside the escape class, where the prefix byte and,
for `F0` and `F4`, the bytes that follow declare the length.

| First byte | Class | Length | Section |
|---|---|---|---|
| `0x00`–`0x4F` | Short | 1 | 4.1 |
| `0x50`–`0x5F` | Register: `LD Rd,#imm16` | 3 | 5 |
| `0x60`–`0x6F` | Register: `LD Rd,[addr16]` | 3 | 5 |
| `0x70`–`0x7F` | Short (system) | 1 | 4.2 |
| `0x80`–`0x9F` | Compact, register form | 2 | 6.1 |
| `0xA0`–`0xBF` | Compact, branch form | 2 | 6.2 |
| `0xC0`–`0xDF` | Normal | 3 | 7 |
| `0xE0`–`0xEF` | Register: `ST [addr16],Rd` | 3 | 5 |
| `0xF0` | Escape: generic two-operand | 4–6 | 8.1 |
| `0xF1` | Escape: long integer | 4 | 8.2 |
| `0xF2` | Escape: reserved | — | 8.9 |
| `0xF3` | Escape: FP register | 2 | 8.3 |
| `0xF4` | Escape: FP load/store | 3–4 | 8.4 |
| `0xF5` | Escape: FP conversion | 3 | 8.5 |
| `0xF6` | Escape: FP push/pop | 2 | 8.6 |
| `0xF7` | Escape: `ENTER`, 8-bit frame | 4 | 8.7 |
| `0xF8` | Escape: `ENTER`, 16-bit frame | 5 | 8.7 |
| `0xF9`–`0xFF` | Escape: reserved | — | 8.9 |

### 3.2 Reserved fields and encodings

A field labelled **reserved** MUST be zero. A reserved first byte, a reserved opcode, or a
non-zero reserved field is an **illegal instruction** and raises trap vector 2. This makes
every unallocated encoding a trap, which is both the forward-compatibility rule and the
decoder fuzzing target (`kr216-plan.md` §8.10).

## 4. Short class

### 4.1 Register rows (`0 OOO RRRR`)

| Opcode | Instruction | Encoding |
|---|---|---|
| `0x00`–`0x0F` | `PUSH Rd` | short form of `PUSH #(1 << Rd)` |
| `0x10`–`0x1F` | `POP Rd` | short form of `POP #(1 << Rd)` |
| `0x20`–`0x2F` | `INC Rd` | `ADD Rd, #1` |
| `0x30`–`0x3F` | `DEC Rd` | `SUB Rd, #1` |
| `0x40`–`0x4F` | `CALL Rd` | indirect call; return address pushed |
| `0x50`–`0x6F` | — | Register class (section 5) |

`PUSH R14`, `PUSH R15`, `POP R14` and `POP R15` are illegal instructions: the short form is
a single-bit mask, and mask bits 14 and 15 are reserved. `PUSH R0` and `PUSH R1` push the
constants 0 and 1.

### 4.2 System encodings (`0 111 SSSS`)

| Byte | Instruction | Effect |
|---|---|---|
| `0x70` | `RET` | pop the return address into PC |
| `0x71` | `NOP` | nothing |
| `0x72` | `BRK` | breakpoint trap, vector 1 |
| `0x73` | `SYS` | host hypercall, section 13 |
| `0x74` | `EI` | set I |
| `0x75` | `DI` | clear I |
| `0x76` | `HALT` | stop the machine |
| `0x77` | `IRET` | return from a trap, section 11.4 |
| `0x78` | `RDCYCLE` | R2 = low word of the cycle counter |
| `0x79`–`0x7F` | reserved | trap |

## 5. Register class

Three bytes: the opcode byte followed by a 16-bit payload.

| First byte | Instruction | Encoding |
|---|---|---|
| `0101 RRRR` (`0x50`–`0x5F`) | `LD Rd, #imm16` | `imm16` (LE) |
| `0110 RRRR` (`0x60`–`0x6F`) | `LD Rd, [addr16]` | `addr16` (LE) |
| `1110 RRRR` (`0xE0`–`0xEF`) | `ST [addr16], Rd` | `addr16` (LE) |

`LD Rd, #imm16` loads the 16-bit value into Rd; for `LD Rd, [addr16]` the second and third
bytes are the address; for `ST [addr16], Rd` they are the address and Rd is the data. All
sixteen registers are legal in every row.

`LD Rd, #s8` and `LD Rd, #u8` in the Normal class are aliases of this form; the assembler
emits the Register form because all three are three bytes.

## 6. Compact class

### 6.1 Register form (`100 OOOOO Ra Rb`)

First byte `0x80` + op; second byte `Ra << 4 | Rb`.

| Op | Instruction | Op | Instruction | Op | Instruction | Op | Instruction |
|---|---|---|---|---|---|---|---|
| 0 | `MOV Ra, Rb` | 8 | `CMP Ra, Rb` | 16 | `LDBU Ra, [Rb]` | 24 | `LD Ra, -[Rb]` |
| 1 | `ADD Ra, Rb` | 9 | `MUL Ra, Rb` | 17 | `LDBS Ra, [Rb]` | 25 | `ST -[Rb], Ra` |
| 2 | `ADDC Ra, Rb` | 10 | `MULH Ra, Rb` | 18 | `STB [Rb], Ra` | 26 | `LDBU Ra, -[Rb]` |
| 3 | `SUB Ra, Rb` | 11 | `SHL Ra, Rb` | 19 | `LD Ra, [Rb]+` | 27 | `LDBS Ra, -[Rb]` |
| 4 | `SUBB Ra, Rb` | 12 | `SHR Ra, Rb` | 20 | `ST [Rb]+, Ra` | 28 | `STB -[Rb], Ra` |
| 5 | `AND Ra, Rb` | 13 | `SAR Ra, Rb` | 21 | `LDBU Ra, [Rb]+` | 29 | `NEG Ra` |
| 6 | `OR Ra, Rb` | 14 | `LD Ra, [Rb]` | 22 | `LDBS Ra, [Rb]+` | 30 | `NOT Ra` |
| 7 | `XOR Ra, Rb` | 15 | `ST [Rb], Ra` | 23 | `STB [Rb]+, Ra` | 31 | `JMP Ra` |

**Register roles.** `Ra` is the data register: the destination for a load or an ALU result,
the stored value for a store, the operand for a unary operation, and the target for `JMP`.
`Rb` is the second register: the source for `MOV` and the ALU operations, the base for the
load/store forms. Unary operations (`NEG`, `NOT`) and `JMP` require `Rb = R0`; any other
value is an illegal instruction.

**Auto-increment and auto-decrement.** `[Rb]+` loads from Rb and then adds the operand size
to Rb; `[Rb]+, Ra` in a store stores at Rb and then adds the operand size. `-[Rb]`
subtracts the operand size from Rb and then accesses. The step is 1 for byte forms and 2
for word forms. When the data register and the base are the same register, the base is read
before the data register is written, so `LD R2, [R2]+` loads from the original R2 and
leaves R2 + 2.

### 6.2 Branch form (`101 CCCC ddddddddd`)

First byte `0xA0 | (cond << 1) | ((disp9 >> 8) & 1)`; second byte `disp9 & 0xFF`. `disp9`
is signed and is relative to the address of the **next** instruction, range −256…+255.

| Cond | Instruction |
|---|---|
| 0–13 | `Bcc` with that condition |
| 14 | `CALL rel9`: push PC+2, then branch |
| 15 | `BRA rel9`: unconditional branch |

## 7. Normal class (`110 OOOOO`)

Three bytes: opcode, then two payload bytes whose layout is per opcode. `RRRR` is the data
register, `BBBB` the base register, `CCCC` a condition.

| Op | Instruction | Payload | Flags |
|---|---|---|---|
| 0 | `LD Rd, [Rb+d8]` | `RRRR BBBB`, `d8` | none |
| 1 | `ST [Rb+d8], Rs` | `RRRR BBBB`, `d8` | none |
| 2 | `LDBU Rd, [Rb+d8]` | `RRRR BBBB`, `d8` | none |
| 3 | `LDBS Rd, [Rb+d8]` | `RRRR BBBB`, `d8` | none |
| 4 | `STB [Rb+d8], Rs` | `RRRR BBBB`, `d8` | none |
| 5 | `LD Rd, #s8` | `RRRR 0000`, signed `imm8` | none |
| 6 | `LD Rd, #u8` | `RRRR 0000`, unsigned `imm8` | none |
| 7 | `ADD Rd, #s8` | `RRRR 0000`, signed `imm8` | Z N C V |
| 8 | `SUB Rd, #s8` | `RRRR 0000`, signed `imm8` | Z N C V |
| 9 | `CMP Rd, #s8` | `RRRR 0000`, signed `imm8` | Z N C V |
| 10 | `CMP Rd, #u8` | `RRRR 0000`, unsigned `imm8` | Z N C V |
| 11 | `AND Rd, #u8` | `RRRR 0000`, `imm8` | Z N; C=0 V=0 |
| 12 | `OR Rd, #u8` | `RRRR 0000`, `imm8` | Z N; C=0 V=0 |
| 13 | `XOR Rd, #u8` | `RRRR 0000`, `imm8` | Z N; C=0 V=0 |
| 14 | `LEA Rd, [Rb+d8]` | `RRRR BBBB`, `d8` | none |
| 15 | `JMP addr16` | `addr16` (LE) | none |
| 16 | `CALL addr16` | `addr16` (LE) | none |
| 17 | `LDBU Rd, [PC+d8]` | `RRRR 0000`, `d8` | none |
| 18 | `LDBS Rd, [PC+d8]` | `RRRR 0000`, `d8` | none |
| 19 | `SETcc Rd` | `RRRR CCCC`, `0x00` | none |
| 20 | `SHL Rd, #n` | `RRRR 0000`, `imm8` | Z N C; V=0 |
| 21 | `SHR Rd, #n` | `RRRR 0000`, `imm8` | Z N C; V=0 |
| 22 | `SAR Rd, #n` | `RRRR 0000`, `imm8` | Z N C; V=0 |
| 23 | `PUSH #mask16` | `mask16` (LE) | none |
| 24 | `POP #mask16` | `mask16` (LE) | none |
| 25 | `DIV Rd, Rb` | `RRRR BBBB` | Z N; C=0 V=0 |
| 26 | `DIVS Rd, Rb` | `RRRR BBBB` | Z N; C=0 V=0 |
| 27 | `SXTB Rd` | `RRRR 0000`, `0x00` | Z N; C=0 V=0 |
| 28 | `LEAVE #mask16` | `mask16` (LE) | none |
| 29–31 | reserved | — | trap |

- `d8` is signed; for the `[Rb+d8]` and `[PC+d8]` loads it is added to the base
  (or to the instruction address for PC) modulo 65536.
- `SETcc` accepts conditions 0–13. Conditions 14 and 15 select `CALL rel9` and `BRA rel9`
  in the Compact class and are illegal here.
- `SHL`/`SHR`/`SAR` immediate counts are 0–15. The assembler rejects an immediate of 16–255;
  the decoder treats such an encoding as count 0, so a hand-written byte stream still has
  defined behaviour.
- `DIV` is unsigned division, `DIVS` signed division truncated toward zero. The quotient
  is written to Rd; no remainder is produced (section 15, item 12).

## 8. Escape class (`1111 SSSS`)

### 8.1 `F0` — generic two-operand form

```
F0 | op | spec1 | spec2 | [extension]       4-6 bytes
```

The operation byte selects one of the operations in the table below; the two specifier
bytes select the operands; at most one operand may need an extension.

**Operation byte.**

| Byte | Operation | Operand 1 (modes) | Operand 2 (modes) |
|---|---|---|---|
| `0x00` | `MOV` | register (0) | register or immediate (0, 1) |
| `0x01` | `ADD` | register (0) | register or immediate (0, 1) |
| `0x02` | `ADDC` | register (0) | register or immediate (0, 1) |
| `0x03` | `SUB` | register (0) | register or immediate (0, 1) |
| `0x04` | `SUBB` | register (0) | register or immediate (0, 1) |
| `0x05` | `AND` | register (0) | register or immediate (0, 1) |
| `0x06` | `OR` | register (0) | register or immediate (0, 1) |
| `0x07` | `XOR` | register (0) | register or immediate (0, 1) |
| `0x08` | `CMP` | register (0) | register or immediate (0, 1) |
| `0x09` | `MUL` | register (0) | register (0) |
| `0x0A` | `MULH` | register (0) | register (0) |
| `0x0B` | `SHL` | register (0) | register or immediate (0, 1) |
| `0x0C` | `SHR` | register (0) | register or immediate (0, 1) |
| `0x0D` | `SAR` | register (0) | register or immediate (0, 1) |
| `0x0E` | `LD` | register (0) | address (2, 3, 4, 5, 6) |
| `0x0F` | `ST` | register (0) | address (2, 3, 4, 5, 6) |
| `0x10` | `LDBU` | register (0) | address (2, 3, 4, 5, 6) |
| `0x11` | `LDBS` | register (0) | address (2, 3, 4, 5, 6) |
| `0x12` | `STB` | register (0) | address (2, 3, 4, 5, 6) |
| `0x13` | `LEA` | register (0) | address (2, 3, 4, 5, 6) |
| `0x14` | `NEG` | register (0) | unused (0) |
| `0x15` | `NOT` | register (0) | unused (0) |
| `0x16` | `SXTB` | register (0) | unused (0) |
| `0x17` | `DIV` | register (0) | register (0) |
| `0x18` | `DIVS` | register (0) | register (0) |
| `0x19` | `JMP` | target (0, 1, 2, 3, 4, 5, 6) | unused (0) |
| `0x1A` | `CALL` | target (0, 1, 2, 3, 4, 5, 6) | unused (0) |
| `0x1B`–`0xFF` | reserved | — | trap |

Operand order is the data-register order the other classes use: operand 1 is the data or
destination register, operand 2 is the source, base or target. A store names its address
first in assembly — `ST [Rb+d8], Rs` — but the encoder writes Rs into specifier 1 and the
address into specifier 2. `unused` operands are encoded as mode 0, register 0 and ignored.

**Specifier byte.** Each specifier is `0 MMM RRRR`: bit 7 reserved, bits 6–4 the mode, bits
3–0 the register. For mode 1 (immediate) the register field is reserved and must be zero.

**Modes.** Mode numbers are those of section 9: 0 register direct, 1 immediate, 2 `[Rb]`,
3 `[Rb]+`, 4 `-[Rb]`, 5 `[Rb+d8]`, 6 `[Rb+d16]`, 7 reserved.

**Extensions.** They follow the two specifier bytes, in operand order: mode 1 and mode 6
take an `imm16`/`d16` (two bytes, LE); mode 5 takes a `d8` (one byte). An encoding in which
both operands need an extension is illegal, which keeps the form within four to six bytes.

**Absolute addressing.** There is no separate absolute mode in the generic form: an
absolute address is `[R0 + d16]` (mode 6 with base 0), because R0 reads as zero.

### 8.2 `F1` — long integer

```
F1 | OOOO RRRR | addr16       4 bytes
```

| Op | Instruction |
|---|---|
| 0 | `LDBU Rd, [addr16]` |
| 1 | `LDBS Rd, [addr16]` |
| 2 | `STB [addr16], Rd` |
| 3 | `LEA Rd, [PC+d16]` |
| 4 | `LEA Rd, [SP+d16]` |
| 5–15 | reserved; trap |

For ops 0–2, `RRRR` is the data register. For ops 3–4, the 16-bit field is a displacement
added to the instruction address (op 3) or to the current SP (op 4), modulo 65536.

### 8.3 `F3` — FP register-register

```
F3 | OOOO Fd Fs               2 bytes
```

| Op | Instruction | Op | Instruction |
|---|---|---|---|
| 0 | `FADD Fd, Fs` | 5 | `FABS Fd` |
| 1 | `FSUB Fd, Fs` | 6 | `FSQRT Fd` |
| 2 | `FMUL Fd, Fs` | 7 | `FMOV Fd, Fs` |
| 3 | `FDIV Fd, Fs` | 8 | `FCMP Fd, Fs` |
| 4 | `FNEG Fd` | 9 | `FRND Fd` |
| | | 10–15 | reserved; trap |

`FADD`/`FSUB`/`FMUL`/`FDIV`/`FMOV` are binary: `Fd` is both destination and left operand,
`Fs` the right operand. `FNEG`, `FABS`, `FSQRT` and `FRND` are unary: `Fs` is reserved and
must be zero.

### 8.4 `F4` — FP load/store

```
F4 | OOOO Fd MM | BBBB 0000 | [d8]       3-4 bytes
```

`OOOO`: 0 `FLD` (8-byte), 1 `FST` (8-byte), 2 `FLDS` (4-byte load and widen), 3 `FSTS`
(4-byte store and narrow); 4–15 reserved. `MM`: 00 `[Rb]`, 01 `[Rb+d8]`, 10 `[Rb]+`,
11 `-[Rb]`. The `d8` byte is present only when `MM = 01`. `BBBB` is the base register; the
low nibble of that byte is reserved and must be zero.

`[Rb]+` adds 8 for `FLD`/`FST` and 4 for `FLDS`/`FSTS`; `-[Rb]` subtracts the same amount
before the access.

### 8.5 `F5` — FP conversion

```
F5 | OOOO Fd 00 | RRRR 0000    3 bytes
```

| Op | Instruction | Meaning |
|---|---|---|
| 0 | `I2F Fd, Rb` | signed 16-bit to binary64 |
| 1 | `U2F Fd, Rb` | unsigned 16-bit to binary64 |
| 2 | `F2I Rb, Fd` | binary64 to signed 16-bit, truncating toward zero |
| 3 | `F2U Rb, Fd` | binary64 to unsigned 16-bit, truncating toward zero |
| 4–15 | reserved; trap |

A conversion of NaN or of a value outside the destination range raises an illegal-operand
trap (vector 3). `F2I` of −32768 is in range; −32768.5 truncates to −32768. The low two
bits of the first payload byte are reserved and must be zero.

### 8.6 `F6` — FP push/pop

```
F6 | OOOOOO Fd                 2 bytes
```

| Op | Instruction | Effect |
|---|---|---|
| 0 | `FPUSH Fd` | SP -= 8; store 8 bytes of Fd at SP |
| 1 | `FPOP Fd` | load 8 bytes at SP into Fd; SP += 8 |
| 2–63 | reserved; trap |

### 8.7 `F7`/`F8` — `ENTER`

```
F7 | frame8 | mask16           4 bytes
F8 | frame16 | mask16          5 bytes
```

The frame size is unsigned; the assembler rejects an odd value. Mask bits 13, 14 and 15
must be zero. See section 10.7 for the operation.

### 8.8 `F2` and `F9`–`FF`

`F2` is reserved for 32- and 64-bit integer operations. `F9`–`FF` are reserved. All of
them are illegal instructions and trap with vector 2.

## 9. Addressing modes

| Mode | Name | Syntax | Extension | Available in |
|---|---|---|---|---|
| 0 | Register direct | `Rd` | — | generic operand 1/2, ALU forms |
| 1 | Immediate | `#imm8`, `#imm16` | `imm8` or `imm16` | generic operand 2, Register/Normal forms |
| 2 | Register indirect | `[Rb]` | — | 2-byte loads/stores, generic, FP |
| 3 | Auto-increment | `[Rb]+` | — | 2-byte loads/stores, generic, FP |
| 4 | Auto-decrement | `-[Rb]` | — | 2-byte loads/stores, generic, FP |
| 5 | 8-bit displacement | `[Rb+d8]` | `d8` | Normal, generic, FP |
| 6 | 16-bit displacement | `[Rb+d16]` | `d16` | generic only |
| 7 | reserved | `[Rb+Ri]` when defined | — | — |

Every mode accepts all sixteen registers as base and as data; there is no mode that works
only for R0–R7. Auto-increment and auto-decrement step by the operand size: 1 for byte
accesses, 2 for word, 8 for `FLD`/`FST`, 4 for `FLDS`/`FSTS`.

Two PC-relative rules, deliberately different:

- A **branch** displacement is relative to the address of the next instruction.
- A **data** displacement (`LD Rd,[PC+d8]` and `LEA Rd,[PC+d16]`) is relative to the
  address of the instruction that contains it.

## 10. Instruction semantics

Flags not named in a row are unchanged. Setting flags means writing Z, N, C and V as
stated; an instruction that "sets Z N" writes 1 or 0 to both.

### 10.1 Data movement

| Instruction | Operation | Flags |
|---|---|---|
| `MOV Rd, Rb` | Rd = Rb | none |
| `MOV Rd, #imm` | Rd = imm16 | none |
| `CLR Rd` | alias: `MOV Rd, R0` | none |
| `LD Rd, [addr]` | Rd = word at address | none |
| `ST [addr], Rd` | word at address = Rd | none |
| `LDBU Rd, [addr]` | Rd = zero-extended byte | none |
| `LDBS Rd, [addr]` | Rd = sign-extended byte | none |
| `STB [addr], Rd` | byte at address = low byte of Rd | none |
| `LEA Rd, [addr]` | Rd = effective address | none |
| `SXTB Rd` | Rd = sext8(low byte of Rd) | Z N; C=0 V=0 |
| `PUSH #mask` / `POP #mask` | see 10.6 | none |

### 10.2 Integer arithmetic and logic

| Instruction | Operation | Flags |
|---|---|---|
| `ADD Rd, x` | Rd = Rd + x | Z N C V |
| `ADDC Rd, x` | Rd = Rd + x + C | Z N C V |
| `SUB Rd, x` | Rd = Rd − x | Z N C V |
| `SUBB Rd, x` | Rd = Rd − x − C | Z N C V |
| `CMP Rd, x` | flags from Rd − x; no write | Z N C V |
| `INC Rd` | `ADD Rd, #1` | Z N C V |
| `DEC Rd` | `SUB Rd, #1` | Z N C V |
| `NEG Rd` | Rd = 0 − Rd | Z N C V |
| `AND Rd, x` | Rd = Rd & x | Z N; C=0 V=0 |
| `OR Rd, x` | Rd = Rd \| x | Z N; C=0 V=0 |
| `XOR Rd, x` | Rd = Rd ^ x | Z N; C=0 V=0 |
| `NOT Rd` | Rd = ~Rd | Z N; C=0 V=0 |
| `MUL Rd, Rb` | Rd = low word of Rd × Rb | Z N from the low word; C=0 V=0 |
| `MULH Rd, Rb` | Rd = high word of the **signed** product | Z N from the high word; C=0 V=0 |
| `DIV Rd, Rb` | Rd = unsigned Rd ÷ Rb | Z N; C=0 V=0 |
| `DIVS Rd, Rb` | Rd = signed Rd ÷ Rb, truncating | Z N; C=0 V=0 |

Carry is the carry out for `ADD`/`ADDC`/`INC` and the no-borrow bit for `SUB`/`SUBB`/`DEC`/
`NEG`/`CMP`; V is signed overflow. `MUL` and `MULH` are the same multiplication, differing
only in which word they leave.

`MULH` is signed. An unsigned 16×16→32 product's high word is
`MULH(a, b) + (a < 0 ? b : 0) + (b < 0 ? a : 0)`, because a value with bit 15 set is
negative when read as signed: adding the other operand once restores its 2^16 contribution.

`DIV`/`DIVS` by zero raises an illegal-operand trap (vector 3). `DIVS` of −32768 by −1
wraps to −32768 and does not trap; C++ leaves the result undefined and the hardware does
not make it a trap.

### 10.3 Shifts

| Instruction | Operation | Flags |
|---|---|---|
| `SHL Rd, n` | Rd = Rd << n | Z N; C = last bit shifted out; V=0 |
| `SHR Rd, n` | Rd = Rd >> n, zero fill | Z N; C = last bit shifted out; V=0 |
| `SAR Rd, n` | Rd = Rd >>> n, sign fill | Z N; C = last bit shifted out; V=0 |

`n` is 0–15, from an immediate (Normal class) or a register (Compact class); a register
count has its low four bits used, so a count of 16 behaves as 0. With `n = 0` the result is
unchanged and C is unchanged. "Last bit shifted out" is bit `16 − n` of the original value
for `SHL` and bit `n − 1` for `SHR`/`SAR`.

### 10.4 Compare and set

`SETcc Rd` writes 1 when the condition holds and 0 otherwise; it reads flags and writes
none.

### 10.5 Branches and calls

| Instruction | Operation |
|---|---|
| `Bcc rel9` | if condition is true, PC += d (d relative to next instruction) |
| `BRA rel9` | PC += d |
| `CALL rel9` | push PC+2, then PC += d |
| `JMP addr16` / `JMP Rd` / `JMP [addr]` | PC = target |
| `CALL addr16` / `CALL Rd` / `CALL [addr]` | push the address of the next instruction, then PC = target |
| `RET` | PC = the word popped from the stack |

All branch displacements and the pushes are modulo 65536. `CALL` decrements SP by 2 and
stores the return address at the new SP. `RET` loads the word at SP into PC and adds 2 to
SP. Neither touches flags.

An indirect `JMP` or `CALL` may name any register, including R0 and R1; the assembler
emits the one-byte `CALL Rd` form and the two-byte `JMP Rd` form for register targets.

### 10.6 Mask push and pop

`PUSH #mask` stores one word for each set bit, in ascending register order: R0 first, R15
last, so the highest-numbered register ends at the lowest address. Each set bit decrements
SP by 2 before storing. `POP #mask` reads in descending order, R15 first and R0 last,
which exactly reverses a `PUSH` of the same mask; each set bit loads then increments SP.

Bits 14 (PC) and 15 (SP) MUST be zero; a mask with either set is an illegal instruction.
Bits 0 and 1 are legal and move the constants 0 and 1. The instruction is atomic with
respect to interrupts: delivery happens only between instructions.

### 10.7 Frames: `ENTER` and `LEAVE`

```
ENTER #frame, #mask
    SP -= 2; [SP] = FP          ; save the caller's frame pointer
    FP = SP                     ; FP points at the saved FP slot
    PUSH #mask                  ; ascending order, as in 10.6
    SP -= frame                 ; locals and outgoing arguments

LEAVE #mask
    SP = FP - 2 * popcount(mask)
    POP #mask                   ; descending order
    FP = [SP]; SP += 2          ; SP now points at the return address
```

`LEAVE` does not pop the return address. A normal epilogue is `LEAVE #mask; RET`; a tail
call is `LEAVE #mask; JMP target`. Computing SP from FP means a frame extended at run time
(`alloca`) still unwinds. Mask bits 13, 14 and 15 MUST be zero, and the frame size MUST be
even. Neither instruction touches flags or exception state.

`ENTER` is atomic with respect to interrupts: no delivery occurs inside it.

### 10.8 System instructions

| Instruction | Effect |
|---|---|
| `NOP` | none |
| `EI` | I = 1 |
| `DI` | I = 0 |
| `HALT` | stop execution; the machine is halted |
| `BRK` | breakpoint trap, vector 1 |
| `SYS` | host hypercall, section 13; not a trap |
| `RDCYCLE` | R2 = low word of the free-running cycle counter; flags unchanged |
| `IRET` | return from a trap, section 11.4 |

### 10.9 Floating point

All FP arithmetic is binary64 with round-to-nearest-even, no traps and no status register:
overflow produces infinity, an invalid operation a quiet NaN, division by zero infinity.
NaN payloads and the sign of zero are not specified beyond IEEE-754.

| Instruction | Operation | Flags |
|---|---|---|
| `FADD Fd, Fs` | Fd = Fd + Fs | none |
| `FSUB Fd, Fs` | Fd = Fd − Fs | none |
| `FMUL Fd, Fs` | Fd = Fd × Fs | none |
| `FDIV Fd, Fs` | Fd = Fd ÷ Fs | none |
| `FNEG Fd` | sign bit inverted | none |
| `FABS Fd` | sign bit cleared | none |
| `FSQRT Fd` | correctly rounded square root | none |
| `FMOV Fd, Fs` | Fd = Fs | none |
| `FCMP Fd, Fs` | N = Fd < Fs; Z = Fd == Fs; V = unordered; C = 0 | Z N C V |
| `FRND Fd` | round to the nearest binary32 value, widened back | none |
| `FLD Fd, addr` | load 8 bytes | none |
| `FST addr, Fd` | store 8 bytes | none |
| `FLDS Fd, addr` | load 4 bytes, binary32 to binary64 | none |
| `FSTS addr, Fd` | store 4 bytes, binary64 to binary32 | none |
| `FPUSH Fd` / `FPOP Fd` | 8-byte stack move | none |
| `I2F`, `U2F`, `F2I`, `F2U` | conversions; out of range traps | none |

`FRND` makes strict binary32 arithmetic possible on a binary64 datapath: an `f32`
operation is computed in binary64 and the result is rounded to binary32 precision before
it is used. `FLDS` widens a binary32 value exactly; `FSTS` narrows with
round-to-nearest-even.

## 11. Traps

### 11.1 Causes

| Vector | Name | Cause |
|---|---|---|
| 0 | reset | not a trap: the reset vector, `PC = [0x0100]` |
| 1 | breakpoint | `BRK`, or the single-step trap when T is set |
| 2 | illegal instruction | reserved or unallocated encoding, non-zero reserved field |
| 3 | illegal operand | divide by zero, invalid float↔integer conversion, unknown `SYS` number |
| 4 | address error | access in the null page, the reserved window, or an undefined I/O register |
| 5 | external interrupt | reserved; no source in v1 |
| 6 | timer interrupt | reserved; no source in v1 |
| 7 | console interrupt | reserved; no source in v1 |
| 8–15 | reserved | no source in v1 |

### 11.2 Priority

At most one trap is raised per instruction. When more than one condition applies, the
lowest-numbered vector wins after the ordering illegal instruction < illegal operand <
address error; a fetch of an illegal encoding is therefore reported before any operand
check.

### 11.3 Entry

```
SP -= 6
[SP+0] = EFP            ; previous exception frame pointer
[SP+2] = faulting PC    ; address of the instruction that trapped
[SP+4] = flags
EFP = SP
flags.I = 0
PC = word at 0x0100 + 2 * vector
```

The saved PC is the address of the faulting instruction, not the next one, so a handler
that wants to resume past a `BRK` must add 1 to `[EFP+2]` before `IRET`. The trace trap is
the one exception: it is taken after the instruction has completed and its saved PC is the
next instruction's address, so `IRET` resumes there and the trace flag re-arms. If the frame
push itself falls in the null page or the reserved window, the machine halts: there is no
double-fault path.

A trap handler that uses F0–F3 must save them itself; the hardware frame does not.

### 11.4 `IRET`

```
SP = EFP
flags = [SP+4]
PC = [SP+2]
EFP = [SP+0]
SP += 6
```

`IRET` restores every flag bit from the saved word, including I and T.

## 12. Boot

The machine starts at the reset state of section 2.4 and executes from the address in the
word at `0x0100`. The image writer places that word at `0x0100`; the linker's default
layout puts `crt0` at `0x0200`, so the reset word is normally `0x0200`.

## 13. `SYS` host calls

`SYS` (0x73) enters the emulator directly. It is a virtual-machine hypercall: it does not
use the vector table, does not push an exception frame, and does not change flags.

| Register | Use |
|---|---|
| R7 | call number, and not preserved |
| R2 | argument 0 / result |
| R3 | argument 1 |
| R4 | argument 2 |
| R5 | argument 3 |

| Call | Number | Arguments | Result |
|---|---|---|---|
| `exit` | 0 | R2 = status | never returns; the machine halts with that status |
| `write` | 1 | R2 = fd, R3 = address, R4 = length | R2 = bytes written, or a negative error |
| `read` | 2 | R2 = fd, R3 = address, R4 = length | R2 = bytes read, or a negative error; 0 is end of input |
| `time` | 3 | — | R2:R3 = 32-bit seconds since the epoch (low word, high word) |
| `debug_write` | 4 | R2 = address, R3 = length | R2 = 0 |

Call numbers 5 and above are reserved and raise an illegal-operand trap (vector 3). A failed
call returns a negative value in R2; the individual codes are the emulator's host-service
contract.

## 14. Memory-mapped I/O

Registers are 16 bits wide unless stated. An access in `0xFF00`–`0xFF7F` to an address not
listed here raises an address-error trap (vector 4).

| Address | Device | Register |
|---|---|---|
| `0xFF00` | Console | `OUT`: write the low byte to the output stream |
| `0xFF02` | Console | `IN`: read the next input byte, zero-extended |
| `0xFF04` | Console | `STATUS`: bit 0 RX_READY, bit 1 TX_READY, bit 2 OVERRUN |
| `0xFF10` | Timer | 16-bit counter |
| `0xFF12` | Timer | 16-bit compare |
| `0xFF14` | Timer | control: bit 0 enable, bit 1 interrupt enable |
| `0xFF20` | System | write any value to halt |
| `0xFF22` | System | write any value to reset the machine |
| `0xFF24` | System | free-running cycle counter, low word |
| `0xFF26` | System | free-running cycle counter, high word |
| `0xFF30`–`0xFF7F` | future | reserved; traps |

Reads of `OUT`, the timer control and compare, or the halt/reset registers return 0.
Console status bits match `krdbg::ComPortStatus`. Polled I/O in v1: no device raises an
interrupt.

## 15. Decisions recorded here

The plan fixes the shape and leaves these to this document. Each is normative from this
revision; changing one is an ISA change and takes the plan's change policy.

1. **Vector numbering.** 1 breakpoint, 2 illegal instruction, 3 illegal operand, 4 address
   error, 5–7 interrupt sources reserved, 8–15 unused. The plan fixes only vector 3.
2. **Trap entry order and the saved PC.** The frame is `{EFP, PC, flags}` at ascending
   addresses, EFP points at it, `I` is cleared, and the saved PC is the faulting
   instruction's address.
3. **`F0` opcode allocation** (0x00–0x1A) and the specifier byte layout `0 MMM RRRR`.
4. **`F0` operand rules:** specifier 1 is the data/destination register and specifier 2 the
   source, base or target, matching `Ra`/`Rb` in the Compact class; at most one operand may
   carry an extension, so the form stays 4–6 bytes; absolute addressing is `[R0+d16]`. F0's
   length is therefore decided by its specifier bytes, which makes it the second prefix —
   beside `F4` — whose length the bytes after the prefix determine.
5. **`F1` op order:** 3 is `LEA Rd,[PC+d16]`, 4 is `LEA Rd,[SP+d16]`.
6. **`F6` op numbers:** 0 `FPUSH`, 1 `FPOP`.
7. **Reserved fields must be zero** and trap otherwise, including `Fs` in unary `F3` ops,
   the `Rb` field of a unary Compact operation or `JMP`, the register field of an immediate
   specifier, and the low nibble of the `F4` base byte.
8. **`MULH` is signed.** The unsigned high word is the correction sequence in 10.2.
9. **Flags for the instructions the plan does not classify:** `INC`/`DEC` are `ADD`/`SUB`
   with 1; `NEG` is `0 − Rd`; `MUL`/`MULH`/`DIV`/`DIVS`/`SXTB` set Z and N, clear C and V;
   `SETcc`, `LEA`, all branches and all frame/system instructions leave flags unchanged.
10. **Shift counts are 0–15**; the assembler rejects an immediate of 16–255, and a register
    count uses its low four bits; `n = 0` leaves C unchanged.
11. **`MUL`'s overflow is not trapped**, and `DIVS` of −32768 by −1 wraps; C++ leaves both
    undefined.
12. **`DIV`/`DIVS` produce the quotient only.** No remainder is produced; the backend
    synthesizes `a − (a ÷ b) × b` for `%`, which is why no `MOD` claims an opcode.
13. **`SYS` numbering** 0–4 and the reserved-5+ trap.
14. **MMIO register offsets** in section 14.
15. **Undefined I/O addresses trap** rather than reading zero.
16. **`BRK`'s resumability rule:** the handler advances the saved PC by hand.

## 16. Deferred

- **Bit operations** (`BTST`/`BSET`/`BCLR`) — the plan's Q7. They would claim Normal-class
  opcodes 29–31 or `F0` bytes.
- **Block move/fill** — Q8. Post-increment loops are the v1 mechanism.
- **Indexed addressing** `[Rb+Ri]` — mode 7 is reserved.
- **32- and 64-bit integer operations** — the `F2` escape.
- **A more than 64 KiB memory** — banking through the escape class.
- **An unsigned high-word multiply (`MULHU`)** — free `F0` byte 0x1B, if the correction
  sequence in 10.2 measures too expensive.
- **Remainder instruction** — see 15.12.
- **Interrupt sources** — vectors 5–7 exist but nothing raises them.

## Appendix A — Encoding vectors

`asm -> bytes`, all little-endian.

| Assembly | Bytes |
|---|---|
| `push fp` | `0D` |
| `pop fp` | `1D` |
| `ret` | `70` |
| `nop` | `71` |
| `brk` | `72` |
| `sys` | `73` |
| `ei` | `74` |
| `di` | `75` |
| `halt` | `76` |
| `iret` | `77` |
| `rdcycle` | `78` |
| `inc r2` | `22` |
| `dec r2` | `32` |
| `call r3` | `43` |
| `mov r3, r0` | `80 30` |
| `add r3, r4` | `81 34` |
| `cmp r4, r0` | `88 40` |
| `ldbu r4, [r2]` | `90 42` |
| `ldbs r4, [r2]` | `91 42` |
| `jmp r3` | `9F 30` |
| `mov fp, sp` | `80 DF` |
| `neg r2` | `9D 20` |
| `not r2` | `9E 20` |
| `beq +6` | `A0 06` |
| `bra -9` | `BF F7` |
| `ld r4, [r5+3]` | `C0 45 03` |
| `st [r5+3], r4` | `C1 45 03` |
| `ld r4, #0x34` | `54 34 00` |
| `setlt r2` | `D3 2B 00` |
| `shl r2, #3` | `D4 20 03` |
| `div r2, r3` | `D9 23` |
| `sxtb r4` | `DB 40 00` |
| `call 0x0200` | `D0 00 02` |
| `jmp 0x0200` | `CF 00 02` |
| `push #0x0300` | `D7 00 03` |
| `pop #0x0300` | `D8 00 03` |
| `leave #0x0300` | `DC 00 03` |
| `ld r2, #0x1234` | `52 34 12` |
| `ld r6, [0x4000]` | `66 00 40` |
| `st [0x4000], r6` | `E6 00 40` |
| `ldbu r2, [0x4000]` | `F1 02 00 40` |
| `lea r2, [sp+6]` | `F1 42 06 00` |
| `fadd f0, f1` | `F3 01` |
| `fadd f2, f3` | `F3 0B` |
| `fcmp f0, f1` | `F3 81` |
| `frnd f1` | `F3 94` |
| `fld f0, [r2+8]` | `F4 01 20 08` |
| `fst f1, [r2]` | `F4 14 20` |
| `fsts f1, [r2+4]` | `F4 35 20 04` |
| `i2f f0, r2` | `F5 00 20` |
| `f2i r2, f0` | `F5 20 20` |
| `fpush f1` | `F6 01` |
| `fpop f1` | `F6 05` |
| `enter #8, #0x0300` | `F7 08 00 03` |
| `enter #300, #0` | `F8 2C 01 00 00` |
| `mov r2, r3` (generic) | `F0 00 02 03` |
| `ld r2, [r3+4]` (generic) | `F0 0E 02 53 04` |
| `mov r2, #0x4000` (generic) | `F0 00 02 10 00 40` |
| `ld r2, [r3+6]` (generic) | `F0 0E 02 63 06 00` |
| `st [r3+2], r2` (generic) | `F0 0F 02 53 02` |

Notes on the vectors:

- `ld r4, #0x34` assembles to the Register form `54 34 00`; Normal op 6 (`C6 40 34`) and op
  5 are decode-only aliases, and the assembler never emits them.
- `setlt r2` is Normal op 19, payload `RRRR CCCC` = `0x2B` (R2, cond 11), then `0x00`.
- `neg r2` and `not r2` are Compact ops 29 and 30 with `Rb = R0`.
- Generic `st` keeps the data register in specifier 1 (mode 0, R2 = `0x02`) and the address
  in specifier 2 (mode 5, R3 = `0x53`), with the single `d8` extension last.

## Appendix B — First-byte map

```
0x00-0x0F  PUSH Rd            1
0x10-0x1F  POP Rd             1
0x20-0x2F  INC Rd             1
0x30-0x3F  DEC Rd             1
0x40-0x4F  CALL Rd            1
0x50-0x5F  LD Rd, #imm16      3
0x60-0x6F  LD Rd, [addr16]    3
0x70-0x77  system             1
0x78       RDCYCLE            1
0x79-0x7F  reserved           trap
0x80-0x9F  Compact register   2
0xA0-0xBF  Compact branch     2
0xC0-0xDF  Normal             3
0xE0-0xEF  ST [addr16], Rd    3
0xF0       generic            4-6
0xF1       long integer       4
0xF2       reserved           trap
0xF3       FP register        2
0xF4       FP load/store      3-4
0xF5       FP convert         3
0xF6       FP push/pop        2
0xF7       ENTER short        4
0xF8       ENTER long         5
0xF9-0xFF  reserved           trap
```
