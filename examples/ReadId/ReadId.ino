#include <Arduino.h>

#include "Jtag.hpp"

#define TCK 2
#define TMS 3
#define TDI 4
#define TDO 5
#define RST 6

Jtag arm_jtag = Jtag(TMS, TDI, TDO, TCK, RST);

byte output[500] = {};

void setup()
{
  Serial.begin(115200);
}

void loop()
{
  uint16_t instruction = 0x1FE;
  uint16_t length = 9;
  uint32_t zero = 0;
  uint32_t id = 0;

  arm_jtag.reset();
  arm_jtag.ir(instruction = 0x1FE, length = 9);
  arm_jtag.dr((byte *)&zero, length = 32, (uint8_t *)&id);

  Serial.print("> ");
  Serial.println(id, HEX);

  delay(3000);
}
