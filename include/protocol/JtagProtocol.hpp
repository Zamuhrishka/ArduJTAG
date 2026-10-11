#pragma once

#include <jtag/core/Jtag.hpp>

namespace ArduJtagProtocol
{
constexpr uint8_t Version = 1;
constexpr size_t MaxScanBits = 512;
constexpr size_t MaxClockBits = 256;
constexpr size_t MaxRequestBytes = 70;
constexpr size_t MaxResponseBytes = 71;

enum class Command : uint8_t {
  Info = 1, Speed = 2, Reset = 3, Ir = 4, Dr = 5, Clocks = 6
};
enum class Status : uint8_t {
  Ok = 0, VersionMismatch = 1, UnknownCommand = 2, InvalidLength = 3,
  InvalidValue = 4, ResetRequired = 5, JtagFailure = 6
};

inline uint16_t read16(const uint8_t *p) { return uint16_t(p[0]) | (uint16_t(p[1]) << 8); }
inline void write16(uint8_t *p, uint16_t v) { p[0] = uint8_t(v); p[1] = uint8_t(v >> 8); }

// Payload decoding and execution; idleKnown tracks whether high-level scans are safe.
// Request and response buffers must not overlap. No heap allocation.
class Handler
{
public:
  explicit Handler(Jtag &jtag) : jtag_(jtag) {}

  // Returns response length; zero means unidentifiable request/insufficient
  // output storage. No clocks occur unless the entire request is valid.
  size_t handle(const uint8_t *request, size_t length,
                uint8_t *response, size_t capacity)
  {
    if (!request || !response || length < 4 || capacity < MaxResponseBytes) return 0;
    response[0] = Version;
    response[1] = request[1];
    response[2] = request[2];
    response[3] = request[3];
    response[4] = uint8_t(Status::Ok);
    if (request[0] != Version) return fail(response, Status::VersionMismatch);
    if (length > MaxRequestBytes) return fail(response, Status::InvalidLength);
    const auto command = static_cast<Command>(request[1]);
    if (command == Command::Info) {
      if (length != 4) return fail(response, Status::InvalidLength);
      write16(response + 5, MaxScanBits);  // max IR bits
      write16(response + 7, MaxScanBits);  // max DR bits
      write16(response + 9, MaxClockBits);
      write16(response + 11, uint16_t(JTAG::CONSTANTS::MAX_SPEED_KHZ));
      response[13] = idleKnown_ ? 1 : 0;
      return 14;
    }
    if (command == Command::Speed) {
      if (length != 8) return fail(response, Status::InvalidLength);
      const uint32_t khz = uint32_t(read16(request + 4)) |
                           (uint32_t(read16(request + 6)) << 16);
      if (jtag_.setSpeed(khz) != JTAG::ERROR::NO) return fail(response, Status::InvalidValue);
      return 5;
    }
    if (command == Command::Reset) {
      if (length != 4) return fail(response, Status::InvalidLength);
      jtag_.reset();
      // Jtag::reset ends in Test-Logic-Reset. Enter Idle so both IR and DR work.
      const auto zero = BitBuffer<1>::fromBits("0");
      BitBuffer<1> ignored;
      jtag_.clockCycles(zero, zero, ignored);
      idleKnown_ = true;
      return 5;
    }
    if (command != Command::Ir && command != Command::Dr && command != Command::Clocks)
      return fail(response, Status::UnknownCommand);
    if (length < 6) return fail(response, Status::InvalidLength);
    const size_t bits = read16(request + 4);
    const bool clocks = command == Command::Clocks;
    if (!bits || bits > (clocks ? MaxClockBits : MaxScanBits))
      return fail(response, Status::InvalidValue);
    const size_t bytes = (bits + 7) / 8;
    if (length != 6 + bytes * (clocks ? 2 : 1)) return fail(response, Status::InvalidLength);
    // Require zero padding so the wire representation is unambiguous.
    const uint8_t unused = bits % 8 ? uint8_t(0xFFU << (bits % 8)) : 0;
    if ((request[5 + bytes] & unused) || (clocks && (request[5 + 2 * bytes] & unused)))
      return fail(response, Status::InvalidValue);
    if (!clocks && !idleKnown_) return fail(response, Status::ResetRequired);

    const auto first = BitBuffer<MaxScanBits>::fromBytes(request + 6, bytes, bits);
    BitBuffer<MaxScanBits> result;
    JTAG::ERROR error;
    if (clocks) {
      const auto tdi = BitBuffer<MaxClockBits>::fromBytes(request + 6 + bytes, bytes, bits);
      error = jtag_.clockCycles(first, tdi, result); // first = TMS
      idleKnown_ = false; // Require RESET before any later high-level scan.
    } else if (command == Command::Ir) {
      error = jtag_.ir(first);
    } else {
      error = jtag_.dr(first, result);
    }
    if (error != JTAG::ERROR::NO) {
      idleKnown_ = false;
      return fail(response, Status::JtagFailure);
    }
    if (command == Command::Ir) return 5;
    write16(response + 5, bits);
    for (size_t i = 0; i < bytes; ++i) response[7 + i] = result.byte(i);
    return 7 + bytes;
  }

private:
  Jtag &jtag_;
  bool idleKnown_ = false;
  static size_t fail(uint8_t *response, Status status)
  {
    response[4] = uint8_t(status);
    return 5;
  }
};
}
