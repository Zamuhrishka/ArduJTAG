#pragma once

#include <BitBuffer.hpp>
#include <JtagCommon.hpp>

/**
 * @brief Non-owning access to one device through its instruction profile.
 * Stores only a chain reference and an index; copying does not copy the chain.
 * The chain must outlive this object. Appending devices preserves indices.
 * valid() checks the current configuration; transfers validate it again, so
 * calling valid() first is optional. No heap allocation or instruction caching.
 */
template <typename Profile, typename Chain>
class JtagDeviceAccess
{
public:
  using Instruction = typename Profile::Instruction;

  JtagDeviceAccess(Chain &chain, size_t index) : chain(chain), index(index) {}

  bool valid() const
  {
    return chain.template matchesProfile<Profile>(index);
  }

  /** @brief Exchange a named command, using the chain's profile and buffer checks. */
  template <size_t InputCapacity, size_t OutputCapacity>
  JTAG::ERROR transfer(Instruction instruction, const BitBuffer<InputCapacity> &input,
                       BitBuffer<OutputCapacity> &output)
  {
    return chain.transfer(index, instruction, input, output);
  }

  /** @brief Select a named IR instruction without a DR exchange. */
  JTAG::ERROR select(Instruction instruction)
  {
    return chain.select(index, instruction);
  }

  /** @brief Select BYPASS; generates only IR clocks. */
  JTAG::ERROR bypass() { return select(Instruction::Bypass); }

  /** @brief Select HIGHZ; generates only IR clocks. Requires profile support. */
  JTAG::ERROR highZ() { return select(Instruction::HighZ); }

  /**
   * @brief Capture boundary cells while shifting explicitly supplied preload data.
   * Profiles with combined SAMPLE/PRELOAD should alias Sample and Preload to
   * that opcode. The shifted values may update the preload latches at Update-DR.
   */
  template <size_t InputCapacity, size_t OutputCapacity>
  JTAG::ERROR sample(const BitBuffer<InputCapacity> &values, BitBuffer<OutputCapacity> &captured)
  {
    return transfer(Instruction::Sample, values, captured);
  }

  /** @brief Load boundary values with PRELOAD, discarding captured bits. */
  template <size_t InputCapacity>
  JTAG::ERROR preload(const BitBuffer<InputCapacity> &values)
  {
    BitBuffer<InputCapacity> discarded;
    return transfer(Instruction::Preload, values, discarded);
  }

  /** @brief Combined SAMPLE/PRELOAD exchange for profiles exposing SamplePreload. */
  template <size_t InputCapacity, size_t OutputCapacity>
  JTAG::ERROR samplePreload(const BitBuffer<InputCapacity> &values, BitBuffer<OutputCapacity> &captured)
  {
    return transfer(Instruction::SamplePreload, values, captured);
  }

  /**
   * @brief Select EXTEST and exchange boundary cells.
   * Preload suitable initial values before the first call: EXTEST takes effect
   * at Update-IR, before these new DR values are shifted. Captured bits precede
   * the update of the supplied values. This does not perform board-level analysis.
   */
  template <size_t InputCapacity, size_t OutputCapacity>
  JTAG::ERROR extest(const BitBuffer<InputCapacity> &values, BitBuffer<OutputCapacity> &captured)
  {
    return transfer(Instruction::Extest, values, captured);
  }

  /**
   * @brief Select INTEST and exchange its data register.
   * Preload initial values and provide any required test clocks according to
   * the device documentation. No core test sequencing is performed here.
   */
  template <size_t InputCapacity, size_t OutputCapacity>
  JTAG::ERROR intest(const BitBuffer<InputCapacity> &values, BitBuffer<OutputCapacity> &captured)
  {
    return transfer(Instruction::Intest, values, captured);
  }

  /**
   * @brief Read a profile's 32-bit IDCODE using a zero-filled request.
   * Profile must expose Instruction::Idcode with drLength() == 32.
   * Returns the transfer status and leaves idcode unchanged on failure.
   * Bit zero received is bit zero of the uint32_t, independent of host byte order.
   * Does not reset the chain or verify that the returned ID matches a device.
   */
  JTAG::ERROR readIdcode(uint32_t &idcode)
  {
    static_assert(Profile::drLength(Profile::Instruction::Idcode) == 32,
                  "IDCODE must have a fixed 32-bit DR");
    const auto request = BitBuffer<32>::fromBytes({0, 0, 0, 0});
    BitBuffer<32> response;
    const JTAG::ERROR status = transfer(Instruction::Idcode, request, response);
    if (status != JTAG::ERROR::NO) {
      return status;
    }

    uint32_t value = 0;
    for (size_t i = 0; i < 4; ++i) {
      value |= static_cast<uint32_t>(response.byte(i)) << (8 * i);
    }
    idcode = value;

    return JTAG::ERROR::NO;
  }

private:
  Chain &chain;
  size_t index;
};
