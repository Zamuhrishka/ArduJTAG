#include <Arduino.h>
#include "Jtag.hpp"
#include <unity.h>
#include <string.h>

static size_t clocks;
static uint8_t sent[32];
static uint8_t sentTms[32];
static size_t cycleCalls;
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

JTAG::ERROR JtagBus::clockCycles(size_t count, const uint8_t tms[], const uint8_t tdi[], uint8_t *tdo)
{
  ++cycleCalls;
  for (size_t i = 0; i < count; ++i) {
    ++clocks;
    JTAG::setBitArray(i, sentTms, JTAG::getBitArray(i, tms));
    JTAG::setBitArray(i, sent, JTAG::getBitArray(i, tdi));
    JTAG::setBitArray(i, tdo, JTAG::getBitArray(i % (sizeof(response) * 8), response));
  }
  return JTAG::ERROR::NO;
}
JTAG::ERROR JtagBus::setSpeed(uint32_t) { return JTAG::ERROR::NO; }

void setUp()
{
  clocks = 0;
  cycleCalls = 0;
  memset(sentTms, 0, sizeof(sentTms));
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

static void check_clock_cycles(size_t count)
{
  Jtag jtag(1, 2, 3, 4, 5);
  BitBuffer<256> tms;
  BitBuffer<300> tdi;
  BitBuffer<320> tdo;
  TEST_ASSERT_TRUE(tms.resize(count));
  TEST_ASSERT_TRUE(tdi.resize(count));
  TEST_ASSERT_TRUE(tdo.resize(tdo.capacity()));
  for (size_t i = 0; i < tdo.bitCount(); ++i) tdo.set(i, true);
  for (size_t i = 0; i < count; ++i) {
    tms.set(i, i % 3 == 0);
    tdi.set(i, i % 5 == 0);
  }
  TEST_ASSERT_EQUAL_INT(int(JTAG::ERROR::NO), int(jtag.clockCycles(tms, tdi, tdo)));
  TEST_ASSERT_EQUAL_UINT32(1, cycleCalls);
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

void test_clock_cycles_single_bit() { check_clock_cycles(1); }
void test_clock_cycles_full_byte() { check_clock_cycles(8); }
void test_clock_cycles_partial_byte() { check_clock_cycles(11); }
void test_clock_cycles_max_length() { check_clock_cycles(256); }

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
  TEST_ASSERT_EQUAL_UINT32(0, cycleCalls);
  TEST_ASSERT_EQUAL_UINT32(before.bitCount(), tdo.bitCount());
  TEST_ASSERT_EQUAL_HEX8_ARRAY(before.data(), tdo.data(), (tdo.capacity() + 7) / 8);
  TEST_ASSERT_EQUAL_UINT32(tmsLength, tms.bitCount());
  TEST_ASSERT_EQUAL_UINT32(tdiLength, tdi.bitCount());
}

void test_clock_cycles_empty_tms() { check_invalid_cycles(0, 8, JTAG::ERROR::INVALID_BUFFER); }
void test_clock_cycles_empty_tdi() { check_invalid_cycles(8, 0, JTAG::ERROR::INVALID_BUFFER); }
void test_clock_cycles_both_empty() { check_invalid_cycles(0, 0, JTAG::ERROR::INVALID_BUFFER); }
void test_clock_cycles_unequal_lengths() { check_invalid_cycles(8, 9, JTAG::ERROR::INVALID_BUFFER); }
void test_clock_cycles_over_limit() { check_invalid_cycles(257, 257, JTAG::ERROR::INVALID_SEQUENCE_LEN); }

void test_clock_cycles_small_output()
{
  Jtag jtag(1, 2, 3, 4, 5);
  auto input = BitBuffer<16>::fromBytes({0xA5, 0x5A});
  auto output = BitBuffer<8>::fromBytes({0xCC});
  TEST_ASSERT_EQUAL_INT(int(JTAG::ERROR::INVALID_BUFFER), int(jtag.clockCycles(input, input, output)));
  TEST_ASSERT_EQUAL_UINT32(0, clocks);
  TEST_ASSERT_EQUAL_UINT32(0, cycleCalls);
  TEST_ASSERT_EQUAL_UINT32(8, output.bitCount());
  TEST_ASSERT_EQUAL_HEX8(0xCC, output.byte(0));
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
  RUN_TEST(test_dr_preserves_guard_byte);
  RUN_TEST(test_small_output_is_rejected_without_clocks);
  RUN_TEST(test_empty_input_is_rejected_without_clocks);
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
