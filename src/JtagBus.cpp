/**
 * \file         JtagBus.cpp
 * \author       Aliaksander Kavalchuk (aliaksander.kavalchuk@gmail.com)
 * \brief        This file contains the prototypes and definition for the JtagBus class which manages the JTAG
 *               communication bus.
 */

//_____ I N C L U D E S _______________________________________________________
#include "JtagBus.hpp"

#include <Arduino.h>

#include <assert.h>

#include "JtagCommon.hpp"

//_____ C O N F I G S  ________________________________________________________
#define DEBUG_MODE
//_____ D E F I N I T I O N S _________________________________________________
//_____ C L A S S E S _________________________________________________________
JtagBus::JtagBus(JtagPin tms, JtagPin tdi, JtagPin tdo, JtagPin tck, JtagPin rst):
    _tms(tms), _tdi(tdi), _tdo(tdo), _tck(tck), _rst(rst)
{
  this->last_tck_micros = micros();
  this->min_tck_micros = 1;
}

JTAG::ERROR JtagBus::setSpeed(uint32_t khz)
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

uint32_t JtagBus::getSpeed() const
{
  return this->min_tck_micros;
}

void JtagBus::reset()
{
  this->_rst.setLow();
  delayMicroseconds(this->min_tck_micros);
  this->_rst.setHigh();
}

uint8_t JtagBus::clock(uint8_t tms, uint8_t tdi)
{
  assert(tms == HIGH || tms == LOW);
  assert(tdi == HIGH || tdi == LOW);

  // Setting TDI and TMS before rising edge of TCK.
  this->_tdi.setValue(tdi);
  this->_tms.setValue(tms);

#if defined(DEBUG_MODE)
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
  this->_tck.setLow();

  // Saving timestamp of last TCK change
  this->last_tck_micros = micros();

  // TDO changes on falling edge of TCK, we are reading
  // value changed during last jtag_clock.
  uint8_t tdo = this->_tdo.get();

#if defined(DEBUG_MODE)
  Serial.println(tdo);
#endif

  return tdo;
}

JTAG::ERROR JtagBus::sequence(size_t n, const uint8_t tms[], const uint8_t tdi[], uint8_t *tdo)
{
  assert(tms != nullptr);
  assert(tdi != nullptr);
  assert(tdo != nullptr);
  assert(n != 0);

  if (n > static_cast<uint32_t>(JTAG::CONSTANTS::MAX_SEQUENCE_LEN))
  {
    return JTAG::ERROR::INVALID_SEQUENCE_LEN;
  }

  for (size_t i = 0; i < n; i++)
  {
    JTAG::setBitArray(i, tdo, this->clock(JTAG::getBitArray(i, tms), JTAG::getBitArray(i, tdi)));
  }

  return JTAG::ERROR::NO;
}
