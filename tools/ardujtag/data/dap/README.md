# DAP register profiles

`stm32f4_adiv5.json` describes the initial supported target: STM32F405/415/407/417
ADIv5 JTAG-DP with an AHB MEM-AP. It is selected by `protocol: stm32f4-adiv5`
in the chain JSON. This is not a universal ADIv5/ADIv6 or SWD profile.

The sources and editions are recorded in the JSON:

- [Arm IHI 0031F, ADIv5.2](https://developer.arm.com/documentation/ihi0031/f/):
  JTAG-DP transactions, DP register descriptions and MEM-AP register descriptions.
- [ST RM0090 Rev 21](https://www.st.com/resource/en/reference_manual/rm0090-stm32f4xx-reference-manual-stmicroelectronics.pdf):
  sections 38.7 (JTAG-DP) and 38.9 (AHB-AP).
- Implementation cross-checks: OpenOCD's
  [ADI register definitions](https://openocd.org/doc/doxygen/html/arm__adi__v5_8h_source.html)
  and [JTAG transport](https://openocd.org/doc/doxygen/html/adi__v5__jtag_8c_source.html).

## Format

All numeric values are JSON integers (decimal). Register names are uppercase.
The profile describes only the registers and fields currently used by the client;
omitted fields are not implied to be reserved or absent in the hardware.

| Property | Meaning |
| --- | --- |
| `version` | Profile format version, currently 1. |
| `sources` | Documents, editions, sections and links. |
| `ir_length`, `instructions` | IR width and numeric instruction codes, encoded LSB first by the driver. |
| `ack` | JTAG-DP WAIT and combined OK/FAULT values; these are not SWD ACKs. |
| `dp`, `ap` | Separate register namespaces. |
| Register `address` | A[3:2] address expressed as byte offset: 0, 4, 8 or 12. |
| Register `bank` | AP bank (0..15); DP registers are unbanked in this initial profile. |
| Register `access` | RO, WO or RW, checked before scans. |
| `fields` | Named fields with `lsb`, `width` and access properties (RO, RW, W1C). |
| `memory.default_ap` | Default AP index for memory commands. |
| `memory.idr_fields` | Expected MEM-AP class and type, checked before memory access. |
| `memory.csw_fields` | STM32F4 AHB-AP settings for 32-bit accesses with increment disabled. |

For example, AP IDR has bank 15 and address 12: its full AP register offset is
`0xFC`. SELECT.APSEL and SELECT.APBANKSEL choose the AP and bank before APACC.
The selected CSW fields compose `0x23000002`; this value is target-specific.
JTAG CTRL_STAT sticky bits are W1C; SWD ABORT clear-bit semantics do not apply.

The loader validates required records, address and bit ranges, overlaps and
access types before opening the serial port. A missing or malformed profile
is fatal: register addresses must not fall back to guesses. Restart the CLI
after editing the file. Extending the JSON does not implement a new transport;
new architectures require corresponding driver logic and tests.

## Code versus metadata

`tools/ardujtag/dap/jtag_dp.py` implements pipelined response handling, bounded WAIT polling,
power-up, sticky-error checks and single-word memory transactions. Each AP
access explicitly selects the AP/bank. Memory accesses validate AP IDR, write
and verify CSW, then set TAR and access DRW. There is no SELECT/CSW cache.
Field access metadata describes behavior; the driver explicitly implements
power-up and W1C recovery. There is no generic field-write command.

ABORT's opcode is documented for completeness; the current CLI does not issue
it automatically or expose `dap-abort`. Faults and timeouts terminate the
session. `dap-init` on a new session clears sticky status if the DP still
responds; a stuck transaction may require target reset/power-cycle.

The driver requests one operation at a time and drains its result using a DP
RDBUFF scan. Accepted writes are not retried. WAIT repeats only the scan that
was not accepted. Existing IR/DR operations provide TAP transitions; no raw
`clocks` call is inserted (the adapter invalidates Idle tracking for raw clocks).
Hardware timings and memory accesses still need physical STM32F4 verification.
