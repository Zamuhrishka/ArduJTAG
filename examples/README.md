# ArduJTAG Examples

ArduJTAG provides several examples to help you explore its API and check how the library works on hardware, from individual
JTAG clocks to named device operations. The examples use **STM32F4-Discovery (STM32F407)** as the reference target board,
with an Arduino acting as the JTAG controller connected to its JTAG pins.

To use another target board, adapt the examples to its JTAG implementation. In particular, check the JTAG instruction opcodes,
IR and DR lengths, the number and order of TAPs in the chain, and the target device index. Update device profiles
or hard-coded bit sequences as appropriate. For DAP and boundary-scan operations, also review register values,
memory addresses, and scan vectors against the target documentation or BSDL file.

The tables below describe what each example does.

## Available examples

Examples are grouped by API: `direct/` uses `Jtag` directly, while `chain/` uses
`JtagChain` and device access objects.

### Direct use of `Jtag`

These examples construct whole-chain IR/DR buffers or explicit clock sequences and call `Jtag` directly.

| Example | What it does |
| --- | --- |
| [ReadId](direct/ReadId/ReadId.ino) | Reads and labels both STM32F407 TAP IDCODEs using explicit 9-bit IR and 33-bit DR sequences, accounting for the other TAP's BYPASS bit. |
| [ReadIdSequence](direct/ReadIdSequence/ReadIdSequence.ino) | Reads and labels both STM32F407 TAP IDCODEs using two explicit 54-clock TMS/TDI sequences, each preceded by reset. Extracts IDs after TAP transition clocks and the appropriate BYPASS offset. |
| [EnableArmDap](direct/EnableArmDap/EnableArmDap.ino) | Issues a fixed sequence of DP/AP register requests for enabling debug access, writing and reading data. |
| [ExtestLeds](direct/ExtestLeds/ExtestLeds.ino) | Performs one LED chase through PRELOAD/EXTEST using only `Jtag` and `BitBuffer`, with explicit IR codes and a full 407-bit DR vector. |

### Using `JtagChain`

These examples use `JtagChain` and, where applicable, `JtagDeviceAccess` to address a device and handle BYPASS padding automatically.
`Jtag` supplies the underlying transport.

| Example | What it does |
| --- | --- |
| [ReadIdChain](chain/ReadIdChain/ReadIdChain.ino) | Reads the Debug TAP IDCODE once per second and prints it as a hexadecimal integer. |
| [TransferChain](chain/TransferChain/TransferChain.ino) | Reads IDCODE through a named instruction and prints the returned bytes, least significant byte first. |
| [BypassChain](chain/BypassChain/BypassChain.ino) | Selects BYPASS on the Debug TAP and the other TAP, once at startup. |
| [EnableArmDapChain](chain/EnableArmDapChain/EnableArmDapChain.ino) | Expresses the same DP/AP request sequence through `JtagChain` and prints the final target response. |
| [BoundaryScanCommands](chain/BoundaryScanCommands/BoundaryScanCommands.ino) | Runs a boundary-scan demonstration once, after its teaching profile and vectors have been configured. |
| [BoundaryScanPins](chain/BoundaryScanPins/BoundaryScanPins.ino) | Uses `BoundaryScan<Layout>` to prepare STM32F4 vectors by pin name and read PD12 with SAMPLE. Optional EXTEST drives PD12 HIGH, captures its level, then disables the driver. |


> ** WARNING: Boundary-scan template**
>
> [ExampleBoundaryProfile.hpp](chain/BoundaryScanCommands/ExampleBoundaryProfile.hpp) contains **teaching opcodes and lengths,
not a profile for a real chip**. Before running `BoundaryScanCommands`:
>
> 1. Replace the profile with the supported instructions, IR length, and boundary register length from your device's documentation/BSDL.
> 2. Replace `initialValues` and `nextValues` with complete BSR vectors, including output-enable/control cells, appropriate for the board connections.
> 3. Remove operations unsupported by the device and supply any required INTEST initialization or test clocks.
> 4. Check the chain capacity and set `DeviceConfigured = true`.


