/**
 * \file         JtagBus.hpp
 * \author       Aliaksander Kavalchuk (aliaksander.kavalchuk@gmail.com)
 * \brief        This file contains the prototypes and definition for the JtagBus class which manages the JTAG
 *               communication bus.
 */

#pragma once

//_____ I N C L U D E S _______________________________________________________
#include <JtagCommon.hpp>
#include <JtagPin.hpp>
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
class JtagBus
{
public:
  JtagBus() = delete;
  explicit JtagBus(JtagPin tms, JtagPin tdi, JtagPin tdo, JtagPin tck, JtagPin rst);

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

  /**
   * \brief Generate TCK cycles, driving TMS and TDI and sampling TDO.
   *
   * \param cycleCount Number of TCK cycles to generate.
   * \param tms Array of TMS values for the sequence.
   * \param tdi Array of TDI values for the sequence.
   * \param tdo Pointer to the array where TDO values will be stored.
   * \return JTAG::ERROR Error status of the sequence operation.
   */
  JTAG::ERROR clockCycles(size_t cycleCount, const uint8_t tms[], const uint8_t tdi[], uint8_t *tdo);

private:
  uint32_t last_tck_micros = 0;  // Timestamp of the last clock pulse, used for timing calculations.
  uint32_t min_tck_micros = 1;   // Minimum duration of one TCK (clock) pulse, used to enforce speed limits.
  JtagPin _tms;
  JtagPin _tdi;
  JtagPin _tdo;
  JtagPin _tck;
  JtagPin _rst;
};
