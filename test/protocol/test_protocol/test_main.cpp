#include <protocol/JtagProtocol.hpp>
#include <SimulatedGpio.hpp>
#include <unity.h>
#include <vector>
#include <utility>
#include <algorithm>

using namespace ArduJtagProtocol;
static std::vector<std::pair<uint8_t, uint8_t>> trace;
static uint8_t simulateClock(uint8_t tms, uint8_t tdi)
{
  trace.emplace_back(tms, tdi);
  return 1; // Makes partial-byte masking visible in responses.
}
void setUp() { trace.clear(); }
void tearDown() {}
static uint8_t reply[MaxResponseBytes];
static size_t send(Handler &handler, std::vector<uint8_t> request)
{
  std::fill(reply, reply + sizeof(reply), 0xCC);
  return handler.handle(request.data(), request.size(), reply, sizeof(reply));
}
static void reset(Handler &handler)
{
  TEST_ASSERT_EQUAL_UINT(5, send(handler, {1, 3, 0, 0}));
  TEST_ASSERT_EQUAL_UINT8(0, reply[4]);
  trace.clear();
}
void test_info_and_speed_do_not_clock()
{
  Jtag jtag(1, 2, 3, 4, 5); Handler handler(jtag);
  TEST_ASSERT_EQUAL_UINT(14, send(handler, {1, 1, 0x34, 0x12}));
  TEST_ASSERT_EQUAL_UINT8(0x34, reply[2]); TEST_ASSERT_EQUAL_UINT8(0x12, reply[3]);
  TEST_ASSERT_EQUAL_UINT(512, read16(reply + 5));
  TEST_ASSERT_EQUAL_UINT(512, read16(reply + 7));
  TEST_ASSERT_EQUAL_UINT(256, read16(reply + 9));
  TEST_ASSERT_EQUAL_UINT(500, read16(reply + 11));
  TEST_ASSERT_EQUAL_UINT8(0, reply[13]);
  send(handler, {1, 2, 0, 0, 100, 0, 0, 0}); TEST_ASSERT_EQUAL_UINT8(0, reply[4]);
  send(handler, {1, 2, 0, 0, 0, 0, 0, 0}); TEST_ASSERT_EQUAL_UINT8(4, reply[4]);
  send(handler, {1, 2, 0, 0, 0, 0, 1, 0}); TEST_ASSERT_EQUAL_UINT8(4, reply[4]);
  TEST_ASSERT_TRUE(trace.empty());
}
void test_reset_enters_idle_and_ir_matches_wire_bits()
{
  Jtag jtag(1, 2, 3, 4, 5); Handler handler(jtag);
  send(handler, {1, 4, 0, 0, 9, 0, 0xFE, 1}); TEST_ASSERT_EQUAL_UINT8(5, reply[4]);
  TEST_ASSERT_TRUE(trace.empty());
  send(handler, {1, 3, 0, 0});
  TEST_ASSERT_EQUAL_UINT(6, trace.size());
  for (size_t i = 0; i < 6; ++i) TEST_ASSERT_EQUAL_UINT8(i < 5, trace[i].first);
  trace.clear();
  TEST_ASSERT_EQUAL_UINT(5, send(handler, {1, 4, 0, 0, 9, 0, 0xFE, 1}));
  TEST_ASSERT_EQUAL_UINT8(0, reply[4]);
  TEST_ASSERT_EQUAL_UINT(16, trace.size());
  for (size_t i = 0; i < 9; ++i) TEST_ASSERT_EQUAL_UINT8((0x1FE >> i) & 1, trace[5 + i].second);
  send(handler, {1, 1, 0, 0}); TEST_ASSERT_EQUAL_UINT8(1, reply[13]);
}
void test_full_boundary_scan_and_partial_response()
{
  Jtag jtag(1, 2, 3, 4, 5); Handler handler(jtag); reset(handler);
  std::vector<uint8_t> request = {1, 5, 7, 0, 0x97, 1}; // 407 bits
  request.resize(57, 0x55); // last byte 0x55 has zero unused top bit
  TEST_ASSERT_EQUAL_UINT(58, send(handler, request));
  TEST_ASSERT_EQUAL_UINT8(0, reply[4]); TEST_ASSERT_EQUAL_UINT(407, read16(reply + 5));
  TEST_ASSERT_EQUAL_UINT(412, trace.size());
  for (size_t i = 0; i < 407; ++i) {
    TEST_ASSERT_EQUAL_UINT8((request[6 + i / 8] >> (i % 8)) & 1, trace[3 + i].second);
    TEST_ASSERT_EQUAL_UINT8(i == 406, trace[3 + i].first);
  }
  for (size_t i = 7; i < 57; ++i) TEST_ASSERT_EQUAL_HEX8(0xFF, reply[i]);
  TEST_ASSERT_EQUAL_HEX8(0x7F, reply[57]);
  request = {1, 5, 0, 0, 0, 2}; request.resize(70, 0);
  TEST_ASSERT_EQUAL_UINT(71, send(handler, request)); // maximum DR size
}
void test_raw_clocks_require_reset_before_scans()
{
  Jtag jtag(1, 2, 3, 4, 5); Handler handler(jtag); reset(handler);
  TEST_ASSERT_EQUAL_UINT(9, send(handler, {1, 6, 0, 0, 9, 0, 0x81, 1, 0x42, 0}));
  TEST_ASSERT_EQUAL_UINT8(0, reply[4]); TEST_ASSERT_EQUAL_UINT(9, trace.size());
  for (size_t i = 0; i < 9; ++i) {
    TEST_ASSERT_EQUAL_UINT8((0x181 >> i) & 1, trace[i].first);
    TEST_ASSERT_EQUAL_UINT8((0x42 >> i) & 1, trace[i].second);
  }
  trace.clear();
  send(handler, {1, 5, 0, 0, 1, 0, 0}); TEST_ASSERT_EQUAL_UINT8(5, reply[4]);
  TEST_ASSERT_TRUE(trace.empty());
  std::vector<uint8_t> request = {1, 6, 0, 0, 0, 1}; request.resize(70, 0);
  TEST_ASSERT_EQUAL_UINT(39, send(handler, request)); // maximum raw sequence
  TEST_ASSERT_EQUAL_UINT(256, trace.size());
  reset(handler);
  send(handler, {1, 5, 0, 0, 1, 0, 0}); TEST_ASSERT_EQUAL_UINT8(0, reply[4]);
}
void test_invalid_packets_never_clock()
{
  Jtag jtag(1, 2, 3, 4, 5); Handler handler(jtag); reset(handler);
  const std::vector<std::vector<uint8_t>> invalid = {
    {2, 3, 0, 0}, {1, 99, 0, 0}, {1, 3, 0, 0, 0}, {1, 1, 0, 0, 0},
    {1, 2, 0, 0, 1}, {1, 4, 0, 0}, {1, 4, 0, 0, 0, 0},
    {1, 5, 0, 0, 1, 2, 0}, {1, 6, 0, 0, 1, 1, 0},
    {1, 5, 0, 0, 9, 0, 0}, {1, 4, 0, 0, 1, 0, 0x80},
    {1, 6, 0, 0, 1, 0, 0, 0x80}, {1, 5, 0, 0, 1, 0, 0, 0}
  };
  for (const auto &request : invalid) {
    TEST_ASSERT_EQUAL_UINT(5, send(handler, request));
    TEST_ASSERT_NOT_EQUAL(0, reply[4]);
    TEST_ASSERT_TRUE(trace.empty());
  }
  std::vector<uint8_t> oversized(254, 0); oversized[0] = 1; oversized[1] = 3;
  send(handler, oversized); TEST_ASSERT_EQUAL_UINT8(3, reply[4]);
  TEST_ASSERT_TRUE(trace.empty());
}
void test_truncation_and_insufficient_output_are_rejected()
{
  Jtag jtag(1, 2, 3, 4, 5); Handler handler(jtag);
  const uint8_t request[] = {1, 3, 0, 0};
  for (size_t length = 0; length < 4; ++length)
    TEST_ASSERT_EQUAL_UINT(0, handler.handle(request, length, reply, sizeof(reply)));
  TEST_ASSERT_EQUAL_UINT(0, handler.handle(request, 4, reply, sizeof(reply) - 1));
  TEST_ASSERT_EQUAL_UINT(0, handler.handle(nullptr, 4, reply, sizeof(reply)));
  TEST_ASSERT_TRUE(trace.empty());
}
int main()
{
  UNITY_BEGIN();
  RUN_TEST(test_info_and_speed_do_not_clock);
  RUN_TEST(test_reset_enters_idle_and_ir_matches_wire_bits);
  RUN_TEST(test_full_boundary_scan_and_partial_response);
  RUN_TEST(test_raw_clocks_require_reset_before_scans);
  RUN_TEST(test_invalid_packets_never_clock);
  RUN_TEST(test_truncation_and_insufficient_output_are_rejected);
  return UNITY_END();
}
