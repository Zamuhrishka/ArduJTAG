#include <SimulatedGpio.hpp>
#include <chain/JtagChain.hpp>
#include <boundary/BoundaryScan.hpp>
#include <profiles/ArmJtagDp.hpp>
#include <profiles/Stm32F405_415_407_417Lqfp100Boundary.hpp>
#include <unity.h>
#include <string.h>

using Profile = Stm32F405_415_407_417Lqfp100;
using Layout = Stm32F405_415_407_417Lqfp100Boundary;
using Boundary = BoundaryScan<Layout>;
using Pin = Boundary::Pin;
using Instruction = Profile::Instruction;

static size_t clocks;
static uint8_t sent[512];
static bool readingId;
static uint8_t simulateClock(uint8_t, uint8_t tdi)
{
  if (clocks >= sizeof(sent)) { TEST_FAIL_MESSAGE("Unexpected clock count"); return 0; }
  sent[clocks] = tdi;
  const size_t tick = clocks++;
  // 9 IR bits + 7 transitions, then 3 DR transitions and Debug TAP BYPASS.
  if (tick < 20) return 0;
  const size_t index = tick - 20;
  if (readingId) return index < 32 ? ((0x36413041UL >> index) & 1U) : 0;
  return index < 406 && index % 7 == 0;
}
void setUp() { clocks = 0; readingId = false; memset(sent, 0, sizeof(sent)); }
void tearDown() {}

void test_instruction_codes_and_widths()
{
  const Instruction instructions[] = {Instruction::Extest, Instruction::Idcode,
    Instruction::Sample, Instruction::Preload, Instruction::SamplePreload, Instruction::Bypass};
  const uint8_t codes[] = {0, 1, 2, 2, 2, 31};
  const size_t widths[] = {406, 32, 406, 406, 406, 1};
  for (size_t i = 0; i < 6; ++i) {
    const auto encoded = Profile::encode(instructions[i]);
    TEST_ASSERT_TRUE(encoded.valid());
    TEST_ASSERT_EQUAL_UINT32(5, encoded.bitCount());
    TEST_ASSERT_EQUAL_HEX8(codes[i], encoded.byte(0));
    TEST_ASSERT_EQUAL_UINT32(widths[i], Profile::drLength(instructions[i]));
  }
  for (uint8_t code = 0; code < 32; ++code) {
    if (code == 0 || code == 1 || code == 2 || code == 31) continue;
    TEST_ASSERT_FALSE(Profile::encode(static_cast<Instruction>(code)).valid());
    TEST_ASSERT_EQUAL_UINT32(0, Profile::drLength(static_cast<Instruction>(code)));
  }
}

void test_identity_and_compliance_metadata()
{
  for (uint32_t revision = 0; revision < 16; ++revision)
    TEST_ASSERT_TRUE(Profile::matchesIdcode((revision << 28) | 0x06413041UL));
  TEST_ASSERT_FALSE(Profile::matchesIdcode(0x4BA00477UL));
  TEST_ASSERT_FALSE(Profile::matchesIdcode(0x06413040UL));
  TEST_ASSERT_FALSE(Profile::matchesIdcode(0x06412041UL));
  TEST_ASSERT_EQUAL_HEX8(1, Profile::InstructionCaptureValue);
  TEST_ASSERT_EQUAL_HEX8(3, Profile::InstructionCaptureMask);
  TEST_ASSERT_EQUAL_UINT32(10000000, Profile::MaxTckHz);
  TEST_ASSERT_TRUE(Profile::RequiresNrstLow);
}

