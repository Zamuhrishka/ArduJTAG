/**
 * @file BoundaryScanPins.ino
 * @brief Prepare and decode STM32F4 LQFP100 boundary vectors by pin name.
 * Hold the target's NRST low externally, as required by its BSDL.
 * NRST is not JTRST (the latter uses controller pin D6).
 */
#include <Arduino.h>
#include <jtag/boundary/BoundaryScan.hpp>
#include <jtag/chain/JtagChain.hpp>
#include <jtag/profiles/ArmJtagDp.hpp>
#include <jtag/profiles/Stm32F405_415_407_417Lqfp100Boundary.hpp>

using Profile = Stm32F405_415_407_417Lqfp100;
using Layout = Stm32F405_415_407_417Lqfp100Boundary;
using Boundary = BoundaryScan<Layout>;
using Pin = Layout::Pin;

// Enable only after checking the board connections and the entire BSR vector.
// EXTEST affects the whole boundary register, not just PD12.
constexpr bool EnableOutputTest = false;

Jtag jtag(3, 4, 5, 2, 6); // TMS, TDI, TDO, TCK, JTRST
JtagChain<2, 407> chain(jtag); // 406 BSR bits + one Debug TAP BYPASS bit.

void setup()
{
  Serial.begin(115200);
  Serial.println(F("BoundaryScanPins: target NRST must be held LOW externally"));

  // Physical order: TDI -> BoundaryScan TAP -> ARM Debug TAP -> TDO.
  if (!chain.add(JtagDevice::fromProfile<Profile>()) ||
      !chain.add(JtagDevice::fromProfile<ArmJtagDp>())) {
    Serial.println(F("Invalid chain configuration"));
    return;
  }
  jtag.reset();
  auto tap = chain.device<Profile>(0);

  uint32_t idcode = 0;
  if (tap.readIdcode(idcode) != JTAG::ERROR::NO) {
    Serial.println(F("IDCODE transfer failed"));
    return;
  }
  Serial.print(F("Boundary TAP IDCODE: "));
  Serial.println(idcode, HEX);
  if (!Profile::matchesIdcode(idcode)) {
    Serial.println(F("Unexpected Boundary TAP IDCODE"));
    return;
  }

  const auto cells = Layout::cells(Pin::Pd12);
  Serial.print(F("PD12 input/output/control cells: "));
  Serial.print(cells.input);
  Serial.print('/');
  Serial.print(cells.output);
  Serial.print('/');
  Serial.println(cells.control);

  // This only constructs a local buffer. No hardware changes occur here.
  auto values = Boundary::initialValues();
  Boundary::Values captured;
  if (tap.sample(values, captured) != JTAG::ERROR::NO) {
    Serial.println(F("SAMPLE failed"));
    return;
  }
  bool level = false;
  if (!Boundary::readInput(captured, Pin::Pd12, level)) {
    Serial.println(F("Cannot decode PD12 input"));
    return;
  }
  Serial.print(F("PD12 sampled input: "));
  Serial.println(level ? F("HIGH") : F("LOW"));

  if (EnableOutputTest) {
    // Preload the disabled-driver vector BEFORE selecting EXTEST.
    if (tap.preload(values) != JTAG::ERROR::NO) {
      Serial.println(F("PRELOAD failed"));
      return;
    }

    // Edit output data and its enable cell by pin name; still no JTAG clocks.
    if (!Boundary::setOutput(values, Pin::Pd12, true)) {
      Serial.println(F("Cannot prepare PD12 output"));
      return;
    }
    // EXTEST starts with the preloaded disabled outputs, then applies PD12 HIGH
    // at Update-DR. Other output drivers remain disabled by the vector.
    if (tap.extest(values, captured) != JTAG::ERROR::NO) {
      Serial.println(F("EXTEST failed"));
      return;
    }
    Serial.println(F("PD12 output set HIGH"));
    delay(1000);

    // A second scan captures the level AFTER the previous vector was applied.
    if (tap.extest(values, captured) != JTAG::ERROR::NO) {
      Serial.println(F("EXTEST capture failed"));
      return;
    }
    if (!Boundary::readInput(captured, Pin::Pd12, level)) {
      Serial.println(F("Cannot decode PD12 capture"));
      return;
    }
    Serial.print(F("PD12 captured while driven HIGH: "));
    Serial.println(level ? F("HIGH") : F("LOW"));

    if (!Boundary::disableOutput(values, Pin::Pd12)) {
      Serial.println(F("Cannot prepare PD12 output disable"));
      return;
    }
    // Apply the changed control bit; editing values alone would not release PD12.
    if (tap.extest(values, captured) != JTAG::ERROR::NO) {
      Serial.println(F("EXTEST output disable failed"));
      return;
    }
    Serial.println(F("PD12 driver disabled in EXTEST"));
  } else {
    Serial.println(F("Output test disabled; EXTEST was not selected"));
  }

  // Leave the test mode. BYPASS restores the target's normal pin control;
  // the disabled-driver vector does not guarantee pin levels outside EXTEST.
  if (tap.bypass() != JTAG::ERROR::NO) {
    Serial.println(F("BYPASS failed"));
    return;
  }
  Serial.println(F("Boundary scan complete; TAPs in BYPASS"));
}

void loop() {}
