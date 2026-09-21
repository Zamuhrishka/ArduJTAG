#pragma once

#include <stddef.h>
#include <stdint.h>

/** @brief Operations on byte arrays packed least significant bit first. */
namespace BitUtils
{
  /**
   * @brief Read a bit by its zero-based index.
   * @note data must be non-null and contain the indexed byte. No bounds checks.
   */
  inline bool getBit(size_t index, const uint8_t *data)
  {
    return ((data[index / 8] >> (index % 8)) & 1U) != 0;
  }

  /**
   * @brief Set or clear a bit, preserving all other bits.
   * @note data must be non-null and contain the indexed byte. No bounds checks.
   */
  inline void set(size_t index, uint8_t *data, bool value)
  {
    const uint8_t mask = uint8_t(1U << (index % 8));
    if (value) {
      data[index / 8] |= mask;
    } else {
      data[index / 8] &= uint8_t(~mask);
    }
  }
}
