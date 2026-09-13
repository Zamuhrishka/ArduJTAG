#pragma once

#include <BitBuffer.hpp>
#include <JtagProfile.hpp>

/** @brief Description of a device whose all-ones IR instruction selects BYPASS. */
class JtagDevice
{
public:
  explicit JtagDevice(size_t irBits = 0) : instructionBits(irBits) {}

  /** @brief Create a device with a statically defined instruction profile. */
  template <typename Profile>
  static JtagDevice fromProfile()
  {
    JtagDevice device(Profile::IrLength);
    device.profile = JtagProfileDetail::identity<typename Profile::Instruction>();
    return device;
  }

  /** @brief True only for the declared profile, never inferred from IR length. */
  template <typename Instruction>
  bool hasProfile() const
  {
    return profile == JtagProfileDetail::identity<Instruction>();
  }

  size_t irLength() const { return instructionBits; }
  bool valid() const
  {
    return instructionBits > 0 && instructionBits <= BitBufferConfig::MaxCapacity;
  }

private:
  size_t instructionBits;
  const void *profile = nullptr;
};
