#pragma once

#include <stdint.h>

namespace JTAG
{
  /**
   * \enum         ERROR
   * \brief        Enumeration of possible error codes that JTAG operations might return.
   */
  enum class ERROR : int32_t
  {
    NO = 0,
    INVALID_PIN = -1,
    INVALID_SPEED = -2,
    INVALID_SEQUENCE_LEN = -3,
    INVALID_BUFFER = -4,
    INVALID_DEVICE = -5,
    INVALID_INSTRUCTION = -6,
  };
}
