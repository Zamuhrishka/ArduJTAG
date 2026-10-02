/**
 * Write, read back and restore one STM32F407 SRAM word through ARM JTAG-DP.
 * Uses only Jtag/BitBuffer; Boundary TAP is in BYPASS throughout the test.
 * Release target NRST (unlike boundary-scan examples). Disconnect ST-LINK.
 * The Cortex-M4 is halted before accessing SRAM and is LEFT HALTED afterward.
 * Power-cycle the target to run its firmware again. Serial: 115200 baud.
 */
#include <Arduino.h>
#include <jtag/core/Jtag.hpp>

Jtag jtag(3, 4, 5, 2, 6); // TMS, TDI, TDO, TCK, nTRST (not target NRST)

constexpr uint32_t TestAddress = 0x20000000UL;
constexpr uint32_t TestValue = 0xA5A55A5AUL;
constexpr uint32_t DhcsrAddress = 0xE000EDF0UL;
constexpr uint32_t PowerRequest = 0x50000000UL;
constexpr uint32_t PowerAck = 0xA0000000UL;
constexpr uint32_t StickyFlags = 0x32UL; // JTAG STICKYORUN, STICKYCMP, STICKYERR
constexpr uint16_t MaxAttempts = 100;
static_assert((TestAddress & 3U) == 0, "Use an aligned SRAM address");
static_assert(TestAddress >= 0x20000000UL && TestAddress <= 0x2001FFFCUL,
              "Use STM32F407 SRAM1/SRAM2, not Flash or peripherals");

// An ACK describes the PREVIOUS request. If it is WAIT, Update-DR discards
// the new request, so repeat that same scan until it is actually accepted.
// ADIv5 JTAG ACKs differ from SWD: 001 = WAIT, 010 = OK/FAULT.
bool scanUntilAccepted(bool ap, uint8_t address, bool read, uint32_t value,
                       uint32_t &previousResult)
{
  // TDI -> Boundary (IR5) -> Debug (IR4) -> TDO: Debug shifts first.
  const uint8_t instructionBytes[] = {uint8_t(ap ? 0xFB : 0xFA), 0x01};
  const auto instruction = BitBuffer<9>::fromBytes(instructionBytes, 9);
  if (jtag.ir(instruction) != JTAG::ERROR::NO) {
    Serial.println(F("IR transfer failed"));
    return false;
  }
  BitBuffer<36> request;
  request.resize(36); // Debug DR35 followed by one Boundary BYPASS bit (0).
  request.set(0, read);
  request.set(1, (address & 4U) != 0);
  request.set(2, (address & 8U) != 0);
  for (size_t i = 0; i < 32; ++i) request.set(i + 3, (value >> i) & 1U);
  BitBuffer<36> response;
  const auto idle = BitBuffer<8>::fromBits("00000000");
  BitBuffer<8> ignored;
  for (uint16_t attempt = 0; attempt < MaxAttempts; ++attempt) {
    if (jtag.dr(request, response) != JTAG::ERROR::NO ||
        jtag.clockCycles(idle, idle, ignored) != JTAG::ERROR::NO) {
      Serial.println(F("DR/idle transfer failed"));
      return false;
    }
    const uint8_t ack = response.byte(0) & 7U;
    if (ack == 2) {
      previousResult = 0;
      for (size_t i = 0; i < 32; ++i)
        previousResult |= uint32_t(response.getBit(i + 3)) << i;
      return true; // Check CTRL/STAT separately: OK/FAULT alone is not success.
    }
    if (ack != 1) {
      Serial.print(F("Invalid JTAG-DP ACK: 0x"));
      Serial.println(ack, HEX);
      return false;
    }
    delay(1);
  }
  Serial.println(F("JTAG-DP WAIT timeout"));
  return false;
}

bool dapAccess(bool ap, uint8_t address, bool read, uint32_t value, uint32_t &result)
{
  uint32_t ignored;
  if (!scanUntilAccepted(ap, address, read, value, ignored)) return false;
  // This scan returns the requested result and posts a harmless DP RDBUFF
  // read. Its own later result is zero/unused on JTAG-DP (not SWD semantics).
  return scanUntilAccepted(false, 0x0C, true, 0, result);
}

bool checkStatus()
{
  uint32_t status;
  if (!dapAccess(false, 0x04, true, 0, status)) return false;
  if (status & StickyFlags) {
    Serial.print(F("DP sticky error; CTRL/STAT: 0x"));
    Serial.println(status, HEX);
    return false;
  }
  return true;
}

bool apAccess(uint8_t address, bool read, uint32_t value, uint32_t &result)
{
  return dapAccess(true, address, read, value, result) && checkStatus();
}

