/**
 * \file         JtagCommon.cpp
 * \author       Aliaksander Kavalchuk (aliaksander.kavalchuk@gmail.com)
 * \brief        This file contains common definitions, constants, and utility functions used across the JTAG interface
 *               implementation.
 */

//_____ I N C L U D E S _______________________________________________________
#include "JtagCommon.hpp"

#include <assert.h>
//_____ C O N F I G S  ________________________________________________________
//_____ D E F I N I T I O N S _________________________________________________
//_____ C L A S S E S _________________________________________________________
namespace JTAG
{
  void setBitArray(int i_bit, uint8_t *data, int value)
  {
    int i_byte;
    uint32_t mask;

    i_byte = i_bit >> 3;  // floor(i_bit/8)
    mask = 1 << (i_bit & 0x7);

    if (value == 0)
    {
      data[i_byte] &= ~mask;
    }
    else
    {
      data[i_byte] |= mask;
    }
  }

  int getBitArray(int i_bit, const uint8_t *data)
  {
    int i_byte;
    uint8_t mask;

    i_byte = i_bit >> 3;  // floor(i_bit/8)
    mask = 1 << (i_bit & 0x7);

    return ((data[i_byte] & mask) == 0) ? 0 : 1;
  }
}
