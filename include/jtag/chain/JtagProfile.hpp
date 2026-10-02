#pragma once

#include <stdint.h>

/** @brief Specialize for a profile's Instruction enum, inheriting that profile. */
template <typename Instruction>
struct JtagInstructionProfile;

namespace JtagProfileDetail
{
  // One identity byte per instruction enum, shared across translation units.
  // Devices store only its address, never a command table or virtual interface.
  template <typename Instruction>
  const void *identity()
  {
    static const uint8_t tag = 0;
    return &tag;
  }
}