bool memoryAccess(uint32_t address, bool read, uint32_t value, uint32_t &result)
{
  uint32_t ignored;
  // CSW disables address increment, but set TAR explicitly for each operation.
  return apAccess(0x04, false, address, ignored) &&
         apAccess(0x0C, read, value, result);
}

void setup()
{
  Serial.begin(115200);
  Serial.println(F("EnableArmDap: release target NRST; test leaves CPU halted"));
  // GpioPin initializes outputs LOW. Release the physical TAP reset explicitly.
  digitalWrite(6, HIGH);
  delay(1);
  jtag.setSpeed(100);
  jtag.reset();

  const auto idIr = BitBuffer<9>::fromBytes({0xFE, 0x01}, 9);
  const auto idRequest = BitBuffer<33>::fromBytes({0, 0, 0, 0, 0}, 33);
  BitBuffer<33> idResponse;
  if (jtag.ir(idIr) != JTAG::ERROR::NO ||
      jtag.dr(idRequest, idResponse) != JTAG::ERROR::NO) {
    Serial.println(F("IDCODE transfer failed"));
    return;
  }
  uint32_t idcode = 0;
  for (size_t i = 0; i < 32; ++i) idcode |= uint32_t(idResponse.getBit(i)) << i;
  Serial.print(F("Debug TAP IDCODE: 0x"));
  Serial.println(idcode, HEX);
  if ((idcode & 0x0FFFFFFFUL) != 0x0BA00477UL) {
    Serial.println(F("Unexpected Debug TAP; test stopped"));
    return;
  }

  uint32_t ignored, status;
  // DP SELECT=0 selects AP0, AP bank 0 and DP bank 0.
  if (!dapAccess(false, 0x08, false, 0, ignored) ||
      !dapAccess(false, 0x04, false, PowerRequest | StickyFlags, ignored)) return;
  // JTAG CTRL/STAT sticky flags are write-one-to-clear. ORUNDETECT stays off.
  bool powered = false;
  for (uint16_t attempt = 0; attempt < MaxAttempts; ++attempt) {
    if (!dapAccess(false, 0x04, true, 0, status)) return;
    if (status & StickyFlags) {
      Serial.println(F("DP error during power-up"));
      return;
    }
    if ((status & PowerAck) == PowerAck) { powered = true; break; }
    delay(10);
  }
  if (!powered) { Serial.println(F("DAP power-up timeout")); return; }

  // STM32F407 AHB-AP: privileged data access, debug master, 32-bit Size,
  // AddrInc=off. This CSW value is target-specific, not universal to all APs.
  if (!apAccess(0x00, false, 0x23000002UL, ignored)) return;
  // DHCSR: DBGKEY | C_DEBUGEN | C_HALT. Keep firmware from changing test RAM.
  if (!memoryAccess(DhcsrAddress, false, 0xA05F0003UL, ignored)) return;
  bool halted = false;
  for (uint16_t attempt = 0; attempt < MaxAttempts; ++attempt) {
    if (!memoryAccess(DhcsrAddress, true, 0, status)) return;
    if (status & (1UL << 17)) { halted = true; break; } // S_HALT
    delay(10);
  }
  if (!halted) { Serial.println(F("CPU halt timeout")); return; }

  uint32_t original, actual = 0;
  if (!memoryAccess(TestAddress, true, 0, original)) return;
  Serial.print(F("SRAM address: 0x")); Serial.println(TestAddress, HEX);
  Serial.print(F("Original: 0x")); Serial.println(original, HEX);
  Serial.print(F("Writing:  0x")); Serial.println(TestValue, HEX);
  const bool written = memoryAccess(TestAddress, false, TestValue, ignored);
  const bool readBack = written && memoryAccess(TestAddress, true, 0, actual);
  if (readBack) {
    Serial.print(F("Read:     0x")); Serial.println(actual, HEX);
    Serial.println(actual == TestValue ? F("VERIFY PASS") : F("VERIFY FAIL"));
  } else {
    Serial.println(F("Memory transfer failed; attempting restore"));
    // Clear sticky flags before a best-effort restoration after a fault.
    if (!dapAccess(false, 0x04, false, PowerRequest | StickyFlags, ignored)) {
      Serial.println(F("RESTORE FAILED; SRAM may have changed"));
      return;
    }
  }
  uint32_t restored;
  if (!memoryAccess(TestAddress, false, original, ignored) ||
      !memoryAccess(TestAddress, true, 0, restored) || restored != original) {
    Serial.println(F("RESTORE FAILED; SRAM may have changed"));
    return;
  }
  Serial.println(F("Original word restored. CPU halted; power-cycle STM32 to run firmware."));
}

void loop() {} // Run once; reset the Arduino to repeat.
