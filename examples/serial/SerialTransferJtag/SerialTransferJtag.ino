// Binary USB/Serial JTAG adapter. Do not print text on this serial port.
#include <Arduino.h>
#include <protocol/SerialTransferJtag.hpp>

Jtag jtag(3, 4, 5, 2, 6); // TMS, TDI, TDO, TCK, nTRST
SerialTransferJtag adapter(jtag);

void setup()
{
  digitalWrite(6, HIGH); // Release nTRST; target NRST is a separate signal.
  Serial.begin(115200);
  jtag.setSpeed(100);
  adapter.begin(Serial);
  // No JTAG clocks until commanded by the host. Start with RESET.
}

void loop() { adapter.update(); }
