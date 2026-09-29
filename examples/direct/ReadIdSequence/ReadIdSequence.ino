/**
 * @file ReadIdSequence.ino
 * @brief Read both STM32F407 TAP IDCODEs with explicit TMS/TDI sequences.
 * Physical order: TDI -> BoundaryScan TAP (5-bit IR) -> Debug TAP (4-bit IR) -> TDO.
 * fromBits() strings are written in transmission order, first bit on the left.
 */
#include <Arduino.h>
#include <core/Jtag.hpp>

#define TCK 2
#define TMS 3
#define TDI 4
#define TDO 5
#define RST 6 // JTRST, not the STM32 system reset NRST.

Jtag jtag(TMS, TDI, TDO, TCK, RST);

void setup()
{
  Serial.begin(115200);
}

void loop()
{
  // Each scan follows reset() and contains 54 clocks:
  // 5 enter Shift-IR, 9 shift IR, 2 return to Idle, 3 enter Shift-DR,
  // 33 shift DR (32 IDCODE + 1 BYPASS), 2 return to Idle.
  // The last IR/DR data clock sets TMS high to leave the shift state.
  const auto tms = BitBuffer<54>::fromBits(
      "01100"                           // Enter Shift-IR.
      "000000001"                       // Shift 9 IR bits; exit on the last bit.
      "10"                              // Update-IR -> Run-Test/Idle.
      "100"                             // Enter Shift-DR.
      "00000000" "00000000" "00000000" "00000000" "1" // Shift 33 DR bits.
      "10");                            // Update-DR -> Run-Test/Idle.

  // Debug IDCODE (0xE, LSB first), then BoundaryScan BYPASS (0x1F).
  const auto debugTdi = BitBuffer<54>::fromBits(
      "00000"
      "0111" "11111"
      "00"
      "000"
      "00000000" "00000000" "00000000" "00000000" "0"
      "00");

  // Debug BYPASS (0xF), then BoundaryScan IDCODE (0x01, LSB first).
  const auto boundaryTdi = BitBuffer<54>::fromBits(
      "00000"
      "1111" "10000"
      "00"
      "000"
      "00000000" "00000000" "00000000" "00000000" "0"
      "00");

  // clockCycles() captures TDO on EVERY clock, including TAP transitions.
  // DR data starts at index 5 + 9 + 2 + 3 = 19 in the captured sequence.
  constexpr size_t DrStart = 19;
  BitBuffer<54> captured;

  jtag.reset();
  if (jtag.clockCycles(tms, debugTdi, captured) != JTAG::ERROR::NO) {
    Serial.println("Error reading Debug TAP IDCODE");
  } else {
    uint32_t idcode = 0;
    // Debug IDCODE occupies indices 19..50; index 51 is BoundaryScan BYPASS.
    for (size_t i = 0; i < 32; ++i) {
      idcode |= uint32_t(captured.getBit(DrStart + i)) << i;
    }
    Serial.print("Debug TAP IDCODE: ");
    Serial.println(idcode, HEX);
  }

  jtag.reset();
  if (jtag.clockCycles(tms, boundaryTdi, captured) != JTAG::ERROR::NO) {
    Serial.println("Error reading BoundaryScan TAP IDCODE");
  } else {
    uint32_t idcode = 0;
    // Index 19 is Debug BYPASS; BoundaryScan IDCODE occupies indices 20..51.
    for (size_t i = 0; i < 32; ++i) {
      idcode |= uint32_t(captured.getBit(DrStart + 1 + i)) << i;
    }
    Serial.print("BoundaryScan TAP IDCODE: ");
    Serial.println(idcode, HEX);
  }
}
