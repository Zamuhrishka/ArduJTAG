#pragma once

#include <Jtag.hpp>
#include <JtagDevice.hpp>
#include <JtagDeviceAccess.hpp>

/**
 * @brief Fixed-capacity chain, with devices added in physical TDI-to-TDO order.
 * @tparam MaxDevices Maximum number of devices.
 * @tparam Capacity Maximum combined IR or DR length, including BYPASS bits.
 *
 * Device descriptions are copied; indices are zero-based insertion positions.
 * The Jtag instance must outlive the chain. No heap allocation is used.
 * Each transfer programs the target instruction and all-ones BYPASS instructions
 * for every other device. Only the target DR response is returned.
 * Call Jtag::reset() before the first transfer or after an unknown TAP state.
 * Transfers start in Test-Logic-Reset or Run-Test/Idle and finish in Run-Test/Idle.
 * No instruction state is cached, and transfers do not reset the target device.
 */
template <size_t MaxDevices = 8, size_t Capacity = BitBufferConfig::DefaultCapacity>
class JtagChain
{
  static_assert(MaxDevices > 0 && MaxDevices <= Capacity, "Invalid device capacity");
  static_assert(Capacity > 0 && Capacity <= BitBufferConfig::MaxCapacity, "Invalid bit capacity");

public:
  explicit JtagChain(Jtag &jtag) : jtag(jtag) {}

  /** @brief Append a device; failure leaves the chain unchanged. */
  bool add(const JtagDevice &device)
  {
    if (!device.valid() || count == MaxDevices || device.irLength() > Capacity - irBits) {
      return false;
    }
    devices[count++] = device;
    irBits += device.irLength();
    return true;
  }

  size_t deviceCount() const { return count; }

  /** @brief Check that an index exists and has the requested instruction profile. */
  template <typename Profile>
  bool matchesProfile(size_t index) const
  {
    return index < count && devices[index].template hasProfile<typename Profile::Instruction>();
  }

  /**
   * @brief Create a lightweight device access object, without generating clocks.
   * The returned object refers to this chain, which must outlive it.
   * An invalid index/profile produces an invalid object; operations return
   * INVALID_DEVICE until that index contains a matching device.
   */
  template <typename Profile>
  JtagDeviceAccess<Profile, JtagChain> device(size_t index)
  {
    return JtagDeviceAccess<Profile, JtagChain>(*this, index);
  }

  /**
   * @brief Load only IR, selecting BYPASS for every non-target device.
   * No DR clocks are generated. The selected mode lasts until the next IR
   * update or reset; a later operation on another device puts this one in BYPASS.
   */
  template <typename Instruction>
  auto select(size_t target, Instruction instruction)
    -> decltype(JtagInstructionProfile<Instruction>::encode(instruction), JTAG::ERROR::NO)
  {
    if (target >= count || !devices[target].template hasProfile<Instruction>()) {
      return JTAG::ERROR::INVALID_DEVICE;
    }
    const auto bits = JtagInstructionProfile<Instruction>::encode(instruction);
    if (!bits.valid()) return JTAG::ERROR::INVALID_INSTRUCTION;
    if (bits.bitCount() != devices[target].irLength()) return JTAG::ERROR::INVALID_BUFFER;
    BitBuffer<Capacity> request;
    return shiftInstruction(target, bits, request);
  }