void test_cell_mapping_and_initial_vector()
{
  const auto pd12 = Layout::cells(Pin::Pd12);
  TEST_ASSERT_EQUAL_UINT16(166, pd12.input);
  TEST_ASSERT_EQUAL_UINT16(167, pd12.output);
  TEST_ASSERT_EQUAL_UINT16(168, pd12.control);
  TEST_ASSERT_EQUAL_UINT8(59, pd12.packagePin);
  TEST_ASSERT_EQUAL_UINT16(405, Layout::cells(Pin::Pe2).control);
  TEST_ASSERT_EQUAL_UINT16(12, Layout::cells(Pin::Pe1).input);
  TEST_ASSERT_EQUAL_UINT16(24, Layout::cells(Pin::Boot0).input);
  TEST_ASSERT_EQUAL_UINT8(94, Layout::cells(Pin::Boot0).packagePin);
  TEST_ASSERT_FALSE(Layout::cells(Pin::Boot0).hasOutput());
  TEST_ASSERT_FALSE(Layout::cells(Pin::Count).valid());

  bool usedCells[406] = {}, usedPins[101] = {};
  auto values = Boundary::initialValues();
  TEST_ASSERT_EQUAL_UINT32(406, values.bitCount());
  TEST_ASSERT_EQUAL_UINT32(78, static_cast<uint8_t>(Pin::Count));
  size_t outputs = 0, ones = 0;
  for (uint8_t i = 0; i < static_cast<uint8_t>(Pin::Count); ++i) {
    const auto cell = Layout::cells(static_cast<Pin>(i));
    TEST_ASSERT_TRUE(cell.valid());
    TEST_ASSERT_TRUE(cell.input < 406);
    TEST_ASSERT_FALSE(usedCells[cell.input]);
    usedCells[cell.input] = true;
    TEST_ASSERT_FALSE(values.getBit(cell.input));
    TEST_ASSERT_TRUE(cell.packagePin > 0 && cell.packagePin <= 100);
    TEST_ASSERT_FALSE(usedPins[cell.packagePin]);
    usedPins[cell.packagePin] = true;
    if (cell.hasOutput()) {
      ++outputs;
      TEST_ASSERT_TRUE(cell.output < 406 && cell.control < 406);
      TEST_ASSERT_FALSE(usedCells[cell.output]);
      usedCells[cell.output] = true;
      TEST_ASSERT_FALSE(usedCells[cell.control]);
      usedCells[cell.control] = true;
      TEST_ASSERT_FALSE(values.getBit(cell.output));
      TEST_ASSERT_TRUE(values.getBit(cell.control));
    }
  }
  for (size_t i = 0; i < 406; ++i) {
    if (values.getBit(i)) ++ones;
    if (!usedCells[i]) TEST_ASSERT_FALSE(values.getBit(i));
  }
  TEST_ASSERT_EQUAL_UINT32(77, outputs);
  TEST_ASSERT_EQUAL_UINT32(77, ones);
}

void test_pin_helpers_and_validation()
{
  auto values = Boundary::initialValues();
  const auto initial = values;
  TEST_ASSERT_TRUE(Boundary::setOutput(values, Pin::Pd12, true));
  TEST_ASSERT_TRUE(values.getBit(167));
  TEST_ASSERT_FALSE(values.getBit(168));
  for (size_t i = 0; i < 406; ++i)
    if (i != 167 && i != 168) TEST_ASSERT_EQUAL_INT(initial.getBit(i), values.getBit(i));
  TEST_ASSERT_TRUE(Boundary::disableOutput(values, Pin::Pd12));
  TEST_ASSERT_TRUE(values.getBit(168));
  TEST_ASSERT_TRUE(values.getBit(167));
  TEST_ASSERT_TRUE(Boundary::setOutput(values, Pin::Pd12, false));
  TEST_ASSERT_FALSE(values.getBit(167));
  const auto before = values;
  TEST_ASSERT_FALSE(Boundary::setOutput(values, Pin::Boot0, true));
  TEST_ASSERT_FALSE(Boundary::disableOutput(values, Pin::Count));
  TEST_ASSERT_EQUAL_HEX8_ARRAY(before.data(), values.data(), values.byteCount());
  bool level = false;
  values.set(24, true);
  TEST_ASSERT_TRUE(Boundary::readInput(values, Pin::Boot0, level));
  TEST_ASSERT_TRUE(level);
  auto shortVector = BitBuffer<405>::fromBits("1");
  TEST_ASSERT_FALSE(Boundary::setOutput(shortVector, Pin::Pd12, true));
  TEST_ASSERT_FALSE(Boundary::disableOutput(shortVector, Pin::Pd12));
  TEST_ASSERT_FALSE(Boundary::readInput(shortVector, Pin::Pd12, level));
  TEST_ASSERT_TRUE(level);
  TEST_ASSERT_FALSE(Boundary::readInput(values, static_cast<Pin>(255), level));
  TEST_ASSERT_TRUE(level);
}

