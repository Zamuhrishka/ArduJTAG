#include <JtagChain.hpp>
#include <profiles/ArmJtagDp.hpp>

Jtag jtag(3, 4, 5, 2, 6); // TMS, TDI, TDO, TCK, TRST
JtagChain<2, 40> chain(jtag);

void setup()
{
  Serial.begin(115200);
  // STM32F407 chain, physical order from TDI to TDO:
  // BoundaryScan TAP (5-bit IR), then Debug TAP (4-bit IR).
  if (!chain.add(JtagDevice(5)) || !chain.add(JtagDevice::fromProfile<ArmJtagDp>())) {
    Serial.println("Invalid chain configuration");
    return;
  }
  jtag.reset();
}

void loop()
{
  uint32_t id;
  // Index 1 selects the Debug TAP; BoundaryScan is placed in BYPASS.
  if (chain.readIdcode<ArmJtagDp>(1, id) != JTAG::ERROR::NO) {
    Serial.println("JTAG transfer failed");
    return;
  }
  Serial.println(id, HEX);
  delay(1000);
}
