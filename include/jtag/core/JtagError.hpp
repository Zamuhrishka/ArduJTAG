#pragma once

//_____ I N C L U D E S _______________________________________________________
#include <stdint.h>
//_____ C O N F I G S  ________________________________________________________
//_____ D E F I N I T I O N S _________________________________________________
namespace JTAG
{
  /**
   * \enum  ERROR
   * \brief Enumeration of possible error codes that JTAG operations might return.
   */
  enum class ERROR : int32_t
  {
    NO = 0,  ///< No error occurred.
    INVALID_PIN = -1,  ///< The specified pin is invalid or not configured correctly.
    INVALID_SPEED = -2,  ///< The specified speed is invalid or not supported.
    INVALID_SEQUENCE_LEN = -3,  ///< The specified sequence length is invalid or exceeds the allowed limit.
    INVALID_BUFFER = -4,  ///< The specified buffer is invalid or not supported.
    INVALID_DEVICE = -5,  ///< The specified device is invalid or not supported.
    INVALID_INSTRUCTION = -6,  ///< The specified instruction is invalid or not supported.
  };
}
