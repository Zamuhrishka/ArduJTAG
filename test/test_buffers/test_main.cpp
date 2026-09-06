#include <Arduino.h>
#include "Jtag.hpp"
#include <unity.h>
#include <string.h>

static size_t clocks;
static uint8_t sent[32];
static const uint8_t response[] = {0xA5, 0x63, 0xFE, 0x91};

JtagPin::JtagPin(int, int) {}
JtagBus::JtagBus(JtagPin a, JtagPin b, JtagPin c, JtagPin d, JtagPin e)
  : _tms(a), _tdi(b), _tdo(c), _tck(d), _rst(e) {}

uint8_t JtagBus::clock(uint8_t, uint8_t tdi)
{
  const size_t tick = clocks++;

  if (tick < 3 || tick >= 35) {
    return 0;
  }

  const size_t bit = tick - 3;
  JTAG::setBitArray(bit, sent, tdi);
  return (response[bit / 8] >> (bit % 8)) & 1;
}

JTAG::ERROR JtagBus::clockCycles(size_t, const uint8_t[], const uint8_t[], uint8_t *) { return JTAG::ERROR::NO; }
JTAG::ERROR JtagBus::setSpeed(uint32_t) { return JTAG::ERROR::NO; }

void setUp()
{
  clocks = 0;
  memset(sent, 0, sizeof(sent));
}

void tearDown() {}

/**
 * Tests that bits and bytes are equivalent when representing the same data.
 */
void test_bits_and_bytes_are_equivalent()
{
  auto bits = BitBuffer<>::fromBits("1010101000000110");
  auto bytes = BitBuffer<>::fromBytes({0x55, 0x60});
  TEST_ASSERT_TRUE(bits.valid());
  TEST_ASSERT_TRUE(bytes.valid());
  TEST_ASSERT_EQUAL_UINT32(16, bits.bitCount());
  const uint8_t expected[] = {0x55, 0x60};
  TEST_ASSERT_EQUAL_HEX8_ARRAY(expected, bits.data(), 2);
  TEST_ASSERT_EQUAL_HEX8_ARRAY(expected, bytes.data(), 2);
}

/**
 * Tests that existing byte arrays are supported.
 */
void test_existing_byte_arrays_are_supported()
{
  const uint8_t raw[] = {0x55, 0xFE};
  auto full = BitBuffer<16>::fromBytes(raw);
  auto partial = BitBuffer<11>::fromBytes(raw, 11);
  TEST_ASSERT_TRUE(full.valid());
  TEST_ASSERT_EQUAL_UINT32(16, full.bitCount());
  TEST_ASSERT_EQUAL_HEX8_ARRAY(raw, full.data(), 2);
  TEST_ASSERT_TRUE(partial.valid());
  TEST_ASSERT_EQUAL_UINT32(11, partial.bitCount());
  TEST_ASSERT_EQUAL_HEX8(6, partial.byte(1));
}

/**
 *
 */
void test_partial_byte_is_masked()
{
  auto partial = BitBuffer<11>::fromBytes({0x55, 0xFE}, 11);
  TEST_ASSERT_TRUE(partial.valid());
  TEST_ASSERT_EQUAL_UINT32(11, partial.bitCount());
  TEST_ASSERT_EQUAL_HEX8(6, partial.byte(1));
}

/**
 * Tests that invalid bit strings are rejected.
 */
void test_invalid_bit_strings_are_rejected()
{
  TEST_ASSERT_FALSE(BitBuffer<8>::fromBits("111111111").valid());
  TEST_ASSERT_FALSE(BitBuffer<>::fromBits("10x").valid());
  TEST_ASSERT_FALSE(BitBuffer<>::fromBits(nullptr).valid());
  TEST_ASSERT_FALSE(BitBuffer<>::fromBits("").valid());
}

/**
 * Tests that invalid byte inputs are rejected.
 */
void test_invalid_byte_inputs_are_rejected()
{
  TEST_ASSERT_FALSE(BitBuffer<>::fromBytes({}).valid());
  TEST_ASSERT_FALSE(BitBuffer<>::fromBytes({}, 0).valid());
  TEST_ASSERT_FALSE(BitBuffer<>::fromBytes({}, 1).valid());
  TEST_ASSERT_FALSE(BitBuffer<>::fromBytes(nullptr).valid());
  TEST_ASSERT_FALSE(BitBuffer<>::fromBytes({-1}).valid());
  TEST_ASSERT_FALSE(BitBuffer<>::fromBytes({256}).valid());
  TEST_ASSERT_FALSE(BitBuffer<>::fromBytes({0x55}, 9).valid());
  TEST_ASSERT_FALSE(BitBuffer<7>::fromBytes({0x55}).valid());
}

/**
 * Tests that bit access respects bounds.
 */
void test_bit_access_respects_bounds()
{
  auto partial = BitBuffer<11>::fromBytes({0x55, 0x06}, 11);
  TEST_ASSERT_FALSE(partial.set(11, true));
  TEST_ASSERT_EQUAL_HEX8(0, partial.byte(2));
  TEST_ASSERT_FALSE(partial.getBit(11));
  TEST_ASSERT_TRUE(partial.set(0, false));
  TEST_ASSERT_EQUAL_HEX8(0x54, partial.byte(0));
}

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
 * Tests that raw DR transfers preserve the guard byte.
 */
void test_raw_dr_preserves_guard_byte()
{
  Jtag jtag(1, 2, 3, 4, 5);
  struct { uint8_t data[4]; uint8_t guard; } out = {{}, 0xCC};
  const uint8_t raw[4] = {};
  jtag.dr(raw, 32, out.data);
  TEST_ASSERT_EQUAL_HEX8(0xCC, out.guard);
  TEST_ASSERT_EQUAL_HEX8_ARRAY(response, out.data, 4);
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
  RUN_TEST(test_bits_and_bytes_are_equivalent);
  RUN_TEST(test_existing_byte_arrays_are_supported);
  RUN_TEST(test_partial_byte_is_masked);
  RUN_TEST(test_invalid_bit_strings_are_rejected);
  RUN_TEST(test_invalid_byte_inputs_are_rejected);
  RUN_TEST(test_bit_access_respects_bounds);
  RUN_TEST(test_dr_transfers_1_bit);
  RUN_TEST(test_dr_transfers_8_bits);
  RUN_TEST(test_dr_transfers_9_bits);
  RUN_TEST(test_dr_transfers_11_bits);
  RUN_TEST(test_dr_transfers_16_bits);
  RUN_TEST(test_dr_transfers_32_bits);
  RUN_TEST(test_raw_dr_preserves_guard_byte);
  RUN_TEST(test_small_output_is_rejected_without_clocks);
  RUN_TEST(test_empty_input_is_rejected_without_clocks);
  return UNITY_END();
}
