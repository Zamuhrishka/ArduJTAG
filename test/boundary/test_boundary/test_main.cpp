#include <boundary/BoundaryScan.hpp>
#include <unity.h>

// Independent layout: two enable polarities and an internal cell initially 1.
struct Layout
{
  static constexpr size_t BoundaryLength = 9;
  enum class Pin { ActiveHigh, ActiveLow, InputOnly, OutputOnly, Missing,
                   BadInput, BadOutput, BadControl, Overlapping };
  static BoundaryTypes::Cells cells(Pin pin)
  {
    const auto none = BoundaryTypes::NoCell;
    switch (pin) {
      case Pin::ActiveHigh: return {0, 1, 2, 1, false};
      case Pin::ActiveLow: return {3, 4, 5, 2, true};
      case Pin::InputOnly: return {6, none, none, 3, false};
      case Pin::OutputOnly: return {none, 7, 5, 4, true};
      case Pin::BadInput: return {9, none, none, 5, false};
      case Pin::BadOutput: return {0, 9, 2, 6, false};
      case Pin::BadControl: return {0, 1, 9, 7, false};
      case Pin::Overlapping: return {0, 1, 1, 8, false};
      default: return {none, none, none, 0, false};
    }
  }
  static bool initialValue(size_t index) { return index == 4 || index == 5 || index == 8; }
};
using Boundary = BoundaryScan<Layout>;
using Pin = Layout::Pin;
void setUp() {}
void tearDown() {}

void test_initial_values_come_from_layout()
{
  const auto values = Boundary::initialValues();
  TEST_ASSERT_EQUAL_UINT32(9, values.bitCount());
  TEST_ASSERT_EQUAL_HEX8(0x30, values.byte(0));
  TEST_ASSERT_EQUAL_HEX8(0x01, values.byte(1));
}

void test_both_output_enable_polarities()
{
  auto values = Boundary::initialValues();
  TEST_ASSERT_TRUE(Boundary::setOutput(values, Pin::ActiveHigh, true));
  TEST_ASSERT_EQUAL_HEX8(0x36, values.byte(0));
  TEST_ASSERT_TRUE(Boundary::disableOutput(values, Pin::ActiveHigh));
  TEST_ASSERT_EQUAL_HEX8(0x32, values.byte(0));
  TEST_ASSERT_TRUE(Boundary::setOutput(values, Pin::ActiveLow, false));
  TEST_ASSERT_EQUAL_HEX8(0x02, values.byte(0));
  TEST_ASSERT_TRUE(Boundary::disableOutput(values, Pin::ActiveLow));
  TEST_ASSERT_EQUAL_HEX8(0x22, values.byte(0));
  TEST_ASSERT_EQUAL_HEX8(1, values.byte(1)); // Internal cell preserved.
}

void test_input_only_and_output_only_pins()
{
  auto values = Boundary::initialValues();
  TEST_ASSERT_TRUE(Boundary::setOutput(values, Pin::OutputOnly, true));
  TEST_ASSERT_TRUE(values.getBit(7));
  TEST_ASSERT_FALSE(values.getBit(5));
  TEST_ASSERT_TRUE(Boundary::disableOutput(values, Pin::OutputOnly));
  bool level = true;
  TEST_ASSERT_FALSE(Boundary::readInput(values, Pin::OutputOnly, level));
  TEST_ASSERT_TRUE(level);
  TEST_ASSERT_TRUE(Boundary::readInput(values, Pin::InputOnly, level));
  TEST_ASSERT_FALSE(level);
  values.set(6, true);
  TEST_ASSERT_TRUE(Boundary::readInput(values, Pin::InputOnly, level));
  TEST_ASSERT_TRUE(level);
  const auto before = values;
  TEST_ASSERT_FALSE(Boundary::setOutput(values, Pin::InputOnly, true));
  TEST_ASSERT_FALSE(Boundary::disableOutput(values, Pin::InputOnly));
  TEST_ASSERT_EQUAL_HEX8_ARRAY(before.data(), values.data(), values.byteCount());
}

void test_invalid_mapping_does_not_partially_modify_values()
{
  auto values = Boundary::initialValues();
  const auto before = values;
  const Pin invalid[] = {Pin::Missing, Pin::BadOutput, Pin::BadControl, Pin::Overlapping};
  for (auto pin : invalid) {
    TEST_ASSERT_FALSE(Boundary::setOutput(values, pin, true));
    TEST_ASSERT_FALSE(Boundary::disableOutput(values, pin));
    TEST_ASSERT_EQUAL_HEX8_ARRAY(before.data(), values.data(), values.byteCount());
  }
  bool level = true;
  TEST_ASSERT_FALSE(Boundary::readInput(values, Pin::BadInput, level));
  TEST_ASSERT_FALSE(Boundary::readInput(values, Pin::Missing, level));
  TEST_ASSERT_TRUE(level);
}

void test_wrong_active_lengths_preserve_data()
{
  BitBuffer<16> values;
  const size_t lengths[] = {0, 8, 10};
  for (size_t length : lengths) {
    if (length) values.resize(length);
    values.set(0, true);
    const auto before = values;
    bool level = true;
    TEST_ASSERT_FALSE(Boundary::setOutput(values, Pin::ActiveHigh, false));
    TEST_ASSERT_FALSE(Boundary::disableOutput(values, Pin::ActiveLow));
    TEST_ASSERT_FALSE(Boundary::readInput(values, Pin::InputOnly, level));
    TEST_ASSERT_TRUE(level);
    TEST_ASSERT_EQUAL_UINT32(length, values.bitCount());
    TEST_ASSERT_EQUAL_HEX8_ARRAY(before.data(), values.data(), 2);
  }
  values.resize(9); // Larger capacity is fine when the active length matches.
  TEST_ASSERT_TRUE(Boundary::setOutput(values, Pin::ActiveHigh, true));
}

int main()
{
  UNITY_BEGIN();
  RUN_TEST(test_initial_values_come_from_layout);
  RUN_TEST(test_both_output_enable_polarities);
  RUN_TEST(test_input_only_and_output_only_pins);
  RUN_TEST(test_invalid_mapping_does_not_partially_modify_values);
  RUN_TEST(test_wrong_active_lengths_preserve_data);
  return UNITY_END();
}
