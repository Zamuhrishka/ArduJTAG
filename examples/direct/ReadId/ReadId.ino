/**
 * @file ReadId.ino
 * @brief Read both STM32F407 TAP IDCODEs using whole-chain IR/DR buffers.
 * Physical order: TDI -> BoundaryScan TAP (5-bit IR) -> Debug TAP (4-bit IR) -> TDO.
 * Each read selects IDCODE on one TAP and BYPASS on the other.
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
  // Instructions for the TAP nearest TDO are shifted first.
  // fromBytes() takes bytes in array order, LSB first within each byte.
  // Debug IDCODE (0xE) + BoundaryScan BYPASS (0x1F): 0111 11111.
  const auto readDebugTapIdInstruction = BitBuffer<9>::fromBytes({0xFE, 0x01}, 9);
  // Debug BYPASS (0xF) + BoundaryScan IDCODE (0x01): 1111 10000.
  const auto readBoundaryScanTapIdInstruction = BitBuffer<9>::fromBytes({0x1F, 0x00}, 9);

  // The complete DR chain has 32 IDCODE bits and one BYPASS bit.
  const auto input = BitBuffer<33>::fromBytes({0, 0, 0, 0, 0}, 33);
  BitBuffer<33> debugTapId;
  BitBuffer<33> boundaryScanTapId;

  jtag.reset();
  JTAG::ERROR status = jtag.ir(readDebugTapIdInstruction);
  if (status == JTAG::ERROR::NO) status = jtag.dr(input, debugTapId);

  if (status != JTAG::ERROR::NO) {
    Serial.println("Error reading Debug TAP IDCODE");
  } else {
    uint32_t id = 0;
    // Debug TAP is nearest TDO: bits 0..31 are IDCODE; bit 32 is BYPASS.
    for (size_t i = 0; i < 32; ++i) {
      id |= uint32_t(debugTapId.getBit(i)) << i;
    }
    Serial.print("Debug TAP IDCODE: ");
    Serial.println(id, HEX);
  }

  jtag.reset();
  status = jtag.ir(readBoundaryScanTapIdInstruction);
  if (status == JTAG::ERROR::NO) status = jtag.dr(input, boundaryScanTapId);

  if (status != JTAG::ERROR::NO) {
    Serial.println("Error reading BoundaryScan TAP IDCODE");
  } else {
    uint32_t id = 0;
    // Debug BYPASS arrives first: skip bit 0 and decode IDCODE from bits 1..32.
    for (size_t i = 0; i < 32; ++i) {
      id |= uint32_t(boundaryScanTapId.getBit(i + 1)) << i;
    }
    Serial.print("BoundaryScan TAP IDCODE: ");
    Serial.println(id, HEX);
  }
}
