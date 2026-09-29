#include <Arduino.h>
#include <backends/gpio/JtagGpio.hpp>
#include <unity.h>

// Exercise the real header-only GPIO backend with simulated Arduino functions.

static const uint8_t TMS_PIN = 1, TDI_PIN = 2, TDO_PIN = 3, TCK_PIN = 4;
static uint8_t levels[6];
static uint32_t targetBits;
static size_t risingEdges, fallingEdges, reads, serialCalls;
static unsigned long nowMicros;
TestSerial Serial;
void TestSerial::print(unsigned char) { ++serialCalls; }
void TestSerial::println(unsigned char) { ++serialCalls; }
unsigned long micros() { return nowMicros; }
void delayMicroseconds(unsigned int us) { nowMicros += us; }

void pinMode(unsigned int, int) {}
void digitalWrite(unsigned int pin, int value)
{
  if (pin == TCK_PIN && value != levels[pin]) {
    if (value == HIGH) {
      ++risingEdges;
    } else {
      ++fallingEdges;
      // Model a target advancing its TDO output on the falling edge.
      targetBits >>= 1;
    }
  }
  levels[pin] = value;
}

int digitalRead(unsigned int pin)
{
  TEST_ASSERT_EQUAL_UINT32(TDO_PIN, pin);
  TEST_ASSERT_EQUAL_UINT8(HIGH, levels[TCK_PIN]);
  ++reads;
  return targetBits & 1U;
}

void setUp()
{
  for (size_t i = 0; i < sizeof(levels); ++i) {
    levels[i] = LOW;
  }

  targetBits = 0x4BA00477;
  risingEdges = fallingEdges = reads = serialCalls = 0;
  nowMicros = 0;
}

void tearDown() {}

void test_clock_preserves_first_and_last_tdo_bits()
{
  JtagGpio gpio(GpioPin(1, OUTPUT), GpioPin(2, OUTPUT), GpioPin(3, INPUT),
                GpioPin(4, OUTPUT), GpioPin(5, OUTPUT));

  // Exercise both a typical IDCODE and a pattern with its top bit set.
  const uint32_t patterns[] = {0x4BA00477UL, 0x80000001UL};

  for (size_t pattern = 0; pattern < 2; ++pattern) {
    targetBits = patterns[pattern];
    uint32_t received = 0;
    for (size_t bitIndex = 0; bitIndex < 32; ++bitIndex) {
      received |= uint32_t(gpio.clock(bitIndex == 31, bitIndex % 2)) << bitIndex;
      TEST_ASSERT_EQUAL_UINT8(LOW, levels[TCK_PIN]);
      TEST_ASSERT_EQUAL_UINT8(bitIndex == 31, levels[TMS_PIN]);
      TEST_ASSERT_EQUAL_UINT8(bitIndex % 2, levels[TDI_PIN]);
    }

    TEST_ASSERT_EQUAL_HEX32(patterns[pattern], received);
  }

  TEST_ASSERT_EQUAL_UINT32(64, risingEdges);
  TEST_ASSERT_EQUAL_UINT32(64, fallingEdges);
  TEST_ASSERT_EQUAL_UINT32(64, reads);
  TEST_ASSERT_EQUAL_UINT32(0, serialCalls);
}

void test_clock_timing_across_counter_boundaries()
{
  // A 100 kHz clock needs at least 5 us for each half-period.
  const uint32_t starts[] = {65534UL, 65536UL, 100000UL, 0xFFFFFFFDUL};
  for (uint32_t start : starts) {
    nowMicros = start;
    JtagGpio gpio(GpioPin(1, OUTPUT), GpioPin(2, OUTPUT), GpioPin(3, INPUT),
                  GpioPin(4, OUTPUT), GpioPin(5, OUTPUT));
    TEST_ASSERT_EQUAL_INT(static_cast<int>(JTAG::ERROR::NO),
                          static_cast<int>(gpio.setSpeed(100)));
    // Wrap explicitly: unsigned long is wider on some host platforms.
    nowMicros = uint32_t(start + 3U);
    gpio.clock(0, 0);
    TEST_ASSERT_EQUAL_UINT32(uint32_t(start + 10U), uint32_t(nowMicros));

    // A long pause already satisfies the low half-period; only high remains.
    nowMicros = uint32_t(nowMicros + 100000UL);
    const uint32_t before = nowMicros;
    gpio.clock(0, 0);
    TEST_ASSERT_EQUAL_UINT32(uint32_t(before + 5U), uint32_t(nowMicros));
  }
}

int main()
{
  UNITY_BEGIN();
  RUN_TEST(test_clock_preserves_first_and_last_tdo_bits);
  RUN_TEST(test_clock_timing_across_counter_boundaries);
  return UNITY_END();
}
