/**
 * \file         JtagCommon.hpp
 * \author       Aliaksander Kavalchuk (aliaksander.kavalchuk@gmail.com)
 * \brief        This file contains common definitions, constants, and utility functions used across the JTAG interface
 *               implementation.
 */

#pragma once

//_____ I N C L U D E S _______________________________________________________
#include <assert.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
//_____ C O N F I G S  ________________________________________________________
//_____ D E F I N I T I O N S _________________________________________________
/**
 * \namespace    JTAG
 * \brief        The namespace JTAG contains all necessary constants, enums, and utility functions for working with the
 * JTAG protocol.
 */
namespace JTAG
{
  const uint8_t PINS_NUMBER = 5;

  /**
   * \enum         CONSTANTS
   * \brief        Defines various constants used in JTAG operations like maximum speed and sequence lengths.
   */
  enum class CONSTANTS : uint32_t
  {
    MAX_SPEED_KHZ = 500,
    MAX_SEQUENCE_LEN = 256,
    MAX_SEQUENCE_LEN_BYTES = MAX_SEQUENCE_LEN / 8,  // 32
  };

  /**
   * \enum         PIN
   * \brief        Enumeration of the pin types in a JTAG interface.
   */
  enum class PIN : uint8_t
  {
    TCK = 0,
    TMS = 1,
    TDI = 2,
    TDO = 3,
    TRST = 4,
  };

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
  };

  /**
   * \brief        Set a specific bit in an array to a value.
   *
   * \param i_bit  The index of the bit to be set.
   * \param data   Pointer to the data array where the bit will be set.
   * \param value  The value to set the bit to (1 or 0).
   */
  void setBitArray(int i_bit, uint8_t *data, int value);

  /**
   * \brief        Get the value of a specific bit in an array.
   *
   * \param i_bit  The index of the bit to get.
   * \param data   Pointer to the data array from which the bit value will be read.
   * \return       The value of the specified bit (1 or 0).
   */
  int getBitArray(int i_bit, const uint8_t *data);
}
//_____ C L A S S E S __________________________________________________________
