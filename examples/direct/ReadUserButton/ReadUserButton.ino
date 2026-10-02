/**
 * @file ReadUserButton.ino
 * @brief Read STM32F4DISCOVERY B1 USER through JTAG SAMPLE, using Jtag/BitBuffer.
 * Target: STM32F407 LQFP100, ST BSDL V1.1 (2014-05-30).
 * Hold target NRST LOW externally; controller D6 is JTRST, not NRST.
 * B1 connects PA0 to VDD when pressed; its released level is LOW.
 * SAMPLE captures inputs without selecting EXTEST or driving boundary outputs.
 */
#include <Arduino.h>
#include <jtag/core/Jtag.hpp>

Jtag jtag(3, 4, 5, 2, 6); // TMS, TDI, TDO, TCK, JTRST

// DR order: Debug BYPASS bit, then 406 BoundaryScan cells.
constexpr size_t BypassOffset = 1;
constexpr size_t UserInputCell = 316; // PA0 INPUT cell, not its output cell 317.
BitBuffer<407> request;
BitBuffer<407> captured;
bool ready = false;
bool stateKnown = false;
bool lastPressed = false;

// SAMPLE and PRELOAD share opcode 0x02, so every scan also updates preload
// latches. Use an explicit vector with all output controls disabled (1),
// internal cells 0 and BSDL X values chosen as 0. This does not drive pins
// in SAMPLE; these values would become active if EXTEST were selected later.
const uint16_t OutputControlCells[] = {
    405, 402, 399, 396, 393, 387, 384, 381, 336, 333, 330, 327, 324, 321,
    318, 315, 312, 297, 294, 291, 288, 285, 282, 279, 276, 273, 270,
    246, 243, 240, 237, 234, 231, 228, 225, 222, 219, 216,
    192, 189, 186, 183, 180, 177, 174, 171, 168, 165, 162, 159,
    135, 132, 129, 126, 123, 120, 117, 114, 111,
    87, 84, 81, 78, 75, 72, 69, 66, 63, 60, 57, 33, 30, 27, 23, 20, 17, 14
};

void setup()
{
  Serial.begin(115200);
  Serial.println(F("ReadUserButton: hold target NRST LOW externally"));

  // TDI -> BoundaryScan TAP (5-bit IR) -> Debug TAP (4-bit IR) -> TDO.
  // fromBits() starts with Debug BYPASS (0xF), then Boundary IDCODE (0x01).
  const auto idcodeIr = BitBuffer<9>::fromBits("1111" "10000");
  const auto idRequest = BitBuffer<33>::fromBytes({0, 0, 0, 0, 0}, 33);
  BitBuffer<33> idResponse;
  jtag.reset();
  if (jtag.ir(idcodeIr) != JTAG::ERROR::NO ||
      jtag.dr(idRequest, idResponse) != JTAG::ERROR::NO) {
    Serial.println(F("IDCODE transfer failed"));
    return;
  }
  uint32_t idcode = 0;
  for (size_t i = 0; i < 32; ++i) {
    idcode |= uint32_t(idResponse.getBit(i + BypassOffset)) << i;
  }
  Serial.print(F("Boundary TAP IDCODE: "));
  Serial.println(idcode, HEX);
  if ((idcode & 0x0FFFFFFFUL) != 0x06413041UL) {
    Serial.println(F("Unexpected target IDCODE; sampling disabled"));
    return;
  }

  request.resize(407);
  for (uint16_t cell : OutputControlCells) request.set(BypassOffset + cell, true);

  // Keep Debug in BYPASS and select Boundary SAMPLE/PRELOAD (0x02).
  const auto sampleIr = BitBuffer<9>::fromBits("1111" "01000");
  if (jtag.ir(sampleIr) != JTAG::ERROR::NO) {
    Serial.println(F("SAMPLE selection failed"));
    return;
  }
  ready = true;
}

void loop()
{
  if (!ready) return;

  // SAMPLE remains selected. Each DR exchange captures a fresh input snapshot.
  if (jtag.dr(request, captured) != JTAG::ERROR::NO) {
    Serial.println(F("SAMPLE transfer failed; sampling stopped"));
    jtag.reset();
    ready = false;
    return;
  }

  // Cell 316 appears at bit 317 after accounting for Debug BYPASS.
  const bool pressed = captured.getBit(BypassOffset + UserInputCell);
  if (!stateKnown || pressed != lastPressed) {
    Serial.println(pressed ? F("USER: PRESSED") : F("USER: RELEASED"));
    lastPressed = pressed;
    stateKnown = true;
  }
  // Polling interval, not a debounce algorithm. Short pulses may be missed.
  // delay(20);
}
