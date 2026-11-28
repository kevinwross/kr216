# kr216 — Host text display

| | |
|---|---|
| Status | **Proposed extension, version 2** — this is the kr216 binding of the common display ABI (version 2). The host device is KRCompiler's `krc::display` package and the guest side is the common `<krc/display.h>` API with the kr216 runtime; `kr216` contributes only the SYS 5 front end. It claims an ISA 1.x reserved SYS number; freezing it takes the ISA revision described in §6 |
| Date | 2026-11-27T22:05:00-08:00 |
| Authority | `kr216-isa.md` §13 fixes the SYS register roles and §15.13 reserves calls 5 and above; `kr216-plan.md` §14 fixes the device model; `..\krcompiler\docs\display-abi.md` fixes the common command set, canonical block and statuses |
| Applies to | `kr216`'s `Machine` SYS 5 front end and guest tests, KRCompiler's `krc::display` package and kr216 runtime, and the krdbg kr216 adapter's DISPLAY surface |
| Open items | §6 (adoption into the frozen ISA) and §10 (UTF-8 cell codes) |

The key words MUST, MUST NOT, SHOULD and MAY are used as in RFC 2119.

The host owns the text screen. The guest has no screen memory, no framebuffer and no
cell-readback command: it writes through the SYS 5 hypercall and the host renders the cells.
The device is an output-and-polled-input terminal, not a graphics adapter.

The display is the program's **stdin and stdout**: the C runtime's `putchar`, `puts`, `printf`,
`getchar` and `getc` use it (§8). The console device (`0xFF00`/`0xFF02`/`0xFF04`, SYS 1/SYS 2)
is the **debug serial port**, not stdio: a program reaches it only by calling `__krc_write` /
`__krc_debug_write` (§7). In the debugger that split is the DISPLAY tab and the COM tab.

## 1. Scope

Version 1 provides a character-cell display with per-cell foreground and background colour, a
hardware cursor, a host-side line editor, and polled keyboard and mouse input. It deliberately
leaves out:

- **Guest screen memory and readback.** Every write is a hypercall; there is no command that
  returns a cell.
