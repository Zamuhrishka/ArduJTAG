# ArduJTAG examples

ArduJTAG provides several examples to help you explore its API and check how the
library works on hardware, from individual JTAG clocks to named device operations.
The examples use **STM32F4-Discovery (STM32F407)** as the reference target board,
with an Arduino acting as the JTAG controller connected to its JTAG pins.
**BoundaryScanCommands** is a template with a teaching profile and requires
configuration before use, including on STM32F4-Discovery.

To use another target board, adapt the examples to its JTAG implementation.
In particular, check the JTAG instruction opcodes, IR and DR lengths, the number
and order of TAPs in the chain, and the target device index. Update device profiles
or hard-coded bit sequences as appropriate. For DAP and boundary-scan operations,
also review register values, memory addresses and scan vectors against the target
documentation or BSDL file. Check the wiring, signal voltage levels and clock
speed for your hardware; changing the pin assignments alone may not be enough.

The tables below describe what each example does and which API it demonstrates.

## Available examples

### Direct use of `Jtag`

These examples construct whole-chain IR/DR buffers or explicit clock sequences
and call `Jtag` directly.

| Example | What it does |
| --- | --- |
| [ReadId](ReadId/ReadId.ino) | Sends a hard-coded 9-bit IR sequence, reads 32 DR bits and prints the result as a hexadecimal integer. |
| [ReadIdSequence](ReadIdSequence/ReadIdSequence.ino) | Sends an explicit 54-clock TMS/TDI sequence after reset and prints all captured TDO bytes. |
| [EnableArmDap](EnableArmDap/EnableArmDap.ino) | Issues a fixed sequence of DP/AP register requests for enabling debug access, writing and reading data; prints the final raw response. |

### Using `JtagChain`

These examples use `JtagChain` and, where applicable, `JtagDeviceAccess` to
address a device and handle BYPASS padding automatically. `Jtag` supplies the
underlying transport.

| Example | What it does |
| --- | --- |
| [ReadIdChain](ReadIdChain/ReadIdChain.ino) | Reads the Debug TAP IDCODE once per second and prints it as a hexadecimal integer. |
| [TransferChain](TransferChain/TransferChain.ino) | Reads IDCODE through a named instruction and prints the returned bytes, least significant byte first. |
| [BypassChain](BypassChain/BypassChain.ino) | Selects BYPASS on the Debug TAP and the other TAP, once at startup. |
| [EnableArmDapChain](EnableArmDapChain/EnableArmDapChain.ino) | Expresses the same DP/AP request sequence through `JtagChain` and prints the final target response. |
| [BoundaryScanCommands](BoundaryScanCommands/BoundaryScanCommands.ino) | Runs a boundary-scan demonstration once, after its teaching profile and vectors have been configured. |

## Target configuration and output

### IDCODE and chain examples

`ReadIdChain`, `TransferChain` and `BypassChain` describe this STM32F407 chain in
physical order from TDI to TDO:

```text
TDI -> BoundaryScan TAP (5-bit IR, index 0) -> Debug TAP (4-bit IR, index 1) -> TDO
```

The Debug TAP uses the `ArmJtagDp` profile. Each operation puts the other TAP in
BYPASS. Change the device descriptions, target index and instruction profile if
your hardware has a different chain.

`ReadIdChain` returns only the target's 32-bit IDCODE. `TransferChain` exposes the
same data as four bytes. `ReadIdSequence` instead prints the entire 54-bit TDO
capture, including clocks used for TAP transitions; its output is not a decoded
32-bit IDCODE.

The low-level `ReadId` and `ReadIdSequence` examples contain literal instruction
or clock sequences. They do not infer the chain layout or validate instruction
meaning. Check their constants and scan lengths against your target before use;
`ReadIdChain` is the example that handles chain padding and extraction explicitly.

### ARM DAP register exchanges

The two `EnableArmDap` variants retain fixed request payloads for DP SELECT,
AP CSW, AP TAR and AP DRW operations. Review these payloads and the target memory
address before running them: these examples perform writes as well as reads.
They demonstrate register exchanges, not a complete ARM DAP driver: neither
implements ACK interpretation, WAIT retries or a general response-pipeline handler.

The direct version sends 9-bit whole-chain IR values and 36-bit DR buffers.
The chain version supplies 35 target bits and lets `JtagChain` add the one-bit
BYPASS register. Its final output excludes that BYPASS bit and still contains
raw ACK/data bits, rather than a decoded memory value.

Both repeat the sequence in `loop()`. The chain version preserves the original
reset points, but its final zero request reloads APACC because every `transfer()`
programs IR; the direct version's final call shifts only DR.

### Boundary-scan template

[ExampleBoundaryProfile.hpp](BoundaryScanCommands/ExampleBoundaryProfile.hpp)
contains **teaching opcodes and lengths, not a profile for a real chip**. Before
running `BoundaryScanCommands`:

1. Replace the profile with the supported instructions, IR length and boundary
   register length from your device's documentation/BSDL.
2. Replace `initialValues` and `nextValues` with complete BSR vectors, including
   output-enable/control cells, appropriate for the board connections.
3. Remove operations unsupported by the device and supply any required INTEST
   initialization or test clocks.
4. Check the chain capacity and set `DeviceConfigured = true`.

By default, the sketch prints a configuration message and generates no JTAG
clocks. Once configured, it shows capture/preload operations, PRELOAD before
EXTEST and INTEST, and IR-only HIGHZ/BYPASS selection. It prints captured bytes,
holds HIGHZ for one second, and finishes in BYPASS. `loop()` is empty.

Every device operation still puts other devices in BYPASS. This example does
not demonstrate coordinated boundary scanning of multiple active devices.

## Run an example with PlatformIO

The project environment is configured for an **Arduino Nano ATmega328P with the
new bootloader** as the JTAG controller. The external target is a separate board.
All sketches use Serial at **115200 baud** and these controller pins:

| Signal | Arduino pin |
| --- | --- |
| TCK | D2 |
| TMS | D3 |
| TDI | D4 |
| TDO | D5 |
| nTRST | D6 |

Use a common ground and electrical levels compatible with both boards.
`nTRST` is the JTAG reset signal, not the target's system `NRST`.

PlatformIO builds `src/main.cpp`; merely opening an `.ino` does not select it.
Replace the contents of `src/main.cpp` with one example include:

```cpp
#include <Arduino.h>
#include "../examples/ReadIdChain/ReadIdChain.ino"
```

Change the path to select another sketch. Include only one example at a time,
since each defines `setup()` and `loop()`. Keep helper headers next to their
sketches; the relative include above also works for `BoundaryScanCommands`.

From the repository root:

```sh
pio run -e nanoatmega328new
pio run -e nanoatmega328new -t upload
pio device monitor -b 115200
```

To specify an upload port, append `--upload-port COM5` on Windows or the actual
serial device path on your system. Close the serial monitor before uploading.
See the main [README](../README.md#development-setup) for PlatformIO setup.

GPIO tracing is off by default. With `-DARDUJTAG_DEBUG` in the firmware build
flags, additional three-digit `TMS/TDI/TDO` lines appear for every clock and slow
the transfer. They are diagnostic traces, not the decoded example output.
