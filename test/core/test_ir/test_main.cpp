#include <SimulatedGpio.hpp>
#include <Arduino.h>
#include <core/Jtag.hpp>
#include <unity.h>
#include <string.h>

static size_t clocks;
static uint8_t clockTms[4097];
static uint8_t clockTdi[4097];


static uint8_t simulateClock(uint8_t tms, uint8_t tdi)
{
  const size_t tick = clocks++;
  TEST_ASSERT_TRUE(tick < sizeof(clockTms) * 8);

  if (tms) {
    clockTms[tick / 8] |= uint8_t(1U << (tick % 8));
  }

  if (tdi) {
    clockTdi[tick / 8] |= uint8_t(1U << (tick % 8));
  }

  return 0;
}


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
    TEST_ASSERT_EQUAL_INT(pre[i], ((clockTms[(i) / 8] >> ((i) % 8)) & 1U));
    TEST_ASSERT_EQUAL_INT(0, ((clockTdi[(i) / 8] >> ((i) % 8)) & 1U));
  }

  for (size_t i = 0; i < count; ++i) {
    TEST_ASSERT_EQUAL_INT(input.getBit(i), ((clockTdi[(i + 5) / 8] >> ((i + 5) % 8)) & 1U));
    TEST_ASSERT_EQUAL_INT(i == count - 1, ((clockTms[(i + 5) / 8] >> ((i + 5) % 8)) & 1U));
  }

  TEST_ASSERT_EQUAL_INT(1, ((clockTms[(count + 5) / 8] >> ((count + 5) % 8)) & 1U));
  TEST_ASSERT_EQUAL_INT(0, ((clockTms[(count + 6) / 8] >> ((count + 6) % 8)) & 1U));
  TEST_ASSERT_EQUAL_INT(0, ((clockTdi[(count + 5) / 8] >> ((count + 5) % 8)) & 1U));
  TEST_ASSERT_EQUAL_INT(0, ((clockTdi[(count + 6) / 8] >> ((count + 6) % 8)) & 1U));
}

/**
 * @brief Check an IR transfer of count bits using a repeating 100 bit pattern.
 * Verifies success, the TMS/TDI trace and that the input buffer is unchanged.
 * The trace contains count + 7 clocks: five to enter Shift-IR, count to shift
 * the instruction, and two to pass through Update-IR back to Run-Test/Idle.
 * The first entry clock (TMS = 0) handles either Test-Logic-Reset or Run-Test/Idle.
 * The last data clock also exits Shift-IR (TMS = 1), so no extra exit clock is needed.
 */
static void check_ir_buffer(size_t count)
{
  Jtag jtag(1, 2, 3, 4, 5);
  BitBuffer<32767> input;
  TEST_ASSERT_TRUE(input.resize(count));

  for (size_t i = 0; i < count; ++i) {
    input.set(i, i % 3 == 0);
  }

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

static void test_ir_invalid_inputs_without_clocks()
{
  Jtag jtag(1, 2, 3, 4, 5);
  BitBuffer<> empty;
  TEST_ASSERT_EQUAL_INT(int(JTAG::ERROR::INVALID_BUFFER), int(jtag.ir(empty)));
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
  RUN_TEST(test_ir_invalid_inputs_without_clocks);
  return UNITY_END();
}
