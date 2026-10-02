#pragma once

#include <stdint.h>

namespace BoundaryTypes
{
  constexpr uint16_t NoCell = 0xFFFF;

  /** @brief Cell indices for a pin with input and/or a controlled output. */
  struct Cells
  {
    uint16_t input;
    uint16_t output;
    uint16_t control;
    uint16_t packagePin; // Physical package pin; zero means unspecified.
    bool disableValue;   // BSDL disval: control value that disables the driver.

    bool valid() const { return input != NoCell || output != NoCell; }
    bool hasOutput() const { return output != NoCell && control != NoCell; }
  };
}
