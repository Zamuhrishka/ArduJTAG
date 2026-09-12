#pragma once

#include <BitBuffer.hpp>

/** @brief Description of a device whose all-ones IR instruction selects BYPASS. */
class JtagDevice
{
public:
  explicit JtagDevice(size_t irBits = 0) : instructionBits(irBits) {}

  size_t irLength() const { return instructionBits; }
  bool valid() const
  {
    return instructionBits > 0 && instructionBits <= BitBufferConfig::MaxCapacity;
  }

private:
  size_t instructionBits;
};
