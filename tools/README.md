# Python JTAG client

Install `requirements.txt` into a Python virtual environment and flash the
SerialTransfer adapter (`pio run -e nano_serial -t upload`). Run commands from
the repository root. See the [wire protocol guide](../include/protocol/README.md)
for wiring, raw commands and adapter limitations.

## JSON chain configuration

Use `--config FILE` to describe the complete chain. The `taps` array is in
physical **TDI-to-TDO order**. Each TAP has a unique, case-sensitive name and
either a BSDL file or a manually specified IR length:

```json
{
  "version": 1,
  "taps": [
    {
      "name": "boundary",
      "bsdl": "./bsdl/STM32F405_415_407_417_LQFP100.bsd",
      "aliases": {"USER": "PA0", "LD4": "PD12"}
    },
    {"name": "debug", "ir_length": 4}
  ],
  "default_target": "boundary"
}
```

BSDL paths are relative to the JSON file, not the current working directory.
Absolute paths are also supported. IR length is derived from BSDL; an explicit
`ir_length` alongside BSDL must match it. A TAP without BSDL participates in
standard all-ones, one-bit BYPASS but cannot use IDCODE or pin commands.
`default_target` defaults to the first TAP if omitted. Aliases are scoped to
each TAP; BSDL port names and aliases are case-insensitive.

All BSDL files are loaded before opening the serial port. Missing files,
unknown fields, duplicate JSON keys/TAP names, invalid targets and inconsistent
IR lengths fail explicitly. TAP names start with a letter and contain letters,
digits, `_` or `-`. There is no automatic chain discovery. All non-target TAPs
must implement all-ones BYPASS with a one-bit DR.

For STM32F4DISCOVERY use the supplied
[configuration](boards/stm32f4discovery.json). Save the manufacturer's BSDL at
`tools/boards/bsdl/STM32F405_415_407_417_LQFP100.bsd`, or edit that configuration's
path to point to your copy. The BSDL is not bundled; the test fixtures are
synthetic and must not be used on hardware.

Before running the commands below, create the `tools/boards/bsdl` directory
and save the complete manufacturer's BSDL there as a plain-text `.bsd` file.
The expected layout is:

```text
tools/boards/
├── stm32f4discovery.json
└── bsdl/
    ├── ARM_JTAG_DP.bsd
    └── STM32F405_415_407_417_LQFP100.bsd
```

Alternatively, edit the `boundary` TAP's `bsdl` field to use your existing copy,
for example on Windows:

```json
"bsdl": "C:/Users/yourname/Documents/STM32F405_415_407_417_LQFP100.bsd"
```

Forward slashes work in Windows JSON paths; backslashes must be escaped as
`\\`. A `cannot load BSDL ... No such file or directory` error means that this
file is missing or the configured path is wrong. For the supplied JSON,
`./bsdl/...` resolves to `tools/boards/bsdl/...`, not a `bsdl` directory at the
repository root. No serial connection is opened when BSDL loading fails.

```sh
python tools/serial_jtag.py --port COM5 --config tools/boards/stm32f4discovery.json idcode
python tools/serial_jtag.py --port COM5 --config tools/boards/stm32f4discovery.json shell
```

The configuration includes USER/PA0 and LD4/PD12, LD3/PD13, LD5/PD14, LD6/PD15.
It describes the Boundary TAP with IR5 and Debug TAP with IR4. The board file
uses the bundled [ARM_JTAG_DP.bsd](boards/bsdl/ARM_JTAG_DP.bsd) for `debug`:

```json
{"name": "debug", "bsdl": "./bsdl/ARM_JTAG_DP.bsd"}
```

This is a minimal ArduJTAG description for the STM32F4 ARM JTAG-DP, not a
complete IEEE-conformant BSDL or a manufacturer's model. It declares IDCODE
(`1110`) and BYPASS (`1111`), with expected IDCODE `0x?BA00477` (version ignored).
It has no boundary register. DAP register and memory access is provided by the
separate `stm32f4-adiv5` protocol profile enabled in the supplied board JSON.
Other debug implementations may require different IDCODE and protocol metadata.
Only the STM32 Boundary TAP's manufacturer BSDL must be supplied separately.

Choose a TAP by name, either in a batch or inside the shell:

```text
jtag> targets
jtag> target boundary
jtag> idcode
jtag> pin-read USER
```

