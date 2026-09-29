# ArduJTAG Tests

The tests check buffer handling, JTAG bit sequences, chain operations and GPIO sampling order on the host computer using PlatformIO Test Runner and Unity.
No Arduino or target board is required.

## Directory structure

Suites are grouped by library component. Shared Arduino simulation headers stay
in `support/`, included through the host environment's `-Itest/support` flag.

```text
test/
├── README.md
├── support/
│   ├── Arduino.h
│   └── SimulatedGpio.hpp
├── buffers/
│   └── test_buffers/
├── core/
│   ├── test_ir/
│   ├── test_dr/
│   └── test_clock_cycles/
├── chain/
│   └── test_chain/
├── boundary/
│   └── test_boundary/
├── profiles/
│   └── test_stm32_profile/
└── backends/
    └── gpio/
        └── test_gpio/
```

## Available test modules

| Module | What it checks |
| --- | --- |
| [test_buffers](buffers/test_buffers/test_main.cpp) | `BitBuffer` construction, bit/byte ordering, partial-byte masking and invalid input handling. |
| [test_ir](core/test_ir/test_main.cpp) | `Jtag::ir()` instruction bits, TAP transitions, supported lengths and rejection of empty instructions. |
| [test_dr](core/test_dr/test_main.cpp) | `Jtag::dr()` transmission, response capture, output bounds and buffer validation. |
| [test_clock_cycles](core/test_clock_cycles/test_main.cpp) | Explicit TMS/TDI sequences, TDO capture, sequence limits and TAP reset clocks. |
| [test_chain](chain/test_chain/test_main.cpp) | Chain ordering, BYPASS padding, profiles, device access, IDCODE and standard device operations. |
| [test_gpio](backends/gpio/test_gpio/test_main.cpp) | Sampling TDO before the falling TCK edge and checking half-period delays across the 16-bit boundary, 32-bit timer wraparound, and long pauses in `JtagGpio::clock()`. |
| [test_boundary](boundary/test_boundary/test_main.cpp) | Generic boundary vectors, both disable polarities, internal initial values, input/output-only pins and validation without partial writes. |
| [test_stm32_profile](profiles/test_stm32_profile/test_main.cpp) | STM32 BSDL opcodes and widths, IDCODE masks, cell mappings, initial vectors, pin helpers and 406-bit scans with Debug TAP BYPASS. |

The generic boundary module has five tests using an independent synthetic layout.
The STM32 profile module has seven tests using `BoundaryScan` with the real layout metadata. It checks supported instruction aliases
and rejects unlisted opcodes, verifies revision-independent IDCODE matching,
checks unique cell/pin mappings and the disabled-driver starting vector, and
exercises pin helper validation. Chain tests exchange the complete BSR with
407-bit capacity, reject shorter/longer vectors and insufficient chain capacity
before clocks, and read the BoundaryScan TAP IDCODE through device access.

## Running the tests

Install PlatformIO Core and a local C++ compiler (GCC or Clang), then run from the repository root:

```sh
pio test -e native
```

PlatformIO installs the Native platform and Unity on the first run. Each test is reported separately. To run one module, select its path relative to `test/`:

```sh
pio test -e native -f chain/test_chain
```

For AddressSanitizer and UndefinedBehaviorSanitizer on a supported host compiler:

```sh
ASAN_OPTIONS=detect_leaks=0 pio test -e native_sanitized
```

Leak detection is disabled in this command for ptrace-based environments. Address and undefined behavior checks remain enabled.
The `-f` filter can also be used with `native_sanitized`.

Firmware builds still default to `nanoatmega328new`, which excludes these host-only modules.
See [platformio.ini](../platformio.ini) for the environment configuration.

## Test setup and limits

The tests use the real header-only implementations, including `GpioPin` and
`JtagGpio`, with a minimal [support/Arduino.h](support/Arduino.h). Protocol suites
simulate Arduino pin access and time using [support/SimulatedGpio.hpp](support/SimulatedGpio.hpp);
each suite supplies the TDO response and records sampled clocks. Traces and
counters are reset before each test. `test_gpio` uses its own Arduino simulation
to check pin levels, clock edges and the ordering of TDO reads.


These tests check software behavior and operation ordering. They do not measure physical GPIO timing, signal integrity
or communication with an actual JTAG chain. They also do not validate real target opcodes, memory transactions or
boundary-scan effects on board pins; those require hardware checks.