- **Interrupts.** Input is polled, exactly like the console (`kr216-isa.md` §14: "Polled I/O in
  v1: no device raises an interrupt"). There is no WaitingForHost stop state.
- **Alpha.** Colours are XRGB8888; the high byte is stored and not interpreted.
- **UTF-8.** Cell codes are 16 bits, but guest strings are 8-bit ASCII today (§10).
- **The wheel.** A click carries the button mask only.

## 2. Machine model

| Property | Default | Notes |
|---|---|---|
| Grid | 80 columns × 30 rows | `MachineOptions::display_cols` / `display_rows` at construction; 1–32767, at most 4,000,000 cells |
| Cell | `{u16 code, u32 fg, u32 bg}` | A code of 0 renders blank; codes 0x08, 0x09, 0x0A, 0x0D are control actions (§5) |
| Colour | XRGB8888 | High byte stored verbatim and not interpreted; no alpha blending |
| Default foreground | `0xFF_FFFFFF` | White |
| Default background | `0xFF_000000` | Black |
| Cursor | row, column, visible | Visible at (0, 0) after reset |
| Reset | clear, home, show, defaults | `TextDisplay::reset()`; `Machine::reset()` calls it and reopens a closed input source |

`TextDisplay::clear()` paints every cell with the *current* colours and code 0 and leaves the
cursor alone. It is the CLEAR command. `reset()` is the device reset: it restores the
construction colours and geometry, homes and shows the cursor, clears the screen, and drops
the key queue, click queue, line editor, any completed line and the input-closed state.

The screen scrolls one row when a write advances past the last row: rows 1..N-1 move up,
the bottom row is filled with code 0 in the current colours, and the cursor stays on the
bottom row. A write past the last column moves to column 0 of the next row first.

## 3. SYS 5

SYS 5 is a hypercall in the shape of `kr216-isa.md` §13: it enters the emulator directly,
uses no vector, pushes no exception frame and leaves the flags alone.

| Register | On entry | On return |
|---|---|---|
| R7 | 5 | unchanged |
| R2 | command number | status or result, as a signed 16-bit value |
| R3 | guest argument-block address | unchanged |
| R4 | argument-block length in bytes | unchanged |
| R5 | unused; MUST be ignored | unchanged |

A command with an argument block states its exact byte length in R4; the host rejects a
shorter or longer block with `-1`. Commands without a block ignore R3 and R4.

| Status | Value | Meaning |
|---|---:|---|
| `DISPLAY_OK` | 0 | the command succeeded |
| `DISPLAY_BAD_ARGUMENT` | -1 | bad block, bad pointer, bad length or bad argument value |
| `DISPLAY_NOT_READY` | -2 | no key or click is queued, or a line is still being edited |
| `DISPLAY_UNKNOWN_COMMAND` | -3 | the host does not know the command number |
| `DISPLAY_END_OF_INPUT` | -4 | `READ_LINE`/`GET_KEY` only: the host closed its input source |

All values in an argument block are 16-bit little-endian words: word *n* is at
`block + 2n`, low byte first. A 32-bit colour is two words, low word first. A guest pointer
inside a block is a 16-bit guest address.

### 3.1 Guest pointer validation

The host MUST validate every guest block, and every guest pointer and length inside a block,
against the guest memory map before touching memory, and MUST return `-1` instead of raising a
trap. A **guest data range** is `0x0100–0xFEFF`: the null page `0x0000–0x00FF` and the
I/O/reserved windows `0xFF00–0xFFFF` are rejected. The range must not wrap the 16-bit address
space, so `address + length <= 0xFF00` and the last byte is at most `0xFEFF`. A zero length
touches no byte and is valid whatever the address.

The host reads blocks with its raw memory view, never through the trap-checked or MMIO
accessors: a display call MUST NOT consume console input or fault.

## 4. Commands

| # | Command | R3 block | R4 | Result in R2 |
|---:|---|---|---:|---|
| 0 | `INFO` | out: 8 bytes | 8 | 0; block written |
| 1 | `CLEAR` | ignored | ignored | 0 |
| 2 | `SET_COLOR` | in: 8 bytes | 8 | 0 |
| 3 | `GET_COLOR` | out: 8 bytes | 8 | 0; block written |
| 4 | `PUT_CHAR` | in: 2 bytes | 2 | 0 |
| 5 | `PUT_STRING` | in: 4 bytes | 4 | 0 |
| 6 | `CURSOR` | in: 6 bytes | 6 | 0, or `-1` |
| 7 | `READ_LINE` | in: 4 bytes | 4 | length ≥ 0, `-2` not ready, `-4` input closed, or `-1` |
| 8 | `GET_KEY` | ignored | ignored | key code, `-2`, or `-4` |
| 9 | `GET_CLICK` | out: 6 bytes | 6 | 0 and block written, or `-2` |

Any other command number returns `-3`.

### 4.1 `INFO` (0)

| Word | Out |
|---:|---|
| 0 | ABI version, 1 for this document |
| 1 | rows |
| 2 | cols |
| 3 | reserved, 0 |

This is the capability probe. A guest MUST read the version and MUST treat a host that cannot
answer as having no display (§6). A guest that only uses the C runtime can call
`display_rows()` and `display_cols()`, which answer from `INFO`.

### 4.2 `CLEAR` (1)

Clears the screen with the current colours. The cursor, the line editor and the queues are
unchanged.

### 4.3 `SET_COLOR` (2) and `GET_COLOR` (3)

| Word | `SET_COLOR` in / `GET_COLOR` out |
|---:|---|
| 0 | foreground, low 16 bits |
| 1 | foreground, high 16 bits |
| 2 | background, low 16 bits |
| 3 | background, high 16 bits |

The colours apply to text written afterwards, to `CLEAR`, to scrolling, and to the line
editor's echo. The high byte is the X in XRGB8888: it is stored and returned verbatim, and a
renderer ignores it.

### 4.4 `PUT_CHAR` (4)

| Word | In |
|---:|---|
| 0 | cell code |

Writes one cell at the cursor and advances. Control codes are actions, not cells: `0x08`
Backspace moves left one column (never across a row boundary), `0x09` Tab advances to the
next multiple of 8 (wrapping if it passes the last column), `0x0A` Line feed moves to column 0
of the next row (scrolling at the bottom), `0x0D` Carriage return moves to column 0 of the
current row.

### 4.5 `PUT_STRING` (5)

| Word | In |
|---:|---|
| 0 | guest address of the bytes |
| 1 | byte count |

Each byte is one 16-bit cell code and goes through `PUT_CHAR`'s control rules. The bytes are
not required to be NUL-terminated and the host never reads past byte count. A count of 0
succeeds and writes nothing. An invalid range returns `-1` and writes nothing.

The kr216 C runtime's `display_puts` computes the count with a NUL scan and calls this
command, so the C API takes an ordinary C string.

### 4.6 `CURSOR` (6)

| Word | In |
|---:|---|
| 0 | row |
| 1 | column |
| 2 | visible: 0 or 1 |

A row outside `0..rows-1`, a column outside `0..cols-1`, or a visibility other than 0/1
returns `-1` and leaves the cursor unchanged.

### 4.7 `READ_LINE` (7)

| Word | In |
|---:|---|
| 0 | guest buffer address |
| 1 | capacity in bytes, at least 1 |

The call is polled:

1. If no line is being edited and no completed line is waiting, the host arms the line editor
   and returns `-2`. The editor's echo starts at the current cursor position.
2. While a line is being edited, every call returns `-2` and the editor's state is unchanged.
   The host echoes each printable key, Backspace, Left and Right; Enter completes the line and
   Escape cancels it (§5).
3. Once a line is complete, the next call copies its bytes to the buffer without a terminator,
   returns the length, and forgets the line. The buffer is not touched past the length.
4. If the completed line is longer than the current capacity the call returns `-1` and the
   line stays pending, so a caller that shrinks its buffer can retry.
5. A capacity of 0, or a buffer range that fails §3.1, returns `-1` without arming or
   consuming anything.
6. If the host has closed its input source (never to reopen before a reset) and no line is
   complete, the call returns `-4`. A non-empty draft that was being edited when the source
   closed is completed first, so the guest receives it before the `-4`.

The editor caps the line at `capacity - 1` bytes so the C wrapper can NUL-terminate it, and
at the remaining columns of its starting row. The guest only ever receives a completed line;
guest code does not echo.

### 4.8 `GET_KEY` (8)

Returns the oldest queued key code, or `-2` when the queue is empty and the input source is
open, or `-4` when the queue is empty and the host has closed it. Key codes are
`0x0000–0x7FFE`; the negative answers are `-2` (0xFFFE in R2) and `-4` (0xFFFC), so a code
that would be negative on return is not representable.

| Key | Code |
|---|---:|
| Backspace | 0x08 |
| Tab | 0x09 |
| Enter | 0x0D |
| Escape | 0x1B |
| Printable ASCII | the ASCII code |
| Left arrow | 0x0102 |
| Right arrow | 0x0103 |

The host front end owns the mapping from physical keys to these codes; codes above the ones
listed here MAY be added without an ABI change as long as they stay in `0x0000–0x7FFE`.

### 4.9 `GET_CLICK` (9)

| Word | Out |
|---:|---|
| 0 | row |
| 1 | column |
| 2 | button mask |

Returns `0` with the block written when a click is queued, or `-2` with the block untouched.
Rows and columns are cell coordinates; the host is trusted to queue coordinates inside the
screen. Buttons are a mask: left = 1, right = 2, middle = 4. There is no wheel event in
version 2.

## 5. Host line editor

The editor is host state, not guest state. It is armed by the first `READ_LINE` that finds no
completed line, and it draws at the cursor position where it was armed.

| Input | Effect |
|---|---|
| Printable ASCII (0x20–0x7E) | insert at the caret; ignored at the capacity limit |
| Backspace (0x08) | erase the character before the caret |
| Left (0x0102) | move the caret left |
| Right (0x0103) | move the caret right |
| Enter (0x0D) | complete the line; the cursor moves to column 0 of the next row (scrolling at the bottom); the echo stays visible |
| Escape (0x1B) | cancel; the echo is erased and the cursor returns to the arming position; no line is delivered |
| anything else | ignored by the editor and not queued for `GET_KEY` |

The echo uses the current colours and the current cursor. While the editor is active it
consumes every key, so raw `GET_KEY` input is only available when no editor is armed. A
completed line that no `READ_LINE` has collected leaves the editor inactive; keys typed in
that window go to the raw key queue.

### 5.1 Closing the input source

The host MAY close the input source when nothing more can arrive: a scripted session or pipe
reaching end of file, or a front end's close gesture. `Machine` uses the package's
`TextDisplay::close_input()`; the emulator closes it when `--display-input`'s script is
exhausted (§7), which is how a guest `getchar` loop sees end of input.

Closing:

- ignores later keys and text (the source stays closed until `Machine::reset()`),
- completes a non-empty editor draft as the final line, so an unterminated last line is still
  delivered, and cancels an empty draft,
- leaves an already completed line available to the next `READ_LINE`,
- makes `READ_LINE` and `GET_KEY` answer `-4` once their queued input is drained,
- leaves the screen, colours, cursor and click queue alone.

The input source is host state, not a guest command; a guest observes it only through `-4`.

## 6. Capability and version rule

This is an **ISA 1.x extension, not an ISA 1.0 instruction**. `kr216-isa.md` is frozen at 1.0
and its §13/§15.13 currently say calls 5 and above are reserved and trap. This document is the
proposal that claims call 5; the frozen ISA document is not edited here. The extension changes
no encoding, no trap vector and no existing SYS call, so an ISA 1.0 host and an ISA 1.0 image
are unaffected:

- **Old host, new image.** SYS 5 traps on the illegal-operand vector exactly as the frozen ISA
  says. A guest that wants to detect the display calls `INFO` and handles the trap, or probes
  first with `display_rows()` and treats a negative answer as "no display".
- **New host, old image.** The image never executes SYS 5, so nothing changes. An image that
  did execute SYS 5 on an ISA 1.0 host was already trapping; on the new host it now gets a
  device answer. Call numbers 6 and above still trap.
- **Version field.** `INFO` word 0 is the ABI version, 2 for this document. A new command, a
  changed block layout or a changed status value takes the next version; a guest MUST compare
  the version and MUST NOT use a layout or status it does not know. Adding key codes and cell
  codes is compatible and does not change the version. Version 2 adds `DISPLAY_END_OF_INPUT`
  and the input-close rule of §5.1; a version 1 guest that reads input must treat any negative
  answer as "no input", because it has no name for `-4`.

Adoption into the frozen ISA takes the next `kr216-isa.md` revision: record SYS 5 in §13 and
change "5 and above" to "6 and above" in §15.13, citing this document.

## 7. Host API

The device is the shared `krc::display` package (`..\krcompiler`, installed as `krc-display`);
`kr216` does not carry a copy. `Machine` owns a `krc::display::TextDisplay` and exposes it:

```cpp
krc::display::TextDisplay& Machine::display();
const krc::display::TextDisplay& Machine::display() const;
```

`MachineOptions` carries `display_rows`, `display_cols`, `display_foreground` and
`display_background`, defaulted from `krc::display`'s constants. `Machine::do_sys()` translates
SYS 5's 16-bit block into the canonical 32-bit block and calls
`krc::display::DisplayService::execute` with a `DisplayMemory` over the machine's 64 KiB; it
never traps and always writes a status or result to R2. The command semantics, the cell model
and the line editor are the package's and are specified in `..\krcompiler\docs\display-abi.md`;
`tests/display_test.cpp` keeps the SYS-level marshalling evidence.

A host that scripts input registers a callback:

```cpp
void Machine::set_display_input_refill(std::function<void()> refill);
```

It runs at the start of every SYS 5 `READ_LINE` and `GET_KEY`, before the command, so it can
queue text with `display().push_text()` once the poll has armed the editor, and call
`display().close_input()` when its script is exhausted. A windowed front end does not need it:
it pushes keys as the user types and closes the source with `close_input()` on its EOF gesture.

The console device is unchanged and is the debug serial port. SYS 1 (`write`) appends to
`Machine::console_output()` and the `0xFF00` data register does the same; SYS 2 (`read`) and
`0xFF02` consume `console_input()`. `--console` still makes the emulator read the console from
stdin, so an interactive debug session keeps working beside the display.

The emulator (`tools/kr216-emu`) does not render the display: it owns a `Machine`, so the
device answers a guest's calls, but a headless run has no window. Two options make a run
inspectable and scriptable:

| Option | Effect |
|---|---|
| `--display-input TEXT` / `--display-inputfile PATH` | queues text for the display line editor, one line at a time, and closes the input source when it is exhausted, so a `getchar` loop reaches EOF |
| `--display-dump` | after the run, prints `console:` (the debug serial bytes) and `display:` (the screen rows as text) |

The interactive surface is the debugger's DISPLAY view (phase 2), which reads the same
`TextDisplay`.

## 8. Guest runtime

`..\krcompiler` ships the guest side:

| File | Contents |
|---|---|
| `sysroot/include/krc/display.h` | the common C API `<krc/display.h>`: statuses (including `DISPLAY_END_OF_INPUT`), command/key/button codes, `display_*` declarations |
| `targets/kr216/runtime/syscalls.s` | `__krc_display` trampoline: R7 = 5, `sys`, result in R2; `__krc_write` / `__krc_exit` |
| `targets/kr216/runtime/display.c` | the kr216 wrappers: `display_clear/rows/cols/set_color/get_color/putc/puts/cursor/read_line/get_key/get_click` |
| `targets/kr216/runtime/libc.c` | stdio over the display (below), `rand`/`srand`, strings and memory, and `__krc_debug_write` for the console |

The common header is staged with the rest of the common sysroot, so `#include <krc/display.h>`
resolves under `krc --target=kr216`. A program links `display.c` beside the rest of the kr216
runtime; the wrappers build the 16-bit blocks of §4 and return R2 as-is. `display_read_line`
NUL-terminates its buffer when the returned length leaves room.

### 8.1 stdio is the display; the console is the debug serial port

| Function | Path |
|---|---|
| `putchar`, `puts`, `printf` (all conversions including `%s`) | `DISPLAY_CMD_PUT_CHAR` / `DISPLAY_CMD_PUT_STRING` |
| `getchar`, `getc`, `getc(stdin)` | `DISPLAY_CMD_READ_LINE` on the host line editor |
| `__krc_debug_write(buffer, count)`, or SYS 1 directly | the console device (debug serial) |

`getchar` keeps a per-program 256-byte line buffer. It polls `display_read_line` until the
host has a completed line (`DISPLAY_NOT_READY` means the host is still editing), serves the
line's bytes followed by `'\n'`, and returns `-1` at end of input: either
`DISPLAY_END_OF_INPUT` when the host closed the source (§5.1) or a hard display failure. A
line the host can deliver is at most the editor's capacity and the columns left on its row, so
it always fits the buffer with room for the terminator.

`rand`/`srand` are the 32-bit LCG KR32 and KR232 use (`1103515245 * state + 12345`, result
`(state >> 16) & 0x7FFF`), with `sys_random` as the step, so a seeded program answers the same
sequence on every target.

`__krc_debug_write` is the explicit serial path: it calls `__krc_write(1, ...)` and never goes
through stdio. printf-style debug output is therefore not available on the console; a program
that wants both streams keeps its display output in `printf` and its serial output as raw
bytes.

## 9. Tests and evidence

- `..\krcompiler`'s unit `tests/unit/test_display_text.cpp`,
  `test_display_surface.cpp` and `test_display_service.cpp` cover the device, the framebuffer
  core and the dispatcher, including the canonical marshalling, `close_input()` and the `-4`
  statuses.
- `tests/display_test.cpp` (kr216, host) covers the SYS 5 front end: defaults through
  `MachineOptions`, `INFO`, colour round-trips, `PUT_CHAR`/`PUT_STRING` wrapping and control
  codes, the cursor, every bad-block/bad-pointer path, the key and click queues, the line
  editor, `reset`, the 16-bit `INFO`/`GET_COLOR`/`GET_CLICK` marshalling, the display input
  refill (including `-4` when the script is exhausted), and the console/display split.
- `src216/display.c` is the guest evidence for the raw wrappers: it calls every wrapper and the
  raw hypercall under the emulator, checks the documented statuses and exits 42. Its CTest row
  is `guest_display`.
- `src216/stdio.c` is the stdio evidence: `printf` writes the screen, `getchar`/`getc` read the
  host-edited lines, `rand` is deterministic, a `getchar` loop reaches EOF when the scripted
  input is exhausted, and the explicit `__krc_debug_write` text appears only in the console
  section. Its CTest row is `guest_stdio`.
- `guest_tictactoe` builds `..\krcompiler\examples\tictactoe.cpp` with the kr216 runtime and no
  input/PRNG shims, plays a scripted game through the display line editor and exits 0 at EOF.
- The guest rows are opt-in: `-DKR216_BUILD_GUEST_TESTS=ON`.
- `..\krcompiler`'s `krc_kr216_display_link` compiles the runtime and a fixture that calls
  every wrapper and references `getchar`/`rand`/`srand`, links the image and checks that the
  entry points survive in its symbol payload; `krc_display_package_consumer` installs the
  build tree into a private prefix and builds a separate CMake project against
  `find_package(krc-display)`.

## 10. Deferred

- **UTF-8 strings.** A future revision can define `PUT_STRING` as UTF-8 and keep 16-bit cell
  codes; today every byte is a code.
- **Cell readback.** A command that returns cells would let a guest verify its own drawing;
  it would also make the host's screen state part of the guest's observable model.
- **Interrupts and a WaitingForHost stop state.** Version 2 stays polled.
- **Interactive display input in the emulator.** `--display-input` is a script; the emulator
  does not read the display from the terminal. `--console` remains the console's interactive
  mode.
- **Wheel and pointer motion.** Clicks carry a button mask at a cell coordinate only.
- **Cursor shape and blinking.** The ABI carries position and visibility only; a renderer
  chooses how to draw the cursor.

## 11. Version history

| Version | Date | Change |
|---|---|---|
| 0.1 | 2026-11-27 | First draft: machine model, SYS 5 ABI, command set, line editor, guest runtime, capability rule. |
| 0.2 | 2026-11-27 | The device moved to KRCompiler's shared `krc::display` package and the guest API to the common `<krc/display.h>`; this document now records the kr216 binding and cross-references `..\krcompiler\docs\display-abi.md`. |
| 0.3 | 2026-11-27 | Display ABI version 2: `DISPLAY_END_OF_INPUT` (-4) and the host input-close rule (§5.1). stdio (putchar/puts/printf/getchar/getc) is defined on the display and the console becomes the debug serial port; the runtime gains `getchar`/`getc`, `rand`/`srand` and `__krc_debug_write`, and the emulator gains `--display-input`/`--display-dump`. |
