// Select BYPASS on the ARM Debug TAP using ArduJTAG.
#include <JtagChain.hpp>
#include <profiles/ArmJtagDp.hpp>

Jtag jtag(3, 4, 5, 2, 6); // TMS, TDI, TDO, TCK, TRST
JtagChain<2, 9> chain(jtag);

void setup()
{
  Serial.begin(115200);
  // STM32F407: TDI -> BoundaryScan (5-bit IR) -> Debug (4-bit IR) -> TDO.
  if (!chain.add(JtagDevice(5)) || !chain.add(JtagDevice::fromProfile<ArmJtagDp>())) {
    Serial.println("Invalid chain configuration");
    return;
  }
  jtag.reset();
  auto debugPort = chain.device<ArmJtagDp>(1);

  // Only IR is shifted. The chain also places the other TAP in BYPASS.
  const JTAG::ERROR status = debugPort.bypass();
  // Equivalent generic instruction selection:
  // const JTAG::ERROR status = debugPort.select(ArmJtagDp::Instruction::Bypass);
  if (status != JTAG::ERROR::NO) {
    Serial.print("BYPASS failed: ");
    Serial.println(static_cast<long>(status));
    return;
  }
  Serial.println("Both TAPs are in BYPASS");
}

void loop() {}
