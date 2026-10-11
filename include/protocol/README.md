# SerialTransfer JTAG adapter

This optional layer exposes `Jtag` over a binary SerialTransfer connection.
`JtagProtocol.hpp` decodes commands independently of the transport;
`SerialTransferJtag.hpp` connects it to [SerialTransfer 3.1.5](https://github.com/PowerBroker2/SerialTransfer).
JTAG headers do not depend on SerialTransfer. The Nano PlatformIO environment
provides the dependency so any example, including this adapter, can be selected
manually in `src/main.cpp`. The `nano_serial` preset inherits that dependency.

## Build and connect

Use an Arduino Nano as the USB/Serial adapter, with the normal JTAG wiring:
D2 TCK, D3 TMS, D4 TDI, D5 TDO, D6 nTRST, plus common GND and compatible voltage
levels. Target NRST is separate and is not controlled by this protocol.
On the classic Nano, USB communication passes through its USB-to-UART bridge.

```sh
pio run -e nano_serial
pio run -e nano_serial -t upload
python -m venv .venv
# Activate .venv using the command appropriate for your shell.
python -m pip install -r tools/requirements.txt
python tools/serial_jtag.py --port /dev/ttyUSB0 info
```

Use `COM5` or the actual port on Windows. Close other programs using that port.
The `nano_serial` environment selects
[SerialTransferJtag.ino](../../examples/serial/SerialTransferJtag/SerialTransferJtag.ino)
without changing the normally selected sketch in `src/main.cpp`.
Serial runs at 115200 baud; TCK initially requests 100 kHz. GPIO overhead can
make the actual TCK frequency lower. No JTAG clocks occur at startup.

The Python CLI uses Click for options, help and error reporting. Run
`python tools/serial_jtag.py --help` for usage; `--timeout` accepts a finite,
positive number of seconds (default 3). Pass one or more Click subcommands without quoting the command and its
arguments together; they run in order within a single serial session. Put
`--port` and `--timeout` before the first subcommand. Without commands, the CLI prints help without opening the port.

The client waits two seconds for the Nano's usual reset on port opening, then
queries INFO. Keep related commands in one session: reopening the port may
reset the adapter and lose TAP state tracking. For example, read the STM32F407
Debug TAP IDCODE (4-bit Debug IR, 5-bit Boundary BYPASS, 33-bit DR):

```sh
python tools/serial_jtag.py --port /dev/ttyUSB0 reset ir 011111111 dr 000000000000000000000000000000000
```

The DR result is printed in transmission order: the first 32 bits are the Debug
IDCODE, least significant bit first; the last bit belongs to Boundary BYPASS.
The CLI also accepts `info`, `speed KHZ` and `clocks TMS_BITS TDI_BITS`
as subcommands. Use `python tools/serial_jtag.py --port COM5 ir --help`
for command-specific help. Help and invalid command syntax do not open the
serial port. Bit strings use `fromBits()` order (leftmost bit first).
BSDL-based instructions and pin commands are available through `--config`
(full chain) or `--bsdl` (single TAP); see
the [Python client guide](../../tools/README.md). With `protocol: stm32f4-adiv5`
on the Debug TAP, the client also provides DP/AP registers and single-word memory
access; see [DAP commands](../../tools/README.md#arm-dap-registers-and-memory).
These use the existing IR/DR wire commands and require no firmware update.

## Interactive mode (explicit opt-in)

Start it with the `shell` subcommand:

```sh
python tools/serial_jtag.py --port COM5 shell
```

The serial port stays open between commands. Enter one command per line:

```text
jtag> reset
jtag> ir 011111111
jtag> dr 000000000000000000000000000000000
jtag> quit
```

Use `help` or `ir --help` for assistance. `quit`, `exit` or EOF closes the
session; Ctrl+C exits with status 130. Invalid arguments and adapter error
responses are displayed so the command can be corrected. Communication errors
or timeouts stop the session without retrying uncertain operations.

You can prepare the TAP before entering the shell using the same connection:

```sh
python tools/serial_jtag.py --port COM5 reset ir 011111111 shell
```

`shell` must be the last subcommand. Without a subcommand the CLI still shows
help and does not open the port; interactive mode never starts implicitly.

## BSDL operations

Use `--config board.json` with a named `taps` array for the full chain, or
`--bsdl target.bsd` for a single TAP. `targets` lists TAPs and `target NAME`
selects one without reopening the port.

The Python client can translate `idcode`, `bypass`, `sample`, `preload`, `extest`,
`intest`, `pin-read`, `pin-write` and `pin-disable` into these same wire commands.
The adapter firmware and payload format are unchanged. Pass a target BSDL and
chain configuration; see the [high-level usage guide](../../tools/README.md).

The supplied STM32F4DISCOVERY JSON references a manufacturer's BSDL file that
is not bundled. Save it at
`tools/boards/bsdl/STM32F405_415_407_417_LQFP100.bsd`, or change the JSON's
`bsdl` path. Relative BSDL paths are resolved from the JSON directory.
Missing files fail before the serial port is opened.
For button and LED checks, hold target NRST LOW externally and follow the
[hardware walkthrough](../../tools/README.md#stm32f4discovery-hardware-checks).

## Protocol version 1

SerialTransfer supplies framing, byte stuffing and CRC-8. Do not wrap the
payload in a second COBS frame or send plain text on the same port.
SerialTransfer debug output is disabled and `ARDUJTAG_DEBUG` is prohibited.

Every integer is explicitly little-endian, never a native C++ struct layout.
SerialTransfer's packet ID is echoed, but request correlation uses the 16-bit
sequence number inside the CRC-protected payload.

| Offset | Request | Response |
| --- | --- | --- |
| 0 | Version = 1 | Supported version = 1 |
| 1 | Command | Echoed command |
| 2..3 | Sequence number (uint16) | Echoed sequence number |
| 4 | Start of parameters | Status byte |
| 5 | Parameters continued | Start of result |

| Command | ID | Parameters | Successful result |
| --- | --- | --- | --- |
| INFO | 1 | None | uint16 max IR bits, uint16 max DR bits, uint16 max clock count, uint16 max requested kHz, uint8 idle-known flag |
| SET_SPEED | 2 | uint32 kHz (1..500) | Empty |
| RESET | 3 | None | Empty |
| IR_WRITE | 4 | uint16 bit count, packed TDI | Empty |
| DR_TRANSFER | 5 | uint16 bit count, packed TDI | uint16 bit count, packed TDO |
| CLOCK_SEQUENCE | 6 | uint16 count, packed TMS, packed TDI | uint16 count, packed TDO |

Limits are 512 bits per IR/DR and 256 clocks per raw sequence. A packed array
occupies `(count + 7) / 8` bytes. Bytes are transmitted in array order, LSB first
within each byte. Unused high bits in the last byte must be zero. Counts must
be nonzero. Each command must have exactly its specified payload length.
Maximum application request/response sizes are 70/71 bytes.

RESET generates five TMS-high clocks, then one TMS-low clock to enter Idle;
it does not pulse physical nTRST. IR_WRITE and DR_TRANSFER operate on the
**entire chain**, start/end in Idle, and require RESET after startup.
CLOCK_SEQUENCE leaves TAP exactly where the requested sequence puts it. The
adapter conservatively invalidates its Idle knowledge afterward: RESET is
required before later IR/DR commands. Raw sequences may be continued with more
CLOCK_SEQUENCE packets, but splitting a scan into separate IR/DR commands is
not equivalent to one long scan.

| Status | Meaning |
| --- | --- |
| 0 | Success |
| 1 | Unsupported protocol version |
| 2 | Unknown command |
| 3 | Incorrect payload length |
| 4 | Invalid count, speed or nonzero padding bits |
| 5 | RESET required before IR/DR |
| 6 | JTAG operation failed; RESET required |

Errors have no result payload. Validation failures generate no JTAG clocks.
Requests shorter than the four-byte header and invalid SerialTransfer frames
are discarded without a response. CRC failure is not an application error
response because the command and sequence cannot be trusted.

Only one request may be outstanding. The client must wait for the response
before sending another; the adapter does not queue commands while clocking.
Sequence numbers identify responses, **not duplicate suppression**: repeating
a request executes it again. On a timeout, execution is uncertain, so the CLI
never retries and closes the session on a communication failure. Reconnection
and any TAP reset must be intentional. The protocol has no persistent session
ID and cannot guarantee exactly-once execution across adapter resets.

## Validation and memory

Run the payload/clock tests with:

```sh
pio test -e native -e native_sanitized -f protocol/test_protocol
python -m unittest discover -s test/python -v
```

The C++ suite checks validation without clocks, reset/Idle behavior, complete
407- and 512-bit DR transfers, 256 raw clocks, response padding and byte order.
Python tests use the real pySerialTransfer encoder/parser to check framing,
CRC failures, bit packing, errors and timeout behavior without hardware.
Click CliRunner tests additionally cover argument validation, batch sessions,
help without opening a port, interrupts and port cleanup.

The Nano build uses about 891 bytes of static RAM, plus SerialTransfer's
256-byte heap-allocated CRC table and runtime stack. The handler itself uses
fixed-size buffers and does not allocate dynamically. Build size is not a
measurement of peak stack usage. Physical USB/JTAG operation still needs
verification on the board.
