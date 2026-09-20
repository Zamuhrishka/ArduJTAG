/**
 * \file         JtagGpio.cpp
 * \author       Aliaksander Kavalchuk (aliaksander.kavalchuk@gmail.com)
 * \brief        This file contains the prototypes and definition for the JtagGpio class which manages the JTAG
 *               communication bus.
 */

//_____ I N C L U D E S _______________________________________________________
#include "JtagGpio.hpp"

#include <Arduino.h>

#include <assert.h>

#include "JtagCommon.hpp"

//_____ C O N F I G S  ________________________________________________________
// Define ARDUJTAG_DEBUG to print TMS/TDI/TDO for each clock.
//_____ D E F I N I T I O N S _________________________________________________
//_____ C L A S S E S _________________________________________________________
JtagGpio::JtagGpio(GpioPin tms, GpioPin tdi, GpioPin tdo, GpioPin tck, GpioPin rst):
    _tms(tms), _tdi(tdi), _tdo(tdo), _tck(tck), _rst(rst)
{
  this->last_tck_micros = micros();
  this->min_tck_micros = 1;
}

JTAG::ERROR JtagGpio::setSpeed(uint32_t khz)
{
  if (khz == 0 || khz > static_cast<uint32_t>(JTAG::CONSTANTS::MAX_SPEED_KHZ))
  {
    return JTAG::ERROR::INVALID_SPEED;
  }

  /*
   * Mininum time for TCK to be stable is half the clock period.
   * For 100kHz of TCK frequency the period is 10us so jtag_min_tck_micros is 5us.
   */
  this->min_tck_micros = (500U + khz - 1) / khz;  // ceil

  return JTAG::ERROR::NO;
}

uint32_t JtagGpio::getSpeed() const
{
  return this->min_tck_micros;
}

void JtagGpio::pulseTrst()
{
  this->_rst.setLow();
  delayMicroseconds(this->min_tck_micros);
  this->_rst.setHigh();
}

uint8_t JtagGpio::clock(uint8_t tms, uint8_t tdi)
{
  assert(tms == HIGH || tms == LOW);
  assert(tdi == HIGH || tdi == LOW);

  // Setting TDI and TMS before rising edge of TCK.
  this->_tdi.setValue(tdi);
  this->_tms.setValue(tms);

#if defined(ARDUJTAG_DEBUG)
  Serial.print(tms);
  Serial.print(tdi);
#endif

  // Waiting until TCK has been stable for at least jtag_min_tck_micros.
  size_t cur_micros = micros();

  if (cur_micros < this->last_tck_micros + this->min_tck_micros)
  {
    delayMicroseconds(this->last_tck_micros + this->min_tck_micros - cur_micros);
  }

  this->_tck.setHigh();
  delayMicroseconds(this->min_tck_micros);

  // Sample the current bit while TCK is high, before the target advances TDO
  // on the falling edge. Reading after setLow() loses the first shifted bit.
  const uint8_t tdo = this->_tdo.get();
  this->_tck.setLow();

  // Saving timestamp of last TCK change
  this->last_tck_micros = micros();

#if defined(ARDUJTAG_DEBUG)
  Serial.println(tdo);
#endif

  return tdo;
}