`target NAME` only selects the host-side target; it generates no clocks and
leaves active instructions unchanged. The next high-level scan puts all other
TAPs in BYPASS, releasing any of their active EXTEST/INTEST modes. Each described
TAP retains its own output vector. Returning to EXTEST on a previously bypassed
TAP performs PRELOAD again before activating that vector. Raw operations or a
communication failure invalidate the mode/vector knowledge for the whole chain.

`--config` cannot be combined with `--bsdl`, `--chain`, `--target` or the legacy
`--chain-config` option. For a single TAP, `--bsdl target.bsd` remains a shorthand
and names the TAP `tap0`. Legacy `--bsdl target.bsd --chain 5,4 --target 0` and
the old `--chain-config` format (`ir_lengths`, numeric `target`, `aliases`) are
also accepted, with generated names `tap0`, `tap1`, etc. New board files should
use the versioned `taps` format and `--config`.

The parser reads literal INSTRUCTION_LENGTH, INSTRUCTION_OPCODE,
REGISTER_ACCESS, IDCODE_REGISTER, BOUNDARY_LENGTH, BOUNDARY_REGISTER and
COMPLIANCE_PATTERNS attributes on the computer; BSDL is not sent to Arduino.
All connection/configuration options precede the first subcommand.

## High-level commands

| Command | Behavior |
| --- | --- |
| `targets` | Lists configured TAPs and marks the current target. |
| `target NAME` | Selects a target without changing hardware state. |
| `idcode` | Reads the target IDCODE, checks all non-X bits against BSDL, prints hex and decoded version, device number and vendor. |
| `bypass` | Selects BYPASS on the whole chain; normal device control resumes. |
| `sample` | Selects SAMPLE and returns the target's captured boundary bits, lowest cell index first. Explicitly leaves EXTEST/INTEST. |
| `preload` | Loads the retained output vector with PRELOAD, leaving active output-test mode. |
| `extest` | Preloads the retained vector before first selecting EXTEST, then scans it. |
| `intest` | Same preparation for INTEST; returns captured boundary bits. Requires BSDL support. |
| `pin-read NAME` | Captures one INPUT cell; prints 0 or 1. Uses SAMPLE normally, preserves an already active EXTEST/INTEST mode. |
| `pin-write NAME 0\|1` | Sets one OUTPUT3 value, enables its driver and enters EXTEST through PRELOAD when needed. |
| `pin-disable NAME` | Applies the BSDL disable value to that pin's control cell through EXTEST. |

The first high-level operation automatically resets TAP to Idle. IDCODE checking
is explicit: run `idcode` first to check that the selected BSDL matches the
connected target. An unsupported instruction, unknown pin or unsupported cell
layout produces an error instead of guessing an opcode or cell mapping.
STM32's supplied BSDL does not contain INTEST. SAMPLE and PRELOAD have the same
opcode; files naming that opcode `SAMPLE/PRELOAD` are also supported.

`idcode` preserves the raw hexadecimal value on the first line and decodes the
captured value below it, for example for the Debug TAP:

```text
0x4BA00477
  Version: 4 (0x4)
  Device number: 0xBA00
  Vendor: Arm (0x23B)
```

Version is bits 31:28, device number (part number) is bits 27:12, and vendor
is the full 11-bit manufacturer code in bits 11:1. Masked `X` bits in the BSDL
are still decoded from the actual capture. Vendor names currently cover
STMicroelectronics (`0x020`) and Arm (`0x23B`); other codes print `Unknown`
alongside their numeric value. The device number identifies the TAP's part,
not necessarily the complete microcontroller model. The same output is used
in batches and interactive sessions, after successful BSDL validation.

Vendor names are stored in [data/jtag_vendors.json](data/jtag_vendors.json):

```json
{
  "0x020": "STMicroelectronics",
  "0x23B": "Arm"
}
```

To extend the table, add a full 11-bit manufacturer code as `0x` followed by
three hexadecimal digits (range `0x000`–`0x7FF`) and a nonempty name. Duplicate
codes are rejected, including different letter cases. The file is loaded once
per process relative to the Python module, regardless of the working directory;
restart the CLI after editing it. If it is missing or invalid, the client prints
one warning to stderr and continues with `Unknown` and the numeric vendor code.
IDCODE validation and decoding do not depend on vendor names.

For example, in a BSDL-configured shell:

```text
jtag> idcode
jtag> pin-read USER
jtag> pin-write LD4 1
jtag> pin-write LD3 1
jtag> pin-read USER
jtag> pin-write LD4 0
jtag> pin-disable LD3
jtag> bypass
jtag> quit
```

