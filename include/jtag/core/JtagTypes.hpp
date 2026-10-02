/**
 * \brief  This file contains pin types and constants used across the JTAG interface implementation.
 */

#pragma once

//_____ I N C L U D E S _______________________________________________________
#include <stdint.h>
//_____ C O N F I G S  ________________________________________________________
//_____ D E F I N I T I O N S _________________________________________________
namespace JTAG
{
  const uint8_t PINS_NUMBER = 5;

  /**
   * \enum  CONSTANTS
   * \brief  Defines various constants used in JTAG operations like maximum speed and sequence lengths.
   */
  enum class CONSTANTS : uint32_t
  {
    MAX_SPEED_KHZ = 500,  ///< Maximum speed in kilohertz for JTAG operations.
    MAX_SEQUENCE_LEN = 256,  ///< Maximum length of a JTAG sequence in bits.
    MAX_SEQUENCE_LEN_BYTES = MAX_SEQUENCE_LEN / 8,  ///< Maximum length of a JTAG sequence in bytes.
  };

  /**
   * \enum  PIN
   * \brief Enumeration of the pin types in a JTAG interface.
   */
  enum class PIN : uint8_t
  {
    TCK = 0,
    TMS = 1,
    TDI = 2,
    TDO = 3,
    TRST = 4,
  };
}
