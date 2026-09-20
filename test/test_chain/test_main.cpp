#include <JtagChain.hpp>
#include <profiles/ArmJtagDp.hpp>
#include <unity.h>
#include <string.h>

static size_t clocks;
static uint8_t tmsTrace[128], tdiTrace[128];
static size_t responseStart;
static size_t responseOffset;
static const uint8_t responseBytes[] = {0xA5, 0x03, 0xCD, 0x89};
static size_t responseBits;
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
  return bit < responseBits ? ((responseBytes[bit / 8] >> (bit % 8)) & 1) : 0;
}
void setUp()
{
  clocks = 0;
  responseStart = 1000;
  responseOffset = 0;
  responseBits = 10;
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

// A different profile with the same IR length must not be accepted as ARM.
struct OtherProfile
{
  static constexpr size_t IrLength = 4;
  enum class Instruction { Read };
  static constexpr size_t drLength(Instruction) { return 0; } // Variable width.
  static BitBuffer<4> encode(Instruction) { return BitBuffer<4>::fromBits("1000"); }
};
template <>
struct JtagInstructionProfile<OtherProfile::Instruction> : OtherProfile {};

void test_named_instructions_match_wire_codes()
{
  Jtag jtag(1, 2, 3, 4, 5);
  JtagChain<2, 40> chain(jtag);
  TEST_ASSERT_TRUE(chain.add(JtagDevice(5)));
  TEST_ASSERT_TRUE(chain.add(JtagDevice::fromProfile<ArmJtagDp>()));
  const ArmJtagDp::Instruction commands[] = {
    ArmJtagDp::Instruction::Abort, ArmJtagDp::Instruction::Dpacc,
    ArmJtagDp::Instruction::Apacc, ArmJtagDp::Instruction::Idcode,
    ArmJtagDp::Instruction::Bypass
  };
  const uint8_t codes[] = {0x8, 0xA, 0xB, 0xE, 0xF};
  const size_t widths[] = {35, 35, 35, 32, 1};
  for (size_t command = 0; command < 5; ++command) {
    setUp();
    BitBuffer<35> input, output;
    input.resize(widths[command]);
    responseStart = 19;
    TEST_ASSERT_EQUAL_INT(0, int(chain.transfer(1, commands[command], input, output)));
    for (size_t bit = 0; bit < 4; ++bit) {
      TEST_ASSERT_EQUAL_UINT8((codes[command] >> bit) & 1, tdiTrace[5 + bit]);
    }
    for (size_t bit = 4; bit < 9; ++bit) TEST_ASSERT_EQUAL_UINT8(1, tdiTrace[5 + bit]);
    const size_t expectedClocks = clocks;
    uint8_t expectedTms[128], expectedTdi[128];
    memcpy(expectedTms, tmsTrace, sizeof(tmsTrace));
    memcpy(expectedTdi, tdiTrace, sizeof(tdiTrace));
    const auto expectedOutput = output;
    const auto raw = BitBuffer<4>::fromBytes({codes[command]}, 4);
    clocks = 0;
    TEST_ASSERT_EQUAL_INT(0, int(chain.transfer(1, raw, input, output)));
    TEST_ASSERT_EQUAL_UINT32(expectedClocks, clocks);
    TEST_ASSERT_EQUAL_UINT8_ARRAY(expectedTms, tmsTrace, clocks);
    TEST_ASSERT_EQUAL_UINT8_ARRAY(expectedTdi, tdiTrace, clocks);
    TEST_ASSERT_EQUAL_UINT32(widths[command], output.bitCount());
    TEST_ASSERT_EQUAL_HEX8_ARRAY(expectedOutput.data(), output.data(), output.byteCount());
  }
}

void test_profiles_reject_wrong_device_and_unknown_commands()
{
  Jtag jtag(1, 2, 3, 4, 5);
  JtagChain<3, 40> chain(jtag);
  TEST_ASSERT_TRUE(chain.add(JtagDevice(4)));
  TEST_ASSERT_TRUE(chain.add(JtagDevice::fromProfile<OtherProfile>()));
  TEST_ASSERT_TRUE(chain.add(JtagDevice::fromProfile<ArmJtagDp>()));
  const auto input = BitBuffer<8>::fromBytes({0x55});
  auto output = BitBuffer<8>::fromBytes({0xCC});
  for (size_t target = 0; target < 2; ++target) {
    TEST_ASSERT_EQUAL_INT(int(JTAG::ERROR::INVALID_DEVICE),
      int(chain.transfer(target, ArmJtagDp::Instruction::Idcode, input, output)));
  }
  TEST_ASSERT_EQUAL_INT(int(JTAG::ERROR::INVALID_DEVICE),
    int(chain.transfer(3, ArmJtagDp::Instruction::Idcode, input, output)));
  TEST_ASSERT_EQUAL_INT(int(JTAG::ERROR::INVALID_INSTRUCTION),
    int(chain.transfer(2, static_cast<ArmJtagDp::Instruction>(0x9), input, output)));
  TEST_ASSERT_EQUAL_INT(int(JTAG::ERROR::INVALID_DEVICE),
    int(chain.transfer(2, OtherProfile::Instruction::Read, input, output)));
  TEST_ASSERT_EQUAL_UINT32(0, clocks);
  TEST_ASSERT_EQUAL_UINT32(8, output.bitCount());
  TEST_ASSERT_EQUAL_HEX8(0xCC, output.byte(0));
  TEST_ASSERT_EQUAL_INT(0, int(chain.transfer(1, OtherProfile::Instruction::Read, input, output)));
}

void test_named_dr_lengths_are_checked_before_clocks()
{
  Jtag jtag(1, 2, 3, 4, 5);
  JtagChain<1, 40> chain(jtag);
  chain.add(JtagDevice::fromProfile<ArmJtagDp>());
  const ArmJtagDp::Instruction commands[] = {
    ArmJtagDp::Instruction::Abort, ArmJtagDp::Instruction::Dpacc,
    ArmJtagDp::Instruction::Apacc, ArmJtagDp::Instruction::Idcode,
    ArmJtagDp::Instruction::Bypass
  };
  const size_t widths[] = {35, 35, 35, 32, 1};
  auto output = BitBuffer<40>::fromBytes({0xCC});
  for (size_t command = 0; command < 5; ++command) {
    TEST_ASSERT_EQUAL_UINT32(widths[command], ArmJtagDp::drLength(commands[command]));
    for (size_t delta = 0; delta < 2; ++delta) {
      BitBuffer<40> input;
      const size_t length = widths[command] - 1 + 2 * delta;
      if (length) input.resize(length);
      TEST_ASSERT_EQUAL_INT(int(JTAG::ERROR::INVALID_BUFFER),
        int(chain.transfer(0, commands[command], input, output)));
      TEST_ASSERT_EQUAL_UINT32(0, clocks);
      TEST_ASSERT_EQUAL_UINT32(8, output.bitCount());
      TEST_ASSERT_EQUAL_HEX8(0xCC, output.byte(0));
    }
  }
  // Raw instructions still allow explicitly sized DR transfers.
  const auto instruction = ArmJtagDp::encode(ArmJtagDp::Instruction::Idcode);
  const auto input = BitBuffer<8>::fromBytes({0});
  TEST_ASSERT_EQUAL_INT(0, int(chain.transfer(0, instruction, input, output)));
}

void test_read_idcode_extracts_uint32_at_both_chain_ends()
{
  Jtag jtag(1, 2, 3, 4, 5);
  JtagChain<2, 33> chain(jtag);
  chain.add(JtagDevice::fromProfile<ArmJtagDp>());
  chain.add(JtagDevice::fromProfile<ArmJtagDp>());
  for (size_t target = 0; target < 2; ++target) {
    setUp();
    responseStart = 18;
    responseOffset = 1 - target;
    responseBits = 32;
    uint32_t id = 0;
    TEST_ASSERT_EQUAL_INT(0, int(chain.device<ArmJtagDp>(target).readIdcode(id)));
    TEST_ASSERT_EQUAL_HEX32(0x89CD03A5UL, id);
    TEST_ASSERT_EQUAL_UINT32(53, clocks);
    const size_t irOffset = 5 + (1 - target) * 4;
    for (size_t bit = 0; bit < 4; ++bit) {
      TEST_ASSERT_EQUAL_UINT8((0xE >> bit) & 1, tdiTrace[irOffset + bit]);
    }
    for (size_t bit = 0; bit < 33; ++bit) TEST_ASSERT_EQUAL_UINT8(0, tdiTrace[18 + bit]);
  }
}

void test_read_idcode_failure_preserves_value()
{
  Jtag jtag(1, 2, 3, 4, 5);
  JtagChain<2, 32> chain(jtag);
  chain.add(JtagDevice(4));
  chain.add(JtagDevice::fromProfile<ArmJtagDp>());
  uint32_t id = 0xDEADBEEFUL;
  TEST_ASSERT_EQUAL_INT(int(JTAG::ERROR::INVALID_DEVICE), int(chain.device<ArmJtagDp>(0).readIdcode(id)));
  TEST_ASSERT_EQUAL_INT(int(JTAG::ERROR::INVALID_DEVICE), int(chain.device<ArmJtagDp>(2).readIdcode(id)));
  TEST_ASSERT_EQUAL_INT(int(JTAG::ERROR::INVALID_SEQUENCE_LEN), int(chain.device<ArmJtagDp>(1).readIdcode(id)));
  TEST_ASSERT_EQUAL_UINT32(0, clocks);
  TEST_ASSERT_EQUAL_HEX32(0xDEADBEEFUL, id);
}

void test_device_access_validates_and_preserves_index()
{
  Jtag jtag(1, 2, 3, 4, 5);
  JtagChain<3, 40> chain(jtag);
  auto device = chain.device<OtherProfile>(0); // This profile has no IDCODE.
  auto wrongProfile = chain.device<ArmJtagDp>(0);
  auto missing = chain.device<OtherProfile>(2);
  TEST_ASSERT_FALSE(device.valid());
  const auto input = BitBuffer<8>::fromBytes({0x55});
  auto output = BitBuffer<8>::fromBytes({0xCC});
  TEST_ASSERT_EQUAL_INT(int(JTAG::ERROR::INVALID_DEVICE),
    int(device.transfer(OtherProfile::Instruction::Read, input, output)));
  chain.add(JtagDevice::fromProfile<OtherProfile>());
  TEST_ASSERT_TRUE(device.valid());
  TEST_ASSERT_FALSE(wrongProfile.valid());
  TEST_ASSERT_FALSE(missing.valid());
  TEST_ASSERT_EQUAL_INT(int(JTAG::ERROR::INVALID_DEVICE),
    int(wrongProfile.transfer(ArmJtagDp::Instruction::Idcode, input, output)));
  TEST_ASSERT_EQUAL_INT(int(JTAG::ERROR::INVALID_DEVICE),
    int(missing.transfer(OtherProfile::Instruction::Read, input, output)));
  TEST_ASSERT_EQUAL_UINT32(0, clocks);
  TEST_ASSERT_EQUAL_HEX8(0xCC, output.byte(0));
  chain.add(JtagDevice(5));
  auto copy = device;
  TEST_ASSERT_TRUE(copy.valid());
  TEST_ASSERT_EQUAL_INT(0, int(copy.transfer(OtherProfile::Instruction::Read, input, output)));
  // Device 0 is still behind the five-bit BYPASS instruction in transmission order.
  for (size_t bit = 0; bit < 5; ++bit) TEST_ASSERT_EQUAL_UINT8(1, tdiTrace[5 + bit]);
  TEST_ASSERT_EQUAL_UINT8(1, tdiTrace[10]);
  for (size_t bit = 1; bit < 4; ++bit) TEST_ASSERT_EQUAL_UINT8(0, tdiTrace[10 + bit]);
}

// Synthetic boundary-scan profile: these opcodes do not describe real hardware.
struct BoundaryProfile
{
  static constexpr size_t IrLength = 4;
  enum class Instruction : uint8_t {
    Extest = 0, Sample = 2, Preload = 2, SamplePreload = 2,
    Intest = 3, HighZ = 4, Bypass = 15, Unsupported = 7
  };
  static constexpr size_t drLength(Instruction instruction)
  {
    return instruction == Instruction::Bypass || instruction == Instruction::HighZ ? 1 : 10;
  }
  static BitBuffer<4> encode(Instruction instruction)
  {
    if (instruction == Instruction::Unsupported) return BitBuffer<4>();
    return BitBuffer<4>::fromBytes({static_cast<uint8_t>(instruction)}, 4);
  }
};
template <>
struct JtagInstructionProfile<BoundaryProfile::Instruction> : BoundaryProfile {};

void test_boundary_modes_generate_only_ir_clocks()
{
  Jtag jtag(1, 2, 3, 4, 5);
  JtagChain<2, 16> chain(jtag);
  chain.add(JtagDevice(5));
  chain.add(JtagDevice::fromProfile<BoundaryProfile>());
  auto device = chain.device<BoundaryProfile>(1);
  TEST_ASSERT_EQUAL_INT(0, int(device.bypass()));
  TEST_ASSERT_EQUAL_UINT32(16, clocks);
  for (size_t bit = 0; bit < 9; ++bit) TEST_ASSERT_EQUAL_UINT8(1, tdiTrace[5 + bit]);
  setUp();
  TEST_ASSERT_EQUAL_INT(0, int(device.highZ()));
  TEST_ASSERT_EQUAL_UINT32(16, clocks);
  for (size_t bit = 0; bit < 4; ++bit) TEST_ASSERT_EQUAL_UINT8((4 >> bit) & 1, tdiTrace[5 + bit]);
  for (size_t bit = 4; bit < 9; ++bit) TEST_ASSERT_EQUAL_UINT8(1, tdiTrace[5 + bit]);
  TEST_ASSERT_EQUAL_UINT8(1, tmsTrace[13]); // Last IR bit exits Shift-IR.
  TEST_ASSERT_EQUAL_UINT8(1, tmsTrace[14]);
  TEST_ASSERT_EQUAL_UINT8(0, tmsTrace[15]);

  // ARM JTAG-DP supports BYPASS without any boundary-scan instructions.
  JtagChain<1, 4> armChain(jtag);
  armChain.add(JtagDevice::fromProfile<ArmJtagDp>());
  setUp();
  TEST_ASSERT_EQUAL_INT(0, int(armChain.device<ArmJtagDp>(0).bypass()));
  TEST_ASSERT_EQUAL_UINT32(11, clocks);
}

void test_boundary_methods_exchange_supplied_values()
{
  Jtag jtag(1, 2, 3, 4, 5);
  JtagChain<2, 16> chain(jtag);
  chain.add(JtagDevice(5));
  chain.add(JtagDevice::fromProfile<BoundaryProfile>());
  auto device = chain.device<BoundaryProfile>(1);
  const auto values = BitBuffer<10>::fromBytes({0x69, 0x02}, 10);
  const uint8_t codes[] = {2, 2, 2, 0, 3};
  for (size_t operation = 0; operation < 5; ++operation) {
    setUp();
    responseStart = 19;
    BitBuffer<10> captured;
    JTAG::ERROR status = JTAG::ERROR::INVALID_INSTRUCTION;
    switch (operation) {
      case 0: status = device.sample(values, captured); break;
      case 1: status = device.preload(values); break;
      case 2: status = device.samplePreload(values, captured); break;
      case 3: status = device.extest(values, captured); break;
      case 4: status = device.intest(values, captured); break;
    }
    TEST_ASSERT_EQUAL_INT(0, int(status));
    TEST_ASSERT_EQUAL_UINT32(32, clocks);
    for (size_t bit = 0; bit < 4; ++bit) {
      TEST_ASSERT_EQUAL_UINT8((codes[operation] >> bit) & 1, tdiTrace[5 + bit]);
    }
    for (size_t bit = 0; bit < 10; ++bit) {
      TEST_ASSERT_EQUAL_UINT8(values.getBit(bit), tdiTrace[19 + bit]);
    }
    TEST_ASSERT_EQUAL_UINT8(0, tdiTrace[29]); // BYPASS padding.
    TEST_ASSERT_EQUAL_UINT8(1, tmsTrace[29]);
    if (operation != 1) {
      TEST_ASSERT_EQUAL_UINT32(10, captured.bitCount());
      TEST_ASSERT_EQUAL_HEX8_ARRAY(responseBytes, captured.data(), 2);
    }
  }
}

void test_boundary_validation_without_clocks()
{
  Jtag jtag(1, 2, 3, 4, 5);
  JtagChain<1, 16> chain(jtag);
  chain.add(JtagDevice::fromProfile<BoundaryProfile>());
  auto device = chain.device<BoundaryProfile>(0);
  auto missing = chain.device<BoundaryProfile>(1);
  auto wrong = chain.device<ArmJtagDp>(0);
  TEST_ASSERT_EQUAL_INT(int(JTAG::ERROR::INVALID_DEVICE), int(missing.highZ()));
  TEST_ASSERT_EQUAL_INT(int(JTAG::ERROR::INVALID_DEVICE), int(wrong.bypass()));
  TEST_ASSERT_EQUAL_INT(int(JTAG::ERROR::INVALID_INSTRUCTION),
    int(device.select(BoundaryProfile::Instruction::Unsupported)));
  const auto values = BitBuffer<8>::fromBytes({0x55});
  auto captured = BitBuffer<10>::fromBytes({0xCC});
  TEST_ASSERT_EQUAL_INT(int(JTAG::ERROR::INVALID_BUFFER), int(device.sample(values, captured)));
  TEST_ASSERT_EQUAL_INT(int(JTAG::ERROR::INVALID_BUFFER), int(device.preload(values)));
  TEST_ASSERT_EQUAL_INT(int(JTAG::ERROR::INVALID_BUFFER), int(device.samplePreload(values, captured)));
  TEST_ASSERT_EQUAL_INT(int(JTAG::ERROR::INVALID_BUFFER), int(device.extest(values, captured)));
  TEST_ASSERT_EQUAL_INT(int(JTAG::ERROR::INVALID_BUFFER), int(device.intest(values, captured)));
  TEST_ASSERT_EQUAL_UINT32(0, clocks);
  TEST_ASSERT_EQUAL_UINT32(8, captured.bitCount());
  TEST_ASSERT_EQUAL_HEX8(0xCC, captured.byte(0));
}

int main()
{
  UNITY_BEGIN();
  RUN_TEST(test_boundary_modes_generate_only_ir_clocks);
  RUN_TEST(test_boundary_methods_exchange_supplied_values);
  RUN_TEST(test_boundary_validation_without_clocks);
  RUN_TEST(test_device_access_validates_and_preserves_index);
  RUN_TEST(test_named_dr_lengths_are_checked_before_clocks);
  RUN_TEST(test_read_idcode_extracts_uint32_at_both_chain_ends);
  RUN_TEST(test_read_idcode_failure_preserves_value);
  RUN_TEST(test_named_instructions_match_wire_codes);
  RUN_TEST(test_profiles_reject_wrong_device_and_unknown_commands);
  RUN_TEST(test_target_near_tdi);
  RUN_TEST(test_target_middle);
  RUN_TEST(test_target_near_tdo);
  RUN_TEST(test_single_device);
  RUN_TEST(test_validation_without_clocks);
  return UNITY_END();
}