The two LED writes retain each other's values. Reading USER while in EXTEST
captures input without replacing the intended output vector with the returned
scan data. The initial vector sets internal cells to their declared safe
values, chooses 0 for X data bits, and disables all described output drivers.
PRELOAD loads the complete vector before EXTEST/INTEST can activate it.

For the STM32 BSDL above, hold **target NRST LOW externally** throughout boundary
scan. The CLI displays COMPLIANCE_PATTERNS but cannot electrically enforce it;
adapter nTRST is a different signal. Device-specific mode/timing requirements
are not inferred from arbitrary VHDL packages or design warnings.

Pin writes control boundary cells, not firmware GPIO registers. Output state is
retained only within the current CLI session. Explicit `sample`, `preload`,
`idcode` or `bypass` changes the selected instruction; BYPASS restores normal
pin control rather than guaranteeing the last BSR levels. Closing the port or
leaving the shell does **not** issue BYPASS automatically. Finish with `bypass`
when you want to release boundary control.

Raw `reset`, `ir`, `dr` and `clocks` invalidate the host's mode/vector knowledge.
The next high-level operation resets and starts from the disabled-output vector.
Transmission failure invalidates state and terminates the session; there are no
automatic write retries. The protocol still checks the combined chain length:
512 IR/DR bits, including BYPASS padding, with the current adapter firmware.

## STM32F4DISCOVERY hardware checks

Flash the SerialTransfer adapter, install `tools/requirements.txt`, and prepare
the BSDL file as described above. Close Serial Monitor and other programs using
the serial port. Keep **target NRST LOW externally throughout these checks**
(for example, hold the target RESET button); nTRST is a separate signal.

Run from the repository root, replacing `COM5` with your adapter's port:

```sh
python tools/serial_jtag.py --port COM5 --config tools/boards/stm32f4discovery.json shell
```

Enter one command per line. Start by checking the selected target:

```text
targets
target boundary
idcode
```

The list contains `boundary` and `debug`. Boundary IDCODE should match
`0x06413041` under mask `0x0FFFFFFF`; the version nibble may differ. The client
checks this against the BSDL. To read the Debug TAP IDCODE in the same session:

```text
target debug
idcode
target boundary
```

Expect `0x4BA00477`, with possible variation in the version nibble. Debug TAP
supports IDCODE and BYPASS; boundary and pin commands fail before sending any
JTAG clocks because its description has no boundary register. Return to
`boundary` before the button and LED checks below.

Read USER without pressing it, then hold USER and repeat:

```text
pin-read USER
```

Expect `0` when released and `1` while pressed. Each command captures once;
there is no continuous polling.

Turn on the LEDs one at a time, observing the board between commands:

```text
pin-write LD4 1
pin-write LD3 1
pin-write LD5 1
pin-write LD6 1
pin-read USER
```

| Alias | STM32 pin | LED color |
| --- | --- | --- |
| LD4 | PD12 | Green |
| LD3 | PD13 | Orange |
| LD5 | PD14 | Red |
| LD6 | PD15 | Blue |

Previously enabled LEDs should stay on. The final input capture preserves the
intended output values. `pin-write` performs PRELOAD before entering EXTEST;
there is no need to issue `extest` manually.

Check output disabling and turn off the remaining LEDs:

```text
pin-disable LD4
pin-write LD3 0
pin-write LD5 0
pin-write LD6 0
bypass
quit
```

`pin-disable LD4` disables its output driver (high impedance), so the LED should
go out. Writing `0` drives the other outputs LOW. `bypass` releases boundary
control; `quit` alone does not do this. Release target RESET after the checks.

For a one-shot IDCODE and button check, use a batch with one port opening:

```sh
python tools/serial_jtag.py --port COM5 --config tools/boards/stm32f4discovery.json idcode pin-read USER bypass
```

These are expected hardware results, not a record of completed board testing.

## ARM DAP registers and memory

The supplied STM32F4DISCOVERY config enables a target-specific ADIv5 driver:

```json
{"name": "debug", "bsdl": "./bsdl/ARM_JTAG_DP.bsd", "protocol": "stm32f4-adiv5"}
```

Register addresses, banks, bit fields and access types live in
[stm32f4_adiv5.json](data/dap/stm32f4_adiv5.json), with sources and format
described in the [profile guide](data/dap/README.md). The profile is loaded and
checked before connecting. BSDL describes the TAP; the protocol profile adds
DP/AP semantics. A four-bit BSDL with matching IDCODE/BYPASS opcodes is required.

