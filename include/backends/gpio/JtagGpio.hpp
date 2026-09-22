/**
 * \brief        This file contains the prototypes and definition for the JtagGpio class which manages the JTAG
 *               communication bus.
 */

#pragma once

//_____ I N C L U D E S _______________________________________________________
#include <Arduino.h>
#include <assert.h>
#include <core/JtagError.hpp>
#include <core/JtagTypes.hpp>
#include <backends/gpio/GpioPin.hpp>
//_____ C O N F I G S  ________________________________________________________
//_____ D E F I N I T I O N S _________________________________________________
//_____ C L A S S E S __________________________________________________________
/**
 * \internal
 * \brief GPIO and timing implementation used internally by Jtag.
 *
 * Generates clock edges, samples TDO, and controls physical TRST.
 * TAP transitions and buffer validation belong to Jtag. Applications should
 * include Jtag.hpp and use Jtag; this class is not a supported public API.
 */
class JtagGpio
{
public:
  JtagGpio() = delete;
  explicit JtagGpio(GpioPin tms, GpioPin tdi, GpioPin tdo, GpioPin tck, GpioPin rst);

  /**
   * \brief Set the communication speed of the JTAG bus.
   *
   * \param khz The speed in kilohertz.
   * \return JTAG::ERROR Error status of the speed setting operation.
   */
  JTAG::ERROR setSpeed(uint32_t khz);

  /**
   * \brief Get the current communication speed of the JTAG bus.
   *
   * \return uint32_t The speed in kilohertz.
   */
  uint32_t getSpeed() const;

  /**
   * \brief Pulse the active-low TRST pin to perform a hardware TAP reset.
   *
   * Holds TRST low for the configured minimum half-period, then releases it
   * high. Requires a connected TRST pin and generates no TCK cycles.
   * \see Jtag::reset() for the reset performed with TMS and TCK.
   */
  void pulseTrst();

  /**
   * \brief Perform a single clock cycle on the JTAG bus, optionally modifying the TMS and TDI lines.
   *
   * \param tms The value to set on the TMS line (1 or 0).
   * \param tdi The value to set on the TDI line (1 or 0).
   * \return uint8_t The read value from the TDO line after the clock cycle.
   */
  uint8_t clock(uint8_t tms, uint8_t tdi);

private:
  uint32_t last_tck_micros = 0;  // Timestamp of the last clock pulse, used for timing calculations.
  uint32_t min_tck_micros = 1;   // Minimum duration of one TCK (clock) pulse, used to enforce speed limits.
  GpioPin _tms;
  GpioPin _tdi;
  GpioPin _tdo;
  GpioPin _tck;
  GpioPin _rst;
};

inline JtagGpio::JtagGpio(GpioPin tms, GpioPin tdi, GpioPin tdo, GpioPin tck, GpioPin rst):
    _tms(tms), _tdi(tdi), _tdo(tdo), _tck(tck), _rst(rst)
{
  this->last_tck_micros = micros();
  this->min_tck_micros = 1;
}

inline JTAG::ERROR JtagGpio::setSpeed(uint32_t khz)
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

inline uint32_t JtagGpio::getSpeed() const
{
  return this->min_tck_micros;
}

inline void JtagGpio::pulseTrst()
{
  this->_rst.setLow();
  delayMicroseconds(this->min_tck_micros);
  this->_rst.setHigh();
}

inline uint8_t JtagGpio::clock(uint8_t tms, uint8_t tdi)
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
