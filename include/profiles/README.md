# Instruction and boundary-register profiles

| Profile | Target |
| --- | --- |
| [ArmJtagDp](ArmJtagDp.hpp) | ARM Debug TAP with a four-bit instruction register. |
| [Stm32F405_415_407_417Lqfp100](Stm32F405_415_407_417Lqfp100.hpp) | BoundaryScan TAP described by ST's `STM32F405_415_407_417_LQFP100.bsd`, V1.1, 2014-05-30. |

The STM32 profile is derived from the BSDL supplied for this project. It covers
these four device families in LQFP100, not every STM32F4 or package. The ARM
Debug TAP is a separate device in the chain.

## STM32 instructions and metadata

| Instruction | IR value (5 bits) | Target DR length |
| --- | --- | --- |
| `Extest` | `0x00` | 406 |
| `Idcode` | `0x01` | 32 |
| `Sample`, `Preload`, `SamplePreload` | `0x02` | 406 |
| `Bypass` | `0x1F` | 1 |

The profile does not declare INTEST or HIGHZ: neither appears in this BSDL.
Instruction encoding sends the least significant bit first; IDCODE (`00001`
in BSDL) is transmitted as `10000` in `fromBits()` notation.

`IdcodeValue` is `0x06413041`, with mask `0x0FFFFFFF`. `matchesIdcode(value)`
ignores the four revision bits marked X in the BSDL. `readIdcode()` still only
reads a value; call `matchesIdcode()` explicitly to check it.

`InstructionCaptureValue = 0x01` and `InstructionCaptureMask = 0x03` describe
Capture-IR (`XXX01`). They are metadata; the current `ir()` API discards TDO.
`MaxTckHz = 10000000` is the target's BSDL rating, not a change to the controller's
speed limit. `RequiresNrstLow = true` records the BSDL compliance requirement.

**Hold NRST low for the boundary-scan behavior described by this BSDL.** NRST
is distinct from JTRST, and neither the profile nor `Jtag::reset()` drives NRST.
The application must arrange this externally. Board configuration can affect
boundary-scan behavior, as noted in the source BSDL.

## LQFP100 boundary cells

[Stm32F405_415_407_417Lqfp100Boundary](Stm32F405_415_407_417Lqfp100Boundary.hpp)
keeps the package-specific cell mapping separate from the instruction profile.
It describes 77 bidirectional ports and the input-only BOOT0 port. Internal
cells remain present in the complete 406-bit vector.

- `cells(Pin)` returns input, output and control indices plus the physical
  LQFP100 pin number. These are not Discovery header numbers. `NoCell` marks
  missing cells; an unknown pin returns an invalid mapping.
- `initialValues()` returns a 406-bit buffer with every output control set to
  1 (driver disabled), internal cells set to 0, and BSDL X values chosen as 0.
  This starting vector must be considered together with the board circuit.
- `setOutput(values, pin, level)` sets the output data and clears the control
  bit to enable the driver.
- `disableOutput(values, pin)` sets the control bit to 1 without changing data.
- `readInput(captured, pin, level)` reads the pin's input cell.

The helpers require an active buffer length of exactly 406 bits. Invalid pins,
wrong lengths and output requests for BOOT0 return false without modifying the
buffer or the output argument. They operate on local buffers and generate no
JTAG clocks. Mappings are defined as a switch; no BSDL parser or per-device cell table is stored.

For PD12, input/output/control indices are 166/167/168, and the package pin is 59.
Enum names use `Pa0`, `Pb0`, etc. to avoid AVR pin macros such as `PA0`.
Cell numbers are buffer bit indices; bit zero is transmitted first. A chain with
this BoundaryScan TAP and the Debug TAP requires capacity for 407 DR bits,
including the latter's BYPASS bit.

```cpp
#include <chain/JtagChain.hpp>
#include <profiles/ArmJtagDp.hpp>
#include <profiles/Stm32F405_415_407_417Lqfp100Boundary.hpp>

using Profile = Stm32F405_415_407_417Lqfp100;
using Boundary = Stm32F405_415_407_417Lqfp100Boundary;

Jtag jtag(3, 4, 5, 2, 6); // TMS, TDI, TDO, TCK, JTRST; NRST is separate.
JtagChain<2, 407> chain(jtag);

void setup()
{
  // Arrange NRST low externally before running this example.
  if (!chain.add(JtagDevice::fromProfile<Profile>()) ||
      !chain.add(JtagDevice::fromProfile<ArmJtagDp>())) return;
  jtag.reset();
  auto boundary = chain.device<Profile>(0);
  uint32_t idcode = 0;
  if (boundary.readIdcode(idcode) != JTAG::ERROR::NO ||
      !Profile::matchesIdcode(idcode)) return;

  auto values = Boundary::initialValues();
  Boundary::Values captured;
  if (boundary.sample(values, captured) != JTAG::ERROR::NO) return;
  bool pd12 = false;
  if (!Boundary::readInput(captured, Boundary::Pin::Pd12, pd12)) return;
  // pd12 now contains the captured input level.
}

void loop() {}
```

This example uses SAMPLE and does not enter EXTEST. For output testing, review
all connected signals, preload suitable initial values before selecting EXTEST,
and remember that captures precede application of the newly shifted values.
The supplied profile has been checked with host simulations, not physical hardware.
