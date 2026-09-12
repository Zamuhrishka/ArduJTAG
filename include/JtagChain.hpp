#pragma once

#include <Jtag.hpp>
#include <JtagDevice.hpp>

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
    request.resize(irBits);
    size_t offset = 0;
    // Bits for the device nearest TDO must enter the chain first.
    for (size_t position = count; position > 0; --position) {
      const size_t index = position - 1;
      for (size_t bit = 0; bit < devices[index].irLength(); ++bit) {
        request.set(offset++, index == target ? instruction.getBit(bit) : true);
      }
    }
    const JTAG::ERROR irStatus = jtag.ir(request);
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
  Jtag &jtag;
  JtagDevice devices[MaxDevices];
  size_t count = 0;
  size_t irBits = 0;
};
