/**
 * \file         JtagPin.hpp
 * \author       Aliaksander Kavalchuk (aliaksander.kavalchuk@gmail.com)
 * \brief        This file contains the prototypes for the JtagPin class which is used for managing individual pins in
 *               JTAG interface.
 */

#pragma once

//_____ I N C L U D E S _______________________________________________________
#include <stddef.h>
#include <stdint.h>
//_____ C O N F I G S  ________________________________________________________
//_____ D E F I N I T I O N S _________________________________________________
//_____ C L A S S E S __________________________________________________________
/**
 * \brief The JtagPin class is responsible for controlling a single pin used in the JTAG communication interface.
 *        It allows setting the pin high or low, pulsing, and reading its value.
 */
class JtagPin
{
public:
  JtagPin() = delete;
  explicit JtagPin(int pin, int dir);

  /**
   * \brief Set the pin to high voltage level.
   *
   */
  void setHigh();

  /**
   * \brief Set the pin to low voltage level.
   *
   */
  void setLow();

  /**
   * \brief Read the current voltage level of the pin.
   *
   * \return int Returns the pin's voltage level as an integer.
   */
  int get() const;

  /**
   * \brief Pulse the pin to high for a specified duration in microseconds.
   *
   * \param us Duration in microseconds for how long the pulse should last.
   */
  void pulseHigh(int us);

  /**
   * \brief Pulse the pin to low for a specified duration in microseconds.
   *
   * \param us Duration in microseconds for how long the pulse should last.
   */
  void pulseLow(int us);

  /**
   * \brief Set the pin's voltage level.
   *
   * \param value The value to set: high or low.
   */
  void setValue(int value);

  /**
   * \brief Assign a new pin number and direction to the JtagPin.
   *
   * \param pin The new pin number to be assigned.
   * \param dir The direction of the pin (input or output).
   */
  void assign(int pin, int dir);

  /**
   * \brief Set the direction of the pin.
   *
   * \param dir The direction to set: input or output.
   */
  void setDir(int dir);

  /**
   * \brief Get the current direction of the pin.
   *
   * \return int Returns the direction of the pin.
   */
  int getDir() const;

private:
  uint32_t pin = 0;
  uint8_t dir = 0;
};