## ExtestLeds configuration

[ExtestLeds](direct/ExtestLeds/ExtestLeds.ino) runs once at startup on
STM32F4DISCOVERY with an STM32F407 in LQFP100. It uses no chain or profile classes.
The sketch checks the Boundary TAP IDCODE, preloads all LED outputs LOW,
selects EXTEST and lights LD4 (green/PD12), LD3 (orange/PD13), LD5 (red/PD14),
then LD6 (blue/PD15), for 500 ms each. These LEDs are active high; see the
[ST board schematic, sheet 6](https://www.st.com/resource/en/schematic_pack/mb997-f407vgt6-b02_schematic.pdf).
It then turns them off, disables their drivers and selects BYPASS on both TAPs.

Hold target **NRST LOW externally throughout the test**, as required by the
supplied ST BSDL. NRST is separate from JTRST on controller D6. Check external
connections: EXTEST controls the entire BSR, and the example disables every
non-LED output driver. BYPASS returns pins to normal target control; it does not
keep the final BSR values driving the pins. No STM32 application firmware is needed.

The 9-bit IR selects Debug BYPASS plus Boundary PRELOAD (`0x02`) or EXTEST
(`0x00`). Each 407-bit DR vector starts with the Debug BYPASS bit, followed by
406 boundary cells. BSDL cell N therefore appears at buffer index N+1. The
control-cell list and LED indices are explicit in the sketch. `loop()` is empty;
reset the Arduino to repeat. Hardware execution has not yet been verified.

## BoundaryScanPins configuration

This example uses the real STM32F405/415/407/417 LQFP100 profile and runs once.
Hold the target's **NRST low externally** before running it, as required by the
BSDL. D6 controls JTRST, a separate signal. The sketch checks the Boundary TAP's
IDCODE, prints the PD12 cell indices (166/167/168), samples its input and finishes
in BYPASS. It uses `JtagChain<2, 407>` for the BSR plus the Debug TAP BYPASS bit.

`EnableOutputTest` is false by default. Set it to true after checking the board
connections and the complete starting vector to demonstrate `setOutput()` and
`disableOutput()`. The optional sequence preloads the disabled-driver vector,
drives PD12 HIGH in EXTEST for one second, captures its level with a second scan,
and applies the disabled-driver vector before leaving EXTEST. Other output
drivers are disabled during EXTEST; this affects more than PD12. BYPASS restores
normal target pin control, so the final vector does not force pin levels afterward.

`BoundaryScan` helpers only edit or decode buffers; the `tap` methods send them.
See the [profile guide](../include/profiles/README.md) for the layout and API.

## Run an example with PlatformIO

The project environment is configured for an **Arduino Nano ATmega328P with the new bootloader** as the JTAG controller. The external target is a separate board.
All sketches use Serial at **115200 baud** and these controller pins:

| Signal | Arduino pin |
| --- | --- |
| TCK | D2 |
| TMS | D3 |
| TDI | D4 |
| TDO | D5 |
| nTRST | D6 |

Use a common ground and electrical levels compatible with both boards. `nTRST` is the JTAG reset signal, not the target's system `NRST`.

PlatformIO builds `src/main.cpp`; merely opening an `.ino` does not select it. Replace the contents of `src/main.cpp` with the following example include:

```cpp
#include <Arduino.h>
#include "../examples/chain/ReadIdChain/ReadIdChain.ino"
```

Change the path to select another sketch. Include only one example at a time, since each defines `setup()` and `loop()`. Keep helper headers next to their
sketches; the relative include above also works for `BoundaryScanCommands`.

From the repository root:

```sh
pio run -e nanoatmega328new
pio run -e nanoatmega328new -t upload
pio device monitor -b 115200
```

To specify an upload port, append `--upload-port COM5` on Windows or the actual serial device path on your system. Close the serial monitor before uploading.
