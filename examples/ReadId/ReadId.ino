// This sketch demonstrates how to use the ArduJTAG library to read the ID of a STM32F4 via JTAG.
// It sets up the JTAG pins, initializes the JTAG interface, sends a standard ID code instruction,
// reads the response, and prints the chip ID to the Serial Monitor. This is a common operation
// in verifying communication with and the identity of a JTAG-compatible device.

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

void setup()
{
  Serial.begin(115200);
}

void loop()
{
  /**
   * @brief Read the ID code from the STM32F4.
   * @note The instruction used here is specific to the STM32F4 series and may vary for other devices.
   * Ensure that the correct instruction is used for your target device
   */
  const auto instruction = BitBuffer<32>::fromBytes({0x01, 0xFE}, 9);
  const auto input = BitBuffer<32>::fromBytes({0x00, 0x00, 0x00, 0x00});
  BitBuffer<32> output;

  jtag.reset();
  jtag.ir(instruction);
  JTAG::ERROR status = jtag.dr(input, output);

  if (status != JTAG::ERROR::NO) {
    Serial.println("Error occurred while reading JTAG data register.");
  } else {
    uint32_t id = 0;
    for (size_t i = 0; i < output.byteCount(); ++i) {
      id |= uint32_t(output.byte(i)) << (8 * i);
    }

    Serial.print("> ");
    Serial.println(id, HEX);
  }

}
