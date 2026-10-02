/**
 * @file ExtestLeds.ino
 * @brief One LED chase on STM32F4DISCOVERY using only Jtag and BitBuffer.
 * Target: STM32F407 in LQFP100, ST BSDL V1.1 (2014-05-30).
 * Hold target NRST LOW externally throughout the test; D6 is JTRST, not NRST.
 * EXTEST affects all boundary outputs: non-LED drivers are disabled here.
 * Check external connections before running. No STM32 firmware is needed.
 */
#include <Arduino.h>
#include <jtag/core/Jtag.hpp>

Jtag jtag(3, 4, 5, 2, 6); // TMS, TDI, TDO, TCK, JTRST

// BSDL indices, before adding the Debug TAP's one-bit BYPASS offset.
// LD4 green / PD12, LD3 orange / PD13, LD5 red / PD14, LD6 blue / PD15.
// The board's LEDs are active high. Control=0 enables a driver; 1 disables it.
const uint16_t LedOutputCells[] = {167, 164, 161, 158};
const uint16_t LedControlCells[] = {168, 165, 162, 159};

// Every OUTPUT3 control cell from STM32F405_415_407_417_LQFP100.bsd.
// Set these to 1 initially; internal cells must remain 0. BSDL X values use 0.
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
  Serial.println(F("ExtestLeds: hold target NRST LOW externally"));

  // TDI -> BoundaryScan (5-bit IR) -> Debug (4-bit IR) -> TDO.
  // The Debug TAP instruction is shifted first. Keep it in BYPASS (1111).
  // fromBits() uses transmission order: the first character is sent first.
  const auto idcodeIr = BitBuffer<9>::fromBits("1111" "10000"); // Boundary IDCODE = 0x01.
  const auto preloadIr = BitBuffer<9>::fromBits("1111" "01000"); // PRELOAD = 0x02.
  const auto extestIr = BitBuffer<9>::fromBits("1111" "00000"); // EXTEST = 0x00.
  const auto bypassIr = BitBuffer<9>::fromBits("1111" "11111"); // Both TAPs in BYPASS.

  jtag.reset();
  const auto idRequest = BitBuffer<33>::fromBytes({0, 0, 0, 0, 0}, 33);
  BitBuffer<33> idResponse;
  if (jtag.ir(idcodeIr) != JTAG::ERROR::NO ||
      jtag.dr(idRequest, idResponse) != JTAG::ERROR::NO) {
    Serial.println(F("IDCODE transfer failed"));
    return;
  }
  uint32_t idcode = 0;
  for (size_t i = 0; i < 32; ++i) {
    idcode |= uint32_t(idResponse.getBit(i + 1)) << i; // Skip Debug BYPASS.
  }
  Serial.print(F("Boundary TAP IDCODE: "));
  Serial.println(idcode, HEX);
  if ((idcode & 0x0FFFFFFFUL) != 0x06413041UL) {
    Serial.println(F("Unexpected target IDCODE; EXTEST not selected"));
    return;
  }

  // DR order: one Debug BYPASS bit, followed by all 406 BoundaryScan cells.
  // Thus BSDL cell N is stored at vector index N+1; index 0 stays zero.
  constexpr size_t BypassOffset = 1;
  BitBuffer<407> values;
  BitBuffer<407> captured;
  values.resize(407); // Zero-filled buffer.
  for (uint16_t cell : OutputControlCells) values.set(BypassOffset + cell, true);
  // Enable just the four LED drivers, with all four output values LOW (off).
  for (uint16_t cell : LedControlCells) values.set(BypassOffset + cell, false);

  // Load the complete vector BEFORE EXTEST: selecting EXTEST immediately
  // activates the values already in the boundary output latches.
  if (jtag.ir(preloadIr) != JTAG::ERROR::NO ||
      jtag.dr(values, captured) != JTAG::ERROR::NO) {
    Serial.println(F("PRELOAD failed"));
    return;
  }
  if (jtag.ir(extestIr) != JTAG::ERROR::NO) {
    Serial.println(F("EXTEST selection failed"));
    return;
  }

  // Keep EXTEST selected. Each DR Update applies the next complete vector.
  for (size_t led = 0; led < 4; ++led) {
    for (size_t i = 0; i < 4; ++i) {
      values.set(BypassOffset + LedOutputCells[i], i == led);
    }
    if (jtag.dr(values, captured) != JTAG::ERROR::NO) {
      Serial.println(F("LED transfer failed"));
      jtag.reset(); // Leave EXTEST on failure.
      return;
    }
    delay(500);
  }

  // Turn LEDs off while their drivers are still enabled.
  for (uint16_t cell : LedOutputCells) values.set(BypassOffset + cell, false);
  if (jtag.dr(values, captured) != JTAG::ERROR::NO) {
    Serial.println(F("LED off transfer failed"));
    jtag.reset();
    return;
  }
  // Apply a vector with all output drivers disabled before leaving EXTEST.
  for (uint16_t cell : LedControlCells) values.set(BypassOffset + cell, true);
  if (jtag.dr(values, captured) != JTAG::ERROR::NO) {
    Serial.println(F("Output disable failed"));
    jtag.reset();
    return;
  }
  if (jtag.ir(bypassIr) != JTAG::ERROR::NO) {
    Serial.println(F("BYPASS selection failed"));
    jtag.reset();
    return;
  }
  // BYPASS restores normal target pin control; the BSR no longer drives LEDs.
  Serial.println(F("LED chase complete; TAPs in BYPASS"));
}

void loop() {} // One pass; reset the Arduino to repeat.
