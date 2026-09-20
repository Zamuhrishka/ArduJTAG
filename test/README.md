# ArduJTAG Tests

The tests check buffer handling, JTAG bit sequences, chain operations and GPIO sampling order on the host computer using PlatformIO Test Runner and Unity.
No Arduino or target board is required.

## Available test modules

| Module | What it checks |
| --- | --- |
| [test_buffers](test_buffers/test_main.cpp) | `BitBuffer` construction, bit/byte ordering, partial-byte masking and invalid input handling. |
| [test_ir](test_ir/test_main.cpp) | `Jtag::ir()` instruction bits, TAP transitions, supported lengths and rejection of empty instructions. |
| [test_dr](test_dr/test_main.cpp) | `Jtag::dr()` transmission, response capture, output bounds and buffer validation. |
| [test_clock_cycles](test_clock_cycles/test_main.cpp) | Explicit TMS/TDI sequences, TDO capture, sequence limits and TAP reset clocks. |
| [test_chain](test_chain/test_main.cpp) | Chain ordering, BYPASS padding, profiles, device access, IDCODE and standard device operations. |
| [test_gpio](test_gpio/test_main.cpp) | Sampling TDO before the falling TCK edge in the real `JtagGpio::clock()` implementation. |

## Running the tests

Install PlatformIO Core and a local C++ compiler (GCC or Clang), then run from the repository root:

```sh
pio test -e native
```

PlatformIO installs the Native platform and Unity on the first run. Each test is reported separately. To run one module, select its directory name:

```sh
pio test -e native -f test_chain
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

The protocol tests use the real header implementations and `JtagCommon.cpp`, with simulated `JtagGpio`/`GpioPin`
implementations and a minimal [support/Arduino.h](support/Arduino.h). Simulated traces and counters are reset before
each test. `test_gpio` instead exercises the real GPIO clock routine, while keeping pin access and time simulated.

These tests check software behavior and operation ordering. They do not measure physical GPIO timing, signal integrity
or communication with an actual JTAG chain. They also do not validate real target opcodes, memory transactions or
boundary-scan effects on board pins; those require hardware checks.
