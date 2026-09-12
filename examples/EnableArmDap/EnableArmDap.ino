// This sketch demonstrates a more complex use of the ArduJTAG library to interact with a microchip via JTAG.
// It shows a series of operations including enabling JTAG functionality, writing to registers, and reading back data.
// The sketch sets up the JTAG interface, sends various instructions and data, and prints out the results.
// Such operations are typical in configuring and verifying the state of a JTAG-compatible device.

#include <Arduino.h>

#include "Jtag.hpp"

// Define the pin numbers for JTAG interface
#define TCK 2  // Test Clock
#define TMS 3  // Test Mode Select
#define TDI 4  // Test Data In
#define TDO 5  // Test Data Out
#define RST 6  // Reset

// Create an instance of the Jtag class with the specified pin assignments
Jtag jtag = Jtag(TMS, TDI, TDO, TCK, RST);

// Create a buffer to store the output data
BitBuffer<36> output;

void setup()
{
  Serial.begin(115200);
}

void loop()
{
  const auto dpInstruction = BitBuffer<9>::fromBytes({0xFA, 0x01}, 9);
  const auto apInstruction = BitBuffer<9>::fromBytes({0xFB, 0x01}, 9);
  const auto DP_SELECT_REG = BitBuffer<36>::fromBytes({0x04, 0x00, 0x00, 0x00, 0x00}, 36);
  const auto AP_CSW_REG = BitBuffer<36>::fromBytes({0x10, 0x00, 0x00, 0x00, 0x00}, 36);
  const auto AP_TAR_REG = BitBuffer<36>::fromBytes({0x02, 0x00, 0x00, 0x00, 0x01}, 36);
  const auto AP_DRW_REG_W = BitBuffer<36>::fromBytes({0x2E, 0x55, 0x55, 0x55, 0x55}, 36);
  const auto AP_DRW_REG_R = BitBuffer<36>::fromBytes({0x07, 0x00, 0x00, 0x00, 0x00}, 36);
  const auto ZEROS = BitBuffer<36>::fromBytes({0x00, 0x00, 0x00, 0x00, 0x00}, 36);
  const auto ENABLE_DP = BitBuffer<36>::fromBytes({0x02, 0x00, 0x00, 0x80, 0x02}, 36);

  // ENABLE
  jtag.reset();
  jtag.ir(dpInstruction);
  jtag.dr(ENABLE_DP, output);

  // WRITE
  jtag.reset();
  jtag.ir(dpInstruction);
  jtag.dr(DP_SELECT_REG, output);

  jtag.reset();
  jtag.ir(apInstruction);
  jtag.dr(AP_CSW_REG, output);

  jtag.reset();
  jtag.ir(apInstruction);
  jtag.dr(AP_TAR_REG, output);

  jtag.reset();
  jtag.ir(apInstruction);
  jtag.dr(AP_DRW_REG_W, output);

  // READ
  jtag.reset();
  jtag.ir(dpInstruction);
  jtag.dr(DP_SELECT_REG, output);

  jtag.reset();
  jtag.ir(apInstruction);
  jtag.dr(AP_CSW_REG, output);

  jtag.reset();
  jtag.ir(apInstruction);
  jtag.dr(AP_TAR_REG, output);

  jtag.reset();
  jtag.ir(apInstruction);
  jtag.dr(AP_DRW_REG_R, output);

  jtag.dr(ZEROS, output);

  Serial.print("> ");

  for (size_t i = 0; i < output.byteCount(); i++)
  {
    Serial.print(output.byte(i), HEX);
    Serial.print(" ");
  }

  Serial.println(" ");
}
