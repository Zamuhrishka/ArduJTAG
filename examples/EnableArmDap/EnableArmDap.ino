// This sketch demonstrates a more complex use of the ArduJtag library to interact with a microchip via JTAG.
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
byte output[500] = {};

void setup()
{
  Serial.begin(115200);
}

void loop()
{
  uint16_t instruction = 0x1FA;
  uint16_t length = 9;
  uint8_t DP_SELECT_REG[] = {0x04, 0x00, 0x00, 0x00, 0x00};
  uint8_t AP_CSW_REG[] = {0x10, 0x00, 0x00, 0x00, 0x00};
  uint8_t AP_TAR_REG[] = {0x02, 0x00, 0x00, 0x00, 0x01};
  uint8_t AP_DRW_REG_W[] = {0x2E, 0x55, 0x55, 0x55, 0x55};
  uint8_t AP_DRW_REG_R[] = {0x07, 0x00, 0x00, 0x00, 0x00};
  uint8_t ZEROS[] = {0x00, 0x00, 0x00, 0x00, 0x00};
  uint8_t ENABLE_DP[] = {0x02, 0x00, 0x00, 0x80, 0x02};

  // ENABLE
  jtag.reset();
  jtag.ir(instruction = 0x1FA, length = 9);
  jtag.dr(ENABLE_DP, length = 36, output);

  // WRITE
  jtag.reset();
  jtag.ir(instruction = 0x1FA, length = 9);
  jtag.dr(DP_SELECT_REG, length = 36, output);

  jtag.reset();
  jtag.ir(instruction = 0x1FB, length = 9);
  jtag.dr(AP_CSW_REG, length = 36, output);

  jtag.reset();
  jtag.ir(instruction = 0x1FB, length = 9);
  jtag.dr(AP_TAR_REG, length = 36, output);

  jtag.reset();
  jtag.ir(instruction = 0x1FB, length = 9);
  jtag.dr(AP_DRW_REG_W, length = 36, output);

  // READ
  jtag.reset();
  jtag.ir(instruction = 0x1FA, length = 9);
  jtag.dr(DP_SELECT_REG, length = 36, output);

  jtag.reset();
  jtag.ir(instruction = 0x1FB, length = 9);
  jtag.dr(AP_CSW_REG, length = 36, output);

  jtag.reset();
  jtag.ir(instruction = 0x1FB, length = 9);
  jtag.dr(AP_TAR_REG, length = 36, output);

  jtag.reset();
  jtag.ir(instruction = 0x1FB, length = 9);
  jtag.dr(AP_DRW_REG_R, length = 36, output);

  jtag.dr(ZEROS, length = 36, output);

  Serial.print("> ");

  for (size_t i = 0; i < 36 / 8; i++)
  {
    Serial.print(output[i], HEX);
    Serial.print(" ");
  }

  Serial.println(" ");
}
