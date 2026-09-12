#include <Arduino.h>
#include "Jtag.hpp"
#include <unity.h>

// Stubs required by the JTAG sources linked into native test modules.
GpioPin::GpioPin(int, int) {}
JtagGpio::JtagGpio(GpioPin a, GpioPin b, GpioPin c, GpioPin d, GpioPin e)
  : _tms(a), _tdi(b), _tdo(c), _tck(d), _rst(e) {}

uint8_t JtagGpio::clock(uint8_t, uint8_t) { return 0; }
JTAG::ERROR JtagGpio::setSpeed(uint32_t) { return JTAG::ERROR::NO; }

void setUp() {}
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
  TEST_ASSERT_EQUAL_UINT32(16, bytes.bitCount());
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
 * Tests that partial bytes are masked correctly.
 * The unused high bits of the last byte should be cleared to zero.
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

int main()
{
  UNITY_BEGIN();
  RUN_TEST(test_bits_and_bytes_are_equivalent);
  RUN_TEST(test_existing_byte_arrays_are_supported);
  RUN_TEST(test_partial_byte_is_masked);
  RUN_TEST(test_invalid_bit_strings_are_rejected);
  RUN_TEST(test_invalid_byte_inputs_are_rejected);
  RUN_TEST(test_bit_access_respects_bounds);
  return UNITY_END();
}