void test_full_boundary_scan_with_debug_bypass()
{
  Jtag jtag(1, 2, 3, 4, 5);
  JtagChain<2, 407> chain(jtag);
  TEST_ASSERT_TRUE(chain.add(JtagDevice::fromProfile<Profile>()));
  TEST_ASSERT_TRUE(chain.add(JtagDevice::fromProfile<ArmJtagDp>()));
  auto input = Boundary::initialValues();
  Boundary::Values output;
  TEST_ASSERT_EQUAL_INT(0, int(chain.device<Profile>(0).sample(input, output)));
  TEST_ASSERT_EQUAL_UINT32(428, clocks);
  // Debug TAP BYPASS is sent first; Boundary TAP SAMPLE follows, LSB first.
  const uint8_t expectedIr[] = {1, 1, 1, 1, 0, 1, 0, 0, 0};
  TEST_ASSERT_EQUAL_UINT8_ARRAY(expectedIr, sent + 5, 9);
  TEST_ASSERT_EQUAL_UINT8(0, sent[19]);
  TEST_ASSERT_EQUAL_UINT32(406, output.bitCount());
  for (size_t i = 0; i < 406; ++i) {
    TEST_ASSERT_EQUAL_INT(input.getBit(i), sent[20 + i]);
    TEST_ASSERT_EQUAL_INT(i % 7 == 0, output.getBit(i));
  }
  TEST_ASSERT_EQUAL_HEX8(0, output.byte(50) & 0xC0);
}

void test_boundary_rejections_before_clocks()
{
  Jtag jtag(1, 2, 3, 4, 5);
  JtagChain<2, 406> small(jtag);
  small.add(JtagDevice::fromProfile<Profile>());
  small.add(JtagDevice::fromProfile<ArmJtagDp>());
  const auto input = Boundary::initialValues();
  auto output = BitBuffer<406>::fromBits("1");
  TEST_ASSERT_EQUAL_INT(int(JTAG::ERROR::INVALID_SEQUENCE_LEN),
    int(small.device<Profile>(0).sample(input, output)));
  JtagChain<2, 407> chain(jtag);
  chain.add(JtagDevice::fromProfile<Profile>());
  chain.add(JtagDevice::fromProfile<ArmJtagDp>());
  BitBuffer<407> wrong;
  wrong.resize(405);
  TEST_ASSERT_EQUAL_INT(int(JTAG::ERROR::INVALID_BUFFER),
    int(chain.device<Profile>(0).sample(wrong, output)));
  wrong.resize(407);
  TEST_ASSERT_EQUAL_INT(int(JTAG::ERROR::INVALID_BUFFER),
    int(chain.device<Profile>(0).sample(wrong, output)));
  TEST_ASSERT_EQUAL_INT(int(JTAG::ERROR::INVALID_INSTRUCTION),
    int(chain.device<Profile>(0).select(static_cast<Instruction>(3))));
  TEST_ASSERT_EQUAL_UINT32(0, clocks);
  TEST_ASSERT_EQUAL_UINT32(1, output.bitCount());
  TEST_ASSERT_EQUAL_HEX8(1, output.byte(0));
}

void test_boundary_idcode_through_device_access()
{
  Jtag jtag(1, 2, 3, 4, 5);
  JtagChain<2, 33> chain(jtag);
  chain.add(JtagDevice::fromProfile<Profile>());
  chain.add(JtagDevice::fromProfile<ArmJtagDp>());
  readingId = true;
  uint32_t id = 0;
  TEST_ASSERT_EQUAL_INT(0, int(chain.device<Profile>(0).readIdcode(id)));
  TEST_ASSERT_EQUAL_HEX32(0x36413041UL, id);
  TEST_ASSERT_TRUE(Profile::matchesIdcode(id));
  TEST_ASSERT_EQUAL_UINT32(54, clocks);
}

int main()
{
  UNITY_BEGIN();
  RUN_TEST(test_instruction_codes_and_widths);
  RUN_TEST(test_identity_and_compliance_metadata);
  RUN_TEST(test_cell_mapping_and_initial_vector);
  RUN_TEST(test_pin_helpers_and_validation);
  RUN_TEST(test_full_boundary_scan_with_debug_bypass);
  RUN_TEST(test_boundary_rejections_before_clocks);
  RUN_TEST(test_boundary_idcode_through_device_access);
  return UNITY_END();
}