  /**
   * @brief Transfer a named command belonging to the target's declared profile.
   * A missing or different profile returns INVALID_DEVICE. Unsupported enum
   * values return INVALID_INSTRUCTION. Both fail before clocks/output changes.
   * A wrong fixed DR length returns INVALID_BUFFER. Profiles may return zero
   * from drLength() for variable-width commands. Payload contents are unchecked.
   */
  template <typename Instruction, size_t InputCapacity, size_t OutputCapacity>
  auto transfer(size_t target, Instruction instruction,
                const BitBuffer<InputCapacity> &input, BitBuffer<OutputCapacity> &output)
    -> decltype(JtagInstructionProfile<Instruction>::encode(instruction), JTAG::ERROR::NO)
  {
    if (target >= count || !devices[target].template hasProfile<Instruction>()) {
      return JTAG::ERROR::INVALID_DEVICE;
    }

    const auto bits = JtagInstructionProfile<Instruction>::encode(instruction);
    if (!bits.valid()) {
      return JTAG::ERROR::INVALID_INSTRUCTION;
    }

    const size_t requiredBits = JtagInstructionProfile<Instruction>::drLength(instruction);
    if (requiredBits != 0 && input.bitCount() != requiredBits) {
      return JTAG::ERROR::INVALID_BUFFER;
    }

    return transfer(target, bits, input, output);
  }

  /**
   * @brief Select an instruction and exchange the target DR in one operation.
   * @param target Device index in TDI-to-TDO order.
   * @return INVALID_DEVICE for an unknown index; INVALID_BUFFER for invalid
   *         buffers or an instruction length different from the target IR;
   *         INVALID_SEQUENCE_LEN if the combined DR exceeds Capacity.
   * All validation precedes clocks and output changes. Input and output may
   * refer to the same buffer. Other devices contribute one zero BYPASS bit.
   * Two Capacity-bit scratch buffers are allocated on the stack.
   */
  template <size_t IrCapacity, size_t InputCapacity, size_t OutputCapacity>
  JTAG::ERROR transfer(size_t target, const BitBuffer<IrCapacity> &instruction,
                       const BitBuffer<InputCapacity> &input, BitBuffer<OutputCapacity> &output)
  {
    if (target >= count) return JTAG::ERROR::INVALID_DEVICE;
    if (!instruction.valid() || instruction.bitCount() != devices[target].irLength() ||
        !input.valid() || input.bitCount() > output.capacity()) {
      return JTAG::ERROR::INVALID_BUFFER;
    }
    const size_t dataBits = input.bitCount();
    if (dataBits > Capacity - (count - 1)) return JTAG::ERROR::INVALID_SEQUENCE_LEN;

    BitBuffer<Capacity> request;
    BitBuffer<Capacity> response;
    const JTAG::ERROR irStatus = shiftInstruction(target, instruction, request);
    if (irStatus != JTAG::ERROR::NO) return irStatus;

    const size_t drBits = dataBits + count - 1;
    const size_t targetOffset = count - target - 1;
    request.resize(drBits);
    for (size_t bit = 0; bit < drBits; ++bit) {
      request.set(bit, bit >= targetOffset && bit - targetOffset < dataBits
                         ? input.getBit(bit - targetOffset) : false);
    }
    const JTAG::ERROR drStatus = jtag.dr(request, response);
    if (drStatus != JTAG::ERROR::NO) return drStatus;

    output.resize(dataBits);
    for (size_t bit = 0; bit < dataBits; ++bit) {
      output.set(bit, response.getBit(targetOffset + bit));
    }
    return JTAG::ERROR::NO;
  }


private:
  // Caller validates the target and instruction before generating any clocks.
  template <size_t IrCapacity>
  JTAG::ERROR shiftInstruction(size_t target, const BitBuffer<IrCapacity> &instruction,
                               BitBuffer<Capacity> &request)
  {
    request.resize(irBits);
    size_t offset = 0;
    // Bits for the device nearest TDO must enter the chain first.
    for (size_t position = count; position > 0; --position) {
      const size_t index = position - 1;
      for (size_t bit = 0; bit < devices[index].irLength(); ++bit) {
        request.set(offset++, index == target ? instruction.getBit(bit) : true);
      }
    }
    return jtag.ir(request);
  }

  Jtag &jtag;  // Reference to the Jtag instance used for all transfers.
  JtagDevice devices[MaxDevices];  // Array of devices in physical TDI-to-TDO order.
  size_t count = 0;  // Number of devices added to the chain.
  size_t irBits = 0;  // Total IR length of all devices, including BYPASS bits.
};
