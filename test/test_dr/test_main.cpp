#include <Arduino.h>
#include "Jtag.hpp"
#include <unity.h>
#include <string.h>

static size_t clocks;
static uint8_t sent[4];
static const uint8_t response[] = {0xA5, 0x63, 0xFE, 0x91};

GpioPin::GpioPin(int, int) {}
JtagGpio::JtagGpio(GpioPin a, GpioPin b, GpioPin c, GpioPin d, GpioPin e)
  : _tms(a), _tdi(b), _tdo(c), _tck(d), _rst(e) {}

uint8_t JtagGpio::clock(uint8_t, uint8_t tdi)
{
  const size_t tick = clocks++;

  if (tick < 3 || tick >= 35) {
    return 0;
  }

  const size_t bit = tick - 3;
  JTAG::setBitArray(bit, sent, tdi);
  return (response[bit / 8] >> (bit % 8)) & 1;
}

JTAG::ERROR JtagGpio::setSpeed(uint32_t) { return JTAG::ERROR::NO; }

void setUp()
{
  clocks = 0;
  memset(sent, 0, sizeof(sent));
}

void tearDown() {}

/**
 * Checks DR transfers for a given length.
 */
static void check_dr_transfer(size_t length)
{
  Jtag jtag(1, 2, 3, 4, 5);
  const uint8_t raw[] = {0x55, 0x60, 0xAA, 0x0F};
  auto input = BitBuffer<32>::fromBytes(raw, sizeof(raw), length);
  BitBuffer<32> output;
  TEST_ASSERT_EQUAL_INT(int(JTAG::ERROR::NO), int(jtag.dr(input, output)));
  TEST_ASSERT_EQUAL_UINT32(length + 5, clocks);
  TEST_ASSERT_EQUAL_UINT32(length, output.bitCount());
  for (size_t i = 0; i < length; ++i)
  {
    TEST_ASSERT_EQUAL_UINT8((response[i / 8] >> (i % 8)) & 1, output.getBit(i));
    TEST_ASSERT_EQUAL_UINT8(input.getBit(i), (sent[i / 8] >> (i % 8)) & 1);
  }
  if (length % 8)
    TEST_ASSERT_EQUAL_HEX8(0, output.byte(output.byteCount() - 1) >> (length % 8));
}

void test_dr_transfers_1_bit() { check_dr_transfer(1); }
void test_dr_transfers_8_bits() { check_dr_transfer(8); }
void test_dr_transfers_9_bits() { check_dr_transfer(9); }
void test_dr_transfers_11_bits() { check_dr_transfer(11); }
void test_dr_transfers_16_bits() { check_dr_transfer(16); }
void test_dr_transfers_32_bits() { check_dr_transfer(32); }

/**
 * Tests that DR transfers preserve the guard byte through the public API.
 */
void test_dr_preserves_guard_byte()
{
  Jtag jtag(1, 2, 3, 4, 5);
  struct { BitBuffer<32> data; uint8_t guard; } out = {{}, 0xCC};
  const auto input = BitBuffer<32>::fromBytes({0, 0, 0, 0});
  TEST_ASSERT_EQUAL_INT(int(JTAG::ERROR::NO), int(jtag.dr(input, out.data)));
  TEST_ASSERT_EQUAL_HEX8(0xCC, out.guard);
  TEST_ASSERT_EQUAL_HEX8_ARRAY(response, out.data.data(), 4);
}

/**
 * Tests that small output buffers are rejected without clocks.
 */
void test_small_output_is_rejected_without_clocks()
{
  Jtag jtag(1, 2, 3, 4, 5);
  auto input = BitBuffer<>::fromBytes({0x55, 0x60});
  BitBuffer<8> output;
  TEST_ASSERT_EQUAL_INT(int(JTAG::ERROR::INVALID_BUFFER), int(jtag.dr(input, output)));
  TEST_ASSERT_EQUAL_UINT32(0, clocks);
}

/**
 * Tests that empty input buffers are rejected without clocks.
 */
void test_empty_input_is_rejected_without_clocks()
{
  Jtag jtag(1, 2, 3, 4, 5);
  BitBuffer<> input;
  BitBuffer<8> output;
  TEST_ASSERT_EQUAL_INT(int(JTAG::ERROR::INVALID_BUFFER), int(jtag.dr(input, output)));
  TEST_ASSERT_EQUAL_UINT32(0, clocks);
}

int main()
{
  UNITY_BEGIN();
  RUN_TEST(test_dr_transfers_1_bit);
  RUN_TEST(test_dr_transfers_8_bits);
  RUN_TEST(test_dr_transfers_9_bits);
  RUN_TEST(test_dr_transfers_11_bits);
  RUN_TEST(test_dr_transfers_16_bits);
  RUN_TEST(test_dr_transfers_32_bits);
  RUN_TEST(test_dr_preserves_guard_byte);
  RUN_TEST(test_small_output_is_rejected_without_clocks);
  RUN_TEST(test_empty_input_is_rejected_without_clocks);
  return UNITY_END();
}
