#include <Arduino.h>
#include "Jtag.hpp"
#include <unity.h>
#include <string.h>

static size_t clocks;
static uint8_t sent[32];
static uint8_t sentTms[32];
static const uint8_t response[] = {0xA5, 0x63, 0xFE, 0x91};

GpioPin::GpioPin(int, int) {}
JtagGpio::JtagGpio(GpioPin a, GpioPin b, GpioPin c, GpioPin d, GpioPin e)
  : _tms(a), _tdi(b), _tdo(c), _tck(d), _rst(e) {}

uint8_t JtagGpio::clock(uint8_t tms, uint8_t tdi)
{
  const size_t tick = clocks++;
  TEST_ASSERT_TRUE(tick < sizeof(sent) * 8);
  JTAG::setBitArray(tick, sentTms, tms);
  JTAG::setBitArray(tick, sent, tdi);
  return JTAG::getBitArray(tick % (sizeof(response) * 8), response);
}
JTAG::ERROR JtagGpio::setSpeed(uint32_t) { return JTAG::ERROR::NO; }

void setUp()
{
  clocks = 0;
  memset(sentTms, 0, sizeof(sentTms));
  memset(sent, 0, sizeof(sent));
}

void tearDown() {}

/**
 * Checks clockCycles() for a given number of cycles.
 */
static void check_clock_cycles(size_t count)
{
  Jtag jtag(1, 2, 3, 4, 5);
  BitBuffer<256> tms;
  BitBuffer<300> tdi;
  BitBuffer<320> tdo;
  TEST_ASSERT_TRUE(tms.resize(count));
  TEST_ASSERT_TRUE(tdi.resize(count));
  TEST_ASSERT_TRUE(tdo.resize(tdo.capacity()));
  for (size_t i = 0; i < tdo.bitCount(); ++i) {
    tdo.set(i, true);
  }

  for (size_t i = 0; i < count; ++i) {
    tms.set(i, i % 3 == 0);
    tdi.set(i, i % 5 == 0);
  }
  TEST_ASSERT_EQUAL_INT(int(JTAG::ERROR::NO), int(jtag.clockCycles(tms, tdi, tdo)));
  TEST_ASSERT_EQUAL_UINT32(count, clocks);
  TEST_ASSERT_EQUAL_UINT32(count, tdo.bitCount());
  for (size_t i = 0; i < count; ++i) {
    TEST_ASSERT_EQUAL_INT(i % 3 == 0, JTAG::getBitArray(i, sentTms));
    TEST_ASSERT_EQUAL_INT(i % 5 == 0, JTAG::getBitArray(i, sent));
    TEST_ASSERT_EQUAL_INT(i % 3 == 0, tms.getBit(i));
    TEST_ASSERT_EQUAL_INT(i % 5 == 0, tdi.getBit(i));
    TEST_ASSERT_EQUAL_INT(JTAG::getBitArray(i % (sizeof(response) * 8), response), tdo.getBit(i));
  }
  if (count % 8)
    TEST_ASSERT_EQUAL_HEX8(0, tdo.byte(tdo.byteCount() - 1) >> (count % 8));
}

/**
 * Tests clockCycles() for a given number of cycles.
 */
void test_clock_cycles_single_bit() { check_clock_cycles(1); }
void test_clock_cycles_full_byte() { check_clock_cycles(8); }
void test_clock_cycles_partial_byte() { check_clock_cycles(11); }
void test_clock_cycles_max_length() { check_clock_cycles(256); }

/**
 * Checks that clockCycles() rejects invalid input and leaves buffers unchanged.
 */
static void check_invalid_cycles(size_t tmsLength, size_t tdiLength, JTAG::ERROR expected)
{
  Jtag jtag(1, 2, 3, 4, 5);
  BitBuffer<257> tms;
  BitBuffer<257> tdi;
  auto tdo = BitBuffer<257>::fromBytes({0xA5, 0x05}, 11);
  if (tmsLength) tms.resize(tmsLength);
  if (tdiLength) tdi.resize(tdiLength);
  const auto before = tdo;
  TEST_ASSERT_EQUAL_INT(int(expected), int(jtag.clockCycles(tms, tdi, tdo)));
  TEST_ASSERT_EQUAL_UINT32(0, clocks);
  TEST_ASSERT_EQUAL_UINT32(before.bitCount(), tdo.bitCount());
  TEST_ASSERT_EQUAL_HEX8_ARRAY(before.data(), tdo.data(), (tdo.capacity() + 7) / 8);
  TEST_ASSERT_EQUAL_UINT32(tmsLength, tms.bitCount());
  TEST_ASSERT_EQUAL_UINT32(tdiLength, tdi.bitCount());
}

/**
 * Tests that clockCycles() rejects invalid input and leaves buffers unchanged.
 */
void test_clock_cycles_empty_tms() { check_invalid_cycles(0, 8, JTAG::ERROR::INVALID_BUFFER); }
void test_clock_cycles_empty_tdi() { check_invalid_cycles(8, 0, JTAG::ERROR::INVALID_BUFFER); }
void test_clock_cycles_both_empty() { check_invalid_cycles(0, 0, JTAG::ERROR::INVALID_BUFFER); }
void test_clock_cycles_unequal_lengths() { check_invalid_cycles(8, 9, JTAG::ERROR::INVALID_BUFFER); }
void test_clock_cycles_over_limit() { check_invalid_cycles(257, 257, JTAG::ERROR::INVALID_SEQUENCE_LEN); }

/**
 * Tests that clockCycles() handles small output buffers correctly.
 */
void test_clock_cycles_small_output()
{
  Jtag jtag(1, 2, 3, 4, 5);
  auto input = BitBuffer<16>::fromBytes({0xA5, 0x5A});
  auto output = BitBuffer<8>::fromBytes({0xCC});
  TEST_ASSERT_EQUAL_INT(int(JTAG::ERROR::INVALID_BUFFER), int(jtag.clockCycles(input, input, output)));
  TEST_ASSERT_EQUAL_UINT32(0, clocks);
  TEST_ASSERT_EQUAL_UINT32(8, output.bitCount());
  TEST_ASSERT_EQUAL_HEX8(0xCC, output.byte(0));
}

void test_reset_generates_five_tms_high_clocks()
{
  Jtag jtag(1, 2, 3, 4, 5);
  jtag.reset();
  TEST_ASSERT_EQUAL_UINT32(5, clocks);
  TEST_ASSERT_EQUAL_HEX8(0x1F, sentTms[0]);
  TEST_ASSERT_EQUAL_HEX8(0, sent[0]);
}

int main()
{
  UNITY_BEGIN();
  RUN_TEST(test_reset_generates_five_tms_high_clocks);
  RUN_TEST(test_clock_cycles_single_bit);
  RUN_TEST(test_clock_cycles_full_byte);
  RUN_TEST(test_clock_cycles_partial_byte);
  RUN_TEST(test_clock_cycles_max_length);
  RUN_TEST(test_clock_cycles_empty_tms);
  RUN_TEST(test_clock_cycles_empty_tdi);
  RUN_TEST(test_clock_cycles_both_empty);
  RUN_TEST(test_clock_cycles_unequal_lengths);
  RUN_TEST(test_clock_cycles_over_limit);
  RUN_TEST(test_clock_cycles_small_output);
  return UNITY_END();
}
