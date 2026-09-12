// This sketch demonstrates how to use the ArduJtag library to perform a sequence of JTAG operations.
// It sets up the JTAG pins, initializes the JTAG interface, and then sends a specific sequence of operations
// to the JTAG device. After the sequence is complete, the resulting output is read into a buffer and printed
// to the Serial Monitor. This is a common operation for interacting with and testing JTAG-compatible devices.

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
BitBuffer<54> id;

void setup()
{
  Serial.begin(115200);
}

void loop()
{
  // Define the sequence of TMS and TDI values to send in the JTAG operation
  // For read ID for chip need to send next bits:
  // TMS: 01100 | 000000001 | 10 | 100 | 000000000000000000000000000000011 | 00
  // TDI: 00000 | 011111111 | 00 | 000 | 000000000000000000000000000000000 | 00
  const auto tms = BitBuffer<54>::fromBits("01100" "000000001" "10" "100" "000000000000000000000000000000011" "00");
  const auto tdi = BitBuffer<54>::fromBits("00000" "011111111" "00" "000" "000000000000000000000000000000000" "00");

  jtag.reset();
  if (jtag.clockCycles(tms, tdi, id) != JTAG::ERROR::NO) {
    Serial.println("JTAG transfer failed");
    return;
  }

  Serial.print("> ");

  for (size_t i = 0; i < id.byteCount(); i++)
  {
    Serial.print(id.byte(i), HEX);
    Serial.print(" ");
  }
  Serial.println(" ");
}
