#include <JtagChain.hpp>

Jtag jtag(3, 4, 5, 2, 6); // TMS, TDI, TDO, TCK, TRST
JtagChain<2, 40> chain(jtag);

void setup()
{
  Serial.begin(115200);
  // STM32F407 chain, physical order from TDI to TDO:
  // BoundaryScan TAP (5-bit IR), then Debug TAP (4-bit IR).
  if (!chain.add(JtagDevice(5)) || !chain.add(JtagDevice(4))) {
    Serial.println("Invalid chain configuration");
    return;
  }
  jtag.reset();
}

void loop()
{
  // Debug TAP IDCODE instruction 0xE, least significant bit first.
  const auto instruction = BitBuffer<4>::fromBits("0111");
  const auto input = BitBuffer<32>::fromBytes({0, 0, 0, 0});
  BitBuffer<32> id;
  // Index 1 selects the Debug TAP; BoundaryScan is placed in BYPASS.
  if (chain.transfer(1, instruction, input, id) != JTAG::ERROR::NO) {
    Serial.println("JTAG transfer failed");
    return;
  }
  for (size_t i = 0; i < id.byteCount(); ++i) {
    Serial.print(id.byte(i), HEX);
    Serial.print(" ");
  }
  Serial.println();
  delay(1000);
}
