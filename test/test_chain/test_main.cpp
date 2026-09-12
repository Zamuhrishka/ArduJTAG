#include <JtagChain.hpp>
#include <unity.h>
#include <string.h>

static size_t clocks;
static uint8_t tmsTrace[128], tdiTrace[128];
static size_t responseStart;
static size_t responseOffset;
static const uint8_t responseBytes[] = {0xA5, 0x03};
GpioPin::GpioPin(int, int) {}
JtagGpio::JtagGpio(GpioPin a, GpioPin b, GpioPin c, GpioPin d, GpioPin e)
  : _tms(a), _tdi(b), _tdo(c), _tck(d), _rst(e) {}
uint8_t JtagGpio::clock(uint8_t tms, uint8_t tdi)
{
  TEST_ASSERT_TRUE(clocks < sizeof(tmsTrace));
  tmsTrace[clocks] = tms;
  tdiTrace[clocks] = tdi;
  const size_t tick = clocks++;
  if (tick < responseStart + responseOffset) return 0;
  const size_t bit = tick - responseStart - responseOffset;
  return bit < 10 ? ((responseBytes[bit / 8] >> (bit % 8)) & 1) : 0;
}
void setUp()
{
  clocks = 0;
  responseStart = 1000;
  responseOffset = 0;
  memset(tmsTrace, 0, sizeof(tmsTrace));
  memset(tdiTrace, 0, sizeof(tdiTrace));
}
void tearDown() {}

static void check_transfer(size_t target)
{
  Jtag jtag(1, 2, 3, 4, 5);
  JtagChain<3, 16> chain(jtag);
  const size_t widths[] = {3, 5, 4};
  for (size_t i = 0; i < 3; ++i) TEST_ASSERT_TRUE(chain.add(JtagDevice(widths[i])));
  auto instruction = BitBuffer<5>::fromBytes({0x02}, widths[target]);
  auto input = BitBuffer<10>::fromBytes({0x69, 0x02}, 10);
  const auto before = input;
  responseStart = 12 + 7 + 3;
  responseOffset = 2 - target;
  // Exercise in-place response capture as well as both chain ends.
  TEST_ASSERT_EQUAL_INT(0, int(chain.transfer(target, instruction, input, input)));
  TEST_ASSERT_EQUAL_UINT32(36, clocks);
  size_t tick = 5;
  for (size_t pos = 3; pos > 0; --pos) {
    const size_t device = pos - 1;
    for (size_t bit = 0; bit < widths[device]; ++bit) {
      TEST_ASSERT_EQUAL_UINT8(device == target ? instruction.getBit(bit) : 1, tdiTrace[tick]);
      TEST_ASSERT_EQUAL_UINT8(tick == 16, tmsTrace[tick]);
      ++tick;
    }
  }
  for (size_t bit = 0; bit < 12; ++bit) {
    const bool expected = bit >= responseOffset && bit - responseOffset < 10
                            ? before.getBit(bit - responseOffset) : false;
    TEST_ASSERT_EQUAL_UINT8(expected, tdiTrace[responseStart + bit]);
    TEST_ASSERT_EQUAL_UINT8(bit == 11, tmsTrace[responseStart + bit]);
  }
  TEST_ASSERT_EQUAL_UINT32(10, input.bitCount());
  TEST_ASSERT_EQUAL_HEX8_ARRAY(responseBytes, input.data(), 2);
  // A second transfer must load IR again, without assuming cached instructions.
  clocks = 0;
  TEST_ASSERT_EQUAL_INT(0, int(chain.transfer(target, instruction, before, input)));
  TEST_ASSERT_EQUAL_UINT32(36, clocks);
}
void test_target_near_tdi() { check_transfer(0); }
void test_target_middle() { check_transfer(1); }
void test_target_near_tdo() { check_transfer(2); }

void test_single_device()
{
  Jtag jtag(1, 2, 3, 4, 5);
  JtagChain<1, 10> chain(jtag);
  TEST_ASSERT_TRUE(chain.add(JtagDevice(4)));
  auto instruction = BitBuffer<4>::fromBits("0100");
  auto input = BitBuffer<10>::fromBytes({0x69, 0x02}, 10);
  BitBuffer<10> output;
  responseStart = 14;
  TEST_ASSERT_EQUAL_INT(0, int(chain.transfer(0, instruction, input, output)));
  TEST_ASSERT_EQUAL_UINT32(26, clocks);
  TEST_ASSERT_EQUAL_HEX8_ARRAY(responseBytes, output.data(), 2);
}

void test_validation_without_clocks()
{
  Jtag jtag(1, 2, 3, 4, 5);
  JtagChain<2, 8> chain(jtag);
  auto instruction = BitBuffer<4>::fromBits("0100");
  auto input = BitBuffer<8>::fromBytes({0xAA});
  auto output = BitBuffer<8>::fromBytes({0xCC});
  TEST_ASSERT_EQUAL_INT(int(JTAG::ERROR::INVALID_DEVICE), int(chain.transfer(0, instruction, input, output)));
  TEST_ASSERT_FALSE(chain.add(JtagDevice()));
  TEST_ASSERT_FALSE(chain.add(JtagDevice(9)));
  TEST_ASSERT_TRUE(chain.add(JtagDevice(4)));
  TEST_ASSERT_FALSE(chain.add(JtagDevice(5)));
  TEST_ASSERT_EQUAL_UINT32(1, chain.deviceCount());
  TEST_ASSERT_TRUE(chain.add(JtagDevice(4)));
  TEST_ASSERT_FALSE(chain.add(JtagDevice(1)));
  TEST_ASSERT_EQUAL_INT(int(JTAG::ERROR::INVALID_SEQUENCE_LEN), int(chain.transfer(0, instruction, input, output)));
  input.resize(7);
  BitBuffer<4> empty;
  TEST_ASSERT_EQUAL_INT(int(JTAG::ERROR::INVALID_BUFFER), int(chain.transfer(0, empty, input, output)));
  auto wrong = BitBuffer<3>::fromBits("010");
  TEST_ASSERT_EQUAL_INT(int(JTAG::ERROR::INVALID_BUFFER), int(chain.transfer(0, wrong, input, output)));
  TEST_ASSERT_EQUAL_INT(int(JTAG::ERROR::INVALID_BUFFER), int(chain.transfer(0, instruction, empty, output)));
  TEST_ASSERT_EQUAL_INT(int(JTAG::ERROR::INVALID_BUFFER), int(chain.transfer(0, instruction, input, empty)));
  TEST_ASSERT_EQUAL_INT(int(JTAG::ERROR::INVALID_DEVICE), int(chain.transfer(2, instruction, input, output)));
  TEST_ASSERT_EQUAL_UINT32(0, clocks);
  TEST_ASSERT_EQUAL_UINT32(8, output.bitCount());
  TEST_ASSERT_EQUAL_HEX8(0xCC, output.byte(0));
  // Exactly eight combined DR bits fit.
  TEST_ASSERT_EQUAL_INT(0, int(chain.transfer(0, instruction, input, output)));
}

int main()
{
  UNITY_BEGIN();
  RUN_TEST(test_target_near_tdi);
  RUN_TEST(test_target_middle);
  RUN_TEST(test_target_near_tdo);
  RUN_TEST(test_single_device);
  RUN_TEST(test_validation_without_clocks);
  return UNITY_END();
}
