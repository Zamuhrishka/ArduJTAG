// ArduJTAG standard boundary-scan operations on a single target.
#include <JtagChain.hpp>
#include "ExampleBoundaryProfile.hpp"

// Set true after replacing the teaching profile and vectors for your device.
constexpr bool DeviceConfigured = false;

Jtag jtag(3, 4, 5, 2, 6); // TMS, TDI, TDO, TCK, TRST
JtagChain<1, 256> chain(jtag); // Must accommodate both the complete IR and BSR.

void setup()
{
  Serial.begin(115200);
  if (!DeviceConfigured) {
    Serial.println("Configure ExampleBoundaryProfile and BSR vectors first");
    return;
  }
  if (!chain.add(JtagDevice::fromProfile<ExampleBoundaryProfile>())) {
    Serial.println("Invalid chain configuration");
    return;
  }
  jtag.reset();
  auto boundary = chain.device<ExampleBoundaryProfile>(0);

  // Teaching vectors, NOT pin numbers or a universal safe pattern.
  // Replace every bit according to the BSDL cell order, output enables and
  // board connections. The first character is the first transmitted BSR bit.
  const auto initialValues = BitBuffer<ExampleBoundaryProfile::BoundaryLength>::fromBits("0000000000");
  const auto nextValues = BitBuffer<ExampleBoundaryProfile::BoundaryLength>::fromBits("1000000000");
  BitBuffer<ExampleBoundaryProfile::BoundaryLength> captured;
  JTAG::ERROR status;

  // SAMPLE: capture current cells and explicitly load the initial preload vector.
  status = boundary.sample(initialValues, captured);
  if (status != JTAG::ERROR::NO) {
    Serial.print("SAMPLE failed: ");
    Serial.println(static_cast<long>(status));
    return;
  }
  Serial.print("SAMPLE (bytes, least significant first): ");
  for (size_t i = 0; i < captured.byteCount(); ++i) {
    Serial.print(captured.byte(i), HEX);
    Serial.print(' ');
  }
  Serial.println();

  // PRELOAD: prepare output/control latches before selecting a test mode.
  status = boundary.preload(initialValues);
  if (status != JTAG::ERROR::NO) {
    Serial.print("PRELOAD failed: ");
    Serial.println(static_cast<long>(status));
    return;
  }
  Serial.println("PRELOAD selected/completed");

  // Combined SAMPLE/PRELOAD: capture cells and load a vector in one exchange.
  status = boundary.samplePreload(initialValues, captured);
  if (status != JTAG::ERROR::NO) {
    Serial.print("SAMPLE/PRELOAD failed: ");
    Serial.println(static_cast<long>(status));
    return;
  }
  Serial.print("SAMPLE/PRELOAD (bytes, least significant first): ");
  for (size_t i = 0; i < captured.byteCount(); ++i) {
    Serial.print(captured.byte(i), HEX);
    Serial.print(' ');
  }
  Serial.println();

  // EXTEST activates the preloaded values at Update-IR. This scan captures
  // the preceding pin state, then applies nextValues at Update-DR.
  status = boundary.extest(nextValues, captured);
  if (status != JTAG::ERROR::NO) {
    Serial.print("EXTEST initial capture failed: ");
    Serial.println(static_cast<long>(status));
    return;
  }
  Serial.print("EXTEST initial capture (bytes, least significant first): ");
  for (size_t i = 0; i < captured.byteCount(); ++i) {
    Serial.print(captured.byte(i), HEX);
    Serial.print(' ');
  }
  Serial.println();

  // Scan again to capture after applying nextValues, while retaining that vector.
  status = boundary.extest(nextValues, captured);
  if (status != JTAG::ERROR::NO) {
    Serial.print("EXTEST next capture failed: ");
    Serial.println(static_cast<long>(status));
    return;
  }
  Serial.print("EXTEST next capture (bytes, least significant first): ");
  for (size_t i = 0; i < captured.byteCount(); ++i) {
    Serial.print(captured.byte(i), HEX);
    Serial.print(' ');
  }
  Serial.println();

  // Leave external test mode before preparing an internal test.
  status = boundary.bypass();
  if (status != JTAG::ERROR::NO) {
    Serial.print("BYPASS failed: ");
    Serial.println(static_cast<long>(status));
    return;
  }
  Serial.println("BYPASS selected/completed");

  // Prepare the initial boundary values for INTEST.
  status = boundary.preload(initialValues);
  if (status != JTAG::ERROR::NO) {
    Serial.print("PRELOAD for INTEST failed: ");
    Serial.println(static_cast<long>(status));
    return;
  }
  Serial.println("PRELOAD for INTEST selected/completed");

  // INTEST: raw internal test exchange. Add device-specific initialization
  // and test clocks as required; this is not a complete core test algorithm.
  status = boundary.intest(nextValues, captured);
  if (status != JTAG::ERROR::NO) {
    Serial.print("INTEST failed: ");
    Serial.println(static_cast<long>(status));
    return;
  }
  Serial.print("INTEST (bytes, least significant first): ");
  for (size_t i = 0; i < captured.byteCount(); ++i) {
    Serial.print(captured.byte(i), HEX);
    Serial.print(' ');
  }
  Serial.println();

  // Return to BYPASS after the internal test.
  status = boundary.bypass();
  if (status != JTAG::ERROR::NO) {
    Serial.print("BYPASS failed: ");
    Serial.println(static_cast<long>(status));
    return;
  }
  Serial.println("BYPASS selected/completed");

  // HIGHZ selects an output mode with IR only; it does not shift DR.
  status = boundary.highZ();
  if (status != JTAG::ERROR::NO) {
    Serial.print("HIGHZ failed: ");
    Serial.println(static_cast<long>(status));
    return;
  }
  Serial.println("HIGHZ selected/completed");
  delay(1000); // Keep the selected mode for one second.

  // select() offers generic IR-only access; finish in BYPASS.
  status = boundary.select(ExampleBoundaryProfile::Instruction::Bypass);
  if (status != JTAG::ERROR::NO) {
    Serial.print("select(BYPASS) failed: ");
    Serial.println(static_cast<long>(status));
    return;
  }
  Serial.println("select(BYPASS) selected/completed");

  Serial.println("Boundary-scan example complete");
}

void loop() {} // Run the sequence once, rather than repeatedly changing modes.
