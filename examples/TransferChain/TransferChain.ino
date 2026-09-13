// Read the Debug TAP IDCODE using ArduJTAG's generic chain transfer API.
#include <JtagChain.hpp>
#include <profiles/ArmJtagDp.hpp>

Jtag jtag(3, 4, 5, 2, 6); // TMS, TDI, TDO, TCK, TRST
// Capacity includes the target's 32 DR bits and one BYPASS bit.
JtagChain<2, 40> chain(jtag);
bool chainReady = false;

void setup()
{
  Serial.begin(115200);
  // STM32F407: physical order from TDI to TDO.
  // Index 0: BoundaryScan TAP, 5-bit IR.
  // Index 1: Debug TAP, 4-bit IR, ARM JTAG-DP profile.
  chainReady = chain.add(JtagDevice(5)) &&
               chain.add(JtagDevice::fromProfile<ArmJtagDp>());
  if (!chainReady) {
    Serial.println("Invalid chain configuration");
    return;
  }
  jtag.reset();
}

void loop()
{
  if (!chainReady) { return; }

  // IDCODE requires exactly 32 target DR bits. Shift zeros to read the register.
  const auto request = BitBuffer<32>::fromBytes({0, 0, 0, 0});
  BitBuffer<32> response;

  // transfer() loads IDCODE on index 1 and BYPASS on every other device,
  // exchanges the combined DR, and returns only the target's response bits.
  const JTAG::ERROR status = chain.transfer(
      1, ArmJtagDp::Instruction::Idcode, request, response);

  // For a device without a profile, replace the call above with the raw overload:
  // const auto instruction = BitBuffer<4>::fromBits("0111");
  // const JTAG::ERROR status = chain.transfer(1, instruction, request, response);

  if (status != JTAG::ERROR::NO) {
    Serial.print("JTAG transfer failed: ");
    Serial.println(static_cast<long>(status));
    delay(1000);
    return;
  }

  Serial.print("IDCODE bytes (least significant first): ");
  for (size_t i = 0; i < response.byteCount(); ++i) {
    if (response.byte(i) < 0x10) Serial.print('0');
    Serial.print(response.byte(i), HEX);
    Serial.print(' ');
  }
  Serial.println();
  delay(1000);
}
