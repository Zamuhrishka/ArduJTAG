// This sketch demonstrates how to use the ArduJtag library to read the ID of a microchip via JTAG.
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
  uint16_t instruction = 0x1FE;  // Instruction to send to the IR register
  uint16_t length = 9;           // Length of the instruction in bits
  uint32_t zero = 0;             // Initialize variable to hold the data to be written to DR
  uint32_t id = 0;               // Initialize variable to store the ID read from the DR

  jtag.reset();                                         // Reset the JTAG state machine
  jtag.ir(instruction = 0x1FE, length = 9);             // Send the instruction to the IR register
  jtag.dr((byte *)&zero, length = 32, (uint8_t *)&id);  // Perform a data register (DR) scan

  Serial.print("> ");
  Serial.println(id, HEX);
}
