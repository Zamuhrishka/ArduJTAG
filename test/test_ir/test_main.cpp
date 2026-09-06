#include <Arduino.h>
#include "Jtag.hpp"
#include <unity.h>
#include <string.h>

static size_t clocks;
static uint8_t clockTms[4097];
static uint8_t clockTdi[4097];

JtagPin::JtagPin(int, int) {}
JtagBus::JtagBus(JtagPin a, JtagPin b, JtagPin c, JtagPin d, JtagPin e)
  : _tms(a), _tdi(b), _tdo(c), _tck(d), _rst(e) {}

uint8_t JtagBus::clock(uint8_t tms, uint8_t tdi)
{
  const size_t tick = clocks++;
  TEST_ASSERT_TRUE(tick < sizeof(clockTms) * 8);
  JTAG::setBitArray(tick, clockTms, tms);
  JTAG::setBitArray(tick, clockTdi, tdi);
  return 0;
}

JTAG::ERROR JtagBus::clockCycles(size_t, const uint8_t[], const uint8_t[], uint8_t *)
{
  TEST_FAIL_MESSAGE("IR must use individual bus clocks");
  return JTAG::ERROR::NO;
}
JTAG::ERROR JtagBus::setSpeed(uint32_t) { return JTAG::ERROR::NO; }

void setUp()
{
  clocks = 0;
  memset(clockTms, 0, sizeof(clockTms));
  memset(clockTdi, 0, sizeof(clockTdi));
}

void tearDown() {}

static void check_ir_trace(const BitBuffer<32767> &input)
{
  const size_t count = input.bitCount();
  TEST_ASSERT_EQUAL_UINT32(count + 7, clocks);
  // Enter Shift-IR with 0,1,1,0,0; exit with the final data bit, then 1,0.
  const uint8_t pre[] = {0, 1, 1, 0, 0};
  for (size_t i = 0; i < sizeof(pre); ++i) {
    TEST_ASSERT_EQUAL_INT(pre[i], JTAG::getBitArray(i, clockTms));
    TEST_ASSERT_EQUAL_INT(0, JTAG::getBitArray(i, clockTdi));
  }
  for (size_t i = 0; i < count; ++i) {
    TEST_ASSERT_EQUAL_INT(input.getBit(i), JTAG::getBitArray(i + 5, clockTdi));
    TEST_ASSERT_EQUAL_INT(i == count - 1, JTAG::getBitArray(i + 5, clockTms));
  }
  TEST_ASSERT_EQUAL_INT(1, JTAG::getBitArray(count + 5, clockTms));
  TEST_ASSERT_EQUAL_INT(0, JTAG::getBitArray(count + 6, clockTms));
  TEST_ASSERT_EQUAL_INT(0, JTAG::getBitArray(count + 5, clockTdi));
  TEST_ASSERT_EQUAL_INT(0, JTAG::getBitArray(count + 6, clockTdi));
}

static void check_ir_buffer(size_t count)
{
  Jtag jtag(1, 2, 3, 4, 5);
  BitBuffer<32767> input;
  TEST_ASSERT_TRUE(input.resize(count));
  for (size_t i = 0; i < count; ++i) input.set(i, i % 3 == 0);
  const auto before = input;
  TEST_ASSERT_EQUAL_INT(int(JTAG::ERROR::NO), int(jtag.ir(input)));
  check_ir_trace(before);
  TEST_ASSERT_EQUAL_UINT32(before.bitCount(), input.bitCount());
  TEST_ASSERT_EQUAL_HEX8_ARRAY(before.data(), input.data(), input.byteCount());
}

static void test_ir_single_bit() { check_ir_buffer(1); }
static void test_ir_full_byte() { check_ir_buffer(8); }
static void test_ir_partial_byte() { check_ir_buffer(9); }
static void test_ir_long_instruction() { check_ir_buffer(33); }
static void test_ir_max_capacity() { check_ir_buffer(32767); }

static void check_numeric_ir(size_t count)
{
  Jtag jtag(1, 2, 3, 4, 5);
  auto expected = BitBuffer<32767>::fromBytes({0xFE, 0xA5}, count);
  TEST_ASSERT_EQUAL_INT(int(JTAG::ERROR::NO), int(jtag.ir(0xA5FE, count)));
  check_ir_trace(expected);
  uint8_t numericTms[3] = {};
  uint8_t numericTdi[3] = {};
  memcpy(numericTms, clockTms, sizeof(numericTms));
  memcpy(numericTdi, clockTdi, sizeof(numericTdi));
  clocks = 0;
  TEST_ASSERT_EQUAL_INT(int(JTAG::ERROR::NO), int(jtag.ir(expected)));
  TEST_ASSERT_EQUAL_HEX8_ARRAY(numericTms, clockTms, sizeof(numericTms));
  TEST_ASSERT_EQUAL_HEX8_ARRAY(numericTdi, clockTdi, sizeof(numericTdi));
}

static void test_ir_numeric_single_bit() { check_numeric_ir(1); }
static void test_ir_numeric_partial_byte() { check_numeric_ir(9); }
static void test_ir_numeric_full_width() { check_numeric_ir(16); }

static void test_ir_invalid_inputs_without_clocks()
{
  Jtag jtag(1, 2, 3, 4, 5);
  BitBuffer<> empty;
  TEST_ASSERT_EQUAL_INT(int(JTAG::ERROR::INVALID_BUFFER), int(jtag.ir(empty)));
  TEST_ASSERT_EQUAL_INT(int(JTAG::ERROR::INVALID_SEQUENCE_LEN), int(jtag.ir(0xFFFF, 0)));
  TEST_ASSERT_EQUAL_INT(int(JTAG::ERROR::INVALID_SEQUENCE_LEN), int(jtag.ir(0xFFFF, 17)));
  TEST_ASSERT_EQUAL_INT(int(JTAG::ERROR::INVALID_SEQUENCE_LEN), int(jtag.ir(0xFFFF, UINT16_MAX)));
  TEST_ASSERT_EQUAL_UINT32(0, clocks);
  TEST_ASSERT_FALSE(empty.valid());
}

int main()
{
  UNITY_BEGIN();
  RUN_TEST(test_ir_single_bit);
  RUN_TEST(test_ir_full_byte);
  RUN_TEST(test_ir_partial_byte);
  RUN_TEST(test_ir_long_instruction);
  RUN_TEST(test_ir_max_capacity);
  RUN_TEST(test_ir_numeric_single_bit);
  RUN_TEST(test_ir_numeric_partial_byte);
  RUN_TEST(test_ir_numeric_full_width);
  RUN_TEST(test_ir_invalid_inputs_without_clocks);
  return UNITY_END();
}
