//_____ I N C L U D E S _______________________________________________________
#include <Arduino.h>

#include "Jtag.hpp"
#include "version.hpp"

//_____ D E F I N I T I O N S _________________________________________________
#define MAX_DATA_LENGTH 128

#define TCK 2
#define TMS 3
#define TDI 4
#define TDO 5
#define RST 6

//_____ V A R I A B L E S _____________________________________________________
Jtag arm_jtag = Jtag(TMS, TDI, TDO, TCK, RST);

byte output[500] = {};
byte dataBuffer[MAX_DATA_LENGTH];

//_____ F U N C T I O N S _____________________________________________________
static void jtag_test_1(void)
{
  uint16_t instruction = 0x1FE;
  uint16_t length = 9;
  uint32_t data = 0;
  uint32_t id = 0;

  arm_jtag.reset();
  arm_jtag.ir(instruction = 0x1FE, length = 9);
  arm_jtag.dr((byte *)&data, length = 32, (uint8_t *)&id);

  Serial.print("> ");
  Serial.println(id, HEX);
}

static void jtag_test_2(void)
{
  uint8_t tms[] = {0x06, 0x60, 0x01, 0x00, 0x00, 0x00, 0x0C};
  uint8_t tdi[] = {0xC0, 0x3F, 0x00, 0x00, 0x00, 0x00, 0x00};
  size_t length = 54;

  arm_jtag.reset();
  arm_jtag.sequence(length = 54, tms, tdi, output);

  Serial.print("> ");

  for (size_t i = 0; i < length / 8; i++)
  {
    // uint8_t bb = swap_nibbles(output[i]);
    Serial.print(output[i], HEX);
    Serial.print(" ");
  }
  Serial.println(" ");
}

static void jtag_test_3(void)
{
  uint16_t instruction = 0x1FA;
  uint16_t length = 9;
  uint8_t data[] = {0x04, 0x00, 0x00, 0x00, 0x00};
  uint8_t data_1[] = {0x10, 0x00, 0x00, 0x00, 0x00};
  uint8_t data_2[] = {0x02, 0x00, 0x00, 0x00, 0x01};
  uint8_t data_3[] = {0x2E, 0x55, 0x55, 0x55, 0x55};
  uint8_t data_4[] = {0x07, 0x00, 0x00, 0x00, 0x00};
  uint8_t zeros[] = {0x00, 0x00, 0x00, 0x00, 0x00};
  uint8_t enable[] = {0x02, 0x00, 0x00, 0x80, 0x02};
  uint8_t enable_rd[] = {0x03, 0x00, 0x00, 0x80, 0x02};
  uint32_t id = 0;

  // ENABLE
  Serial.println("E============================E");
  arm_jtag.reset();
  arm_jtag.ir(instruction = 0x1FA, length = 9);
  arm_jtag.dr(enable, length = 36, output);

  Serial.println("----------------------------");

  arm_jtag.reset();
  arm_jtag.ir(instruction = 0x1FA, length = 9);
  arm_jtag.dr(enable_rd, length = 36, output);

  Serial.println("----------------------------");

  // arm_jtag.reset();
  // arm_jtag.dr(zeros, length = 36, output);

  // WRITE
  Serial.println("W============================W");
  arm_jtag.reset();
  arm_jtag.ir(instruction = 0x1FA, length = 9);
  arm_jtag.dr(data, length = 36, output);

  Serial.println("----------------------------");

  arm_jtag.reset();
  arm_jtag.ir(instruction = 0x1FB, length = 9);
  arm_jtag.dr(data_1, length = 36, output);

  Serial.println("----------------------------");

  arm_jtag.reset();
  arm_jtag.ir(instruction = 0x1FB, length = 9);
  arm_jtag.dr(data_2, length = 36, output);

  Serial.println("----------------------------");

  arm_jtag.reset();
  arm_jtag.ir(instruction = 0x1FB, length = 9);
  arm_jtag.dr(data_3, length = 36, output);

  Serial.println("----------------------------");

  // READ
  Serial.println("R============================R");
  arm_jtag.reset();
  arm_jtag.ir(instruction = 0x1FA, length = 9);
  arm_jtag.dr(data, length = 36, output);

  Serial.println("----------------------------");

  arm_jtag.reset();
  arm_jtag.ir(instruction = 0x1FB, length = 9);
  arm_jtag.dr(data_1, length = 36, output);

  Serial.println("----------------------------");

  arm_jtag.reset();
  arm_jtag.ir(instruction = 0x1FB, length = 9);
  arm_jtag.dr(data_2, length = 36, output);

  Serial.println("----------------------------");

  arm_jtag.reset();
  arm_jtag.ir(instruction = 0x1FB, length = 9);
  arm_jtag.dr(data_4, length = 36, output);

  arm_jtag.dr(zeros, length = 36, output);

  Serial.println("----------------------------");

  Serial.print("> ");

  for (size_t i = 0; i < 36 / 8; i++)
  {
    Serial.print(output[i], HEX);
    Serial.print(" ");
  }

  Serial.println(" ");
}
//_____ A R D U I N O ____________________________________________________________
void setup()
{
  Serial.begin(115200);
}

void loop()
{
  Serial.println("Start");

  // jtag_test_1();
  // delay(1000);
  // jtag_test_2();
  // delay(1000);
  // jtag_test_3();
  // delay(1000);

  uint8_t in[] = {0xFF, 0x00, 0xF8, 0x55};
  arm_jtag.dr(in, 32, output);

  delay(3000);
}