For DAP operations, **release target NRST** and disconnect other active debug
probes. Start the shell as above, then:

```text
target debug
dap-init
dp-read CTRL_STAT
ap-read 0 IDR
mem-read32 0x20000000
```

`dap-init` checks Debug IDCODE, clears JTAG sticky errors, requests debug/system
power, and waits for both acknowledgements. It does not halt or resume the CPU.

| Command | Behavior |
| --- | --- |
| `dap-init` | Initializes the selected DAP before register/memory operations. |
| `dp-read REGISTER` | Reads a named DP register: CTRL_STAT, SELECT or RDBUFF. |
| `dp-write REGISTER VALUE` | Writes a writable DP register, then checks status. |
| `ap-read AP REGISTER` | Selects AP and bank, reads CSW, TAR, DRW or IDR, checks status. |
| `ap-write AP REGISTER VALUE` | Writes a writable AP register and checks status. |
| `mem-read32 [--ap N] ADDRESS` | Reads one aligned 32-bit word; default AP is 0 from the profile. |
| `mem-write32 [--ap N] ADDRESS VALUE` | Writes one aligned 32-bit word. |

Addresses and values accept decimal or `0x` hexadecimal; AP indices are decimal
0..255. Register names are case-insensitive. Put command options before their
positional arguments when chaining commands. RDBUFF has JTAG-DP pipeline
semantics; it is not an arbitrary memory register.

For a write/read test, reserve a RAM word that running firmware and DMA do not
use. For example, **only if `0x20000000` is reserved for this test**, read and
record its original value, then run:

```text
mem-write32 0x20000000 0xA5A55A5A
mem-read32 0x20000000
```

Expect `0xA5A55A5A`; restore the recorded original value with `mem-write32` and
read it back. The CLI does not automatically reserve RAM, halt the CPU or
restore writes. Do not use an arbitrary firmware-owned address for this test.
Finish with `bypass` and `quit`. BYPASS does not undo memory writes or DAP power
requests.

Related commands can run in one connection:

```sh
python tools/serial_jtag.py --port COM5 --config tools/boards/stm32f4discovery.json target debug dap-init ap-read 0 IDR mem-read32 0x20000000 bypass
```

Raw reset/IR/DR/clocks and non-DAP high-level scans invalidate DAP initialization;
run `dap-init` again afterward. Merely choosing a target or querying info/speed
does not invalidate it. Explicit CTRL_STAT writes also require reinitialization.
WAIT polling is bounded to 100 attempts, as is power-up polling. Faults, invalid
ACKs and uncertain communication stop the session and invalidate chain state;
accepted writes are never automatically replayed. Memory commands check MEM-AP
class/type and verify the requested CSW configuration before accessing memory.

Only single aligned words on the STM32F4 ADIv5 JTAG-DP/AHB-AP combination are
supported initially. SWD, ADIv6, block transfers, flash programming and CPU
halt/resume commands are not implemented. Arduino firmware and the serial wire
protocol are unchanged. Validation uses a simulated DP/AP, not physical hardware.

## Parser scope

This first implementation supports literal decimal widths, quoted strings
joined with `&`, binary instruction opcodes, X-masked 32-bit IDCODEs and
complete numbered boundary-register records (including names like `DATA(0)`).
Descriptions for TAPs without boundary scan may omit both `BOUNDARY_LENGTH`
and `BOUNDARY_REGISTER`. Supplying only one, an explicit zero length, or a
`REGISTER_ACCESS` reference to an absent BOUNDARY register is rejected.
IDCODE and BYPASS remain available without boundary metadata.
It is not a general VHDL/BSDL compiler: named constant expressions, opcode
alternatives, custom register declarations and executable package semantics
are not evaluated. Operations require the selected instruction to map to
BOUNDARY, DEVICE_ID or BYPASS as appropriate.

Boundary operations support BC_1 INPUT/OUTPUT3/CONTROL/CONTROLR/INTERNAL cells
and BC_4 INPUT cells. OUTPUT3 must reference a control cell with a binary disable
value and a Z result. Unsupported cell kinds/functions and undefined internal
or control initial values are rejected. Shared controls can be initialized when
polarities agree, but individual writes/disables on shared drivers are rejected.
INPUT-only pins can be read; OUTPUT2, bidirectional cells and differential pin
semantics are not implemented. IDCODE/BYPASS remain usable when boundary cell
semantics are unsupported, provided the metadata itself parses successfully.

Validation is software-only; physical operation with the STM32 board and the
manufacturer's complete BSDL still needs verification.
