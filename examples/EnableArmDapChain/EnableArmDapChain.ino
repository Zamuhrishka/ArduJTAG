// ArduJTAG JtagChain version of EnableArmDap.
// Reuses the original DP/AP requests and reset points. This demonstrates raw
// register exchanges, not a complete DAP driver with ACK/WAIT handling.
#include <JtagChain.hpp>
#include <profiles/ArmJtagDp.hpp>

#define TCK 2
#define TMS 3
#define TDI 4
#define TDO 5
#define RST 6

Jtag jtag(TMS, TDI, TDO, TCK, RST);
JtagChain<2, 36> chain(jtag);
BitBuffer<35> output;
bool chainReady = false;

void setup()
{
  Serial.begin(115200);
  // Physical order TDI -> BoundaryScan (5-bit IR) -> Debug (4-bit IR) -> TDO.
  chainReady = chain.add(JtagDevice(5)) &&
               chain.add(JtagDevice::fromProfile<ArmJtagDp>());
  if (!chainReady) {
    Serial.println("Invalid chain configuration");
    return;
  }
}

void loop()
{
  // The original sketch uses 36-bit whole-chain requests: 35 target bits
  // followed by one BoundaryScan BYPASS bit. JtagChain adds that bit itself.
  // Preserve the low 35 bits of each original request in transmission order.
  const auto DP_SELECT_REG = BitBuffer<35>::fromBytes({0x04, 0x00, 0x00, 0x00, 0x00}, 35);
  const auto AP_CSW_REG = BitBuffer<35>::fromBytes({0x10, 0x00, 0x00, 0x00, 0x00}, 35);
  const auto AP_TAR_REG = BitBuffer<35>::fromBytes({0x02, 0x00, 0x00, 0x00, 0x01}, 35);
  const auto AP_DRW_REG_W = BitBuffer<35>::fromBytes({0x2E, 0x55, 0x55, 0x55, 0x55}, 35);
  const auto AP_DRW_REG_R = BitBuffer<35>::fromBytes({0x07, 0x00, 0x00, 0x00, 0x00}, 35);
  const auto ZEROS = BitBuffer<35>::fromBytes({0x00, 0x00, 0x00, 0x00, 0x00}, 35);
  const auto ENABLE_DP = BitBuffer<35>::fromBytes({0x02, 0x00, 0x00, 0x80, 0x02}, 35);

  JTAG::ERROR status;

  // ENABLE
  jtag.reset();
  status = chain.transfer(1, ArmJtagDp::Instruction::Dpacc, ENABLE_DP, output);
  if (status != JTAG::ERROR::NO) {
    Serial.print("JTAG transfer failed: ");
    Serial.println(static_cast<long>(status));
    return;
  }

  // WRITE
  jtag.reset();
  status = chain.transfer(1, ArmJtagDp::Instruction::Dpacc, DP_SELECT_REG, output);
  if (status != JTAG::ERROR::NO) {
    Serial.print("JTAG transfer failed: ");
    Serial.println(static_cast<long>(status));
    return;
  }
  jtag.reset();
  status = chain.transfer(1, ArmJtagDp::Instruction::Apacc, AP_CSW_REG, output);
  if (status != JTAG::ERROR::NO) {
    Serial.print("JTAG transfer failed: ");
    Serial.println(static_cast<long>(status));
    return;
  }
  jtag.reset();
  status = chain.transfer(1, ArmJtagDp::Instruction::Apacc, AP_TAR_REG, output);
  if (status != JTAG::ERROR::NO) {
    Serial.print("JTAG transfer failed: ");
    Serial.println(static_cast<long>(status));
    return;
  }
  jtag.reset();
  status = chain.transfer(1, ArmJtagDp::Instruction::Apacc, AP_DRW_REG_W, output);
  if (status != JTAG::ERROR::NO) {
    Serial.print("JTAG transfer failed: ");
    Serial.println(static_cast<long>(status));
    return;
  }

  // READ
  jtag.reset();
  status = chain.transfer(1, ArmJtagDp::Instruction::Dpacc, DP_SELECT_REG, output);
  if (status != JTAG::ERROR::NO) {
    Serial.print("JTAG transfer failed: ");
    Serial.println(static_cast<long>(status));
    return;
  }
  jtag.reset();
  status = chain.transfer(1, ArmJtagDp::Instruction::Apacc, AP_CSW_REG, output);
  if (status != JTAG::ERROR::NO) {
    Serial.print("JTAG transfer failed: ");
    Serial.println(static_cast<long>(status));
    return;
  }
  jtag.reset();
  status = chain.transfer(1, ArmJtagDp::Instruction::Apacc, AP_TAR_REG, output);
  if (status != JTAG::ERROR::NO) {
    Serial.print("JTAG transfer failed: ");
    Serial.println(static_cast<long>(status));
    return;
  }
  jtag.reset();
  status = chain.transfer(1, ArmJtagDp::Instruction::Apacc, AP_DRW_REG_R, output);
  if (status != JTAG::ERROR::NO) {
    Serial.print("JTAG transfer failed: ");
    Serial.println(static_cast<long>(status));
    return;
  }

  // Preserve the final zero request without a reset. Unlike the original
  // dr-only call, transfer() reloads APACC before this DR exchange.
  status = chain.transfer(1, ArmJtagDp::Instruction::Apacc, ZEROS, output);
  if (status != JTAG::ERROR::NO) {
    Serial.print("JTAG transfer failed: ");
    Serial.println(static_cast<long>(status));
    return;
  }

  // The response contains 35 target bits (ACK + data), without the BYPASS bit.
  Serial.print("> ");
  for (size_t i = 0; i < output.byteCount(); ++i) {
    Serial.print(output.byte(i), HEX);
    Serial.print(' ');
  }
  Serial.println();
}
