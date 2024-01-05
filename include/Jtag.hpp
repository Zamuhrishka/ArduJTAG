/**
 * \file         Jtag.hpp
 * \author       Aliaksander Kavalchuk (aliaksander.kavalchuk@gmail.com)
 * \brief        This file contains the class definition for the JTAG interface used to communicate with devices
 *               supporting the JTAG protocol.
 */

#pragma once

//_____ I N C L U D E S _______________________________________________________
#include <JtagBus.hpp>
#include <JtagCommon.hpp>
//_____ C O N F I G S  ________________________________________________________
//_____ D E F I N I T I O N S _________________________________________________
//_____ C L A S S E S __________________________________________________________
/**
 * \brief This class represents a JTAG interface.
 *        It is used to communicate with and control devices that support JTAG (Joint Test Action Group) protocol.
 *        Note: The default constructor is deleted to prevent instantiation without parameters.
 */
class Jtag
{
public:
  Jtag() = delete;

  /**
   * \brief Construct a new Jtag object
   *        Initialize a JTAG interface with specified pin assignments.
   *
   * \param tms Pin number for Test Mode Select
   * \param tdi Pin number for Test Data In
   * \param tdo Pin number for Test Data Out
   * \param tck Pin number for Test Clock
   * \param trst Pin number for Test Reset (optional, depending on JTAG hardware)
   */
  explicit Jtag(uint8_t tms, uint8_t tdi, uint8_t tdo, uint8_t tck, uint8_t trst);

  /**
   * \brief Send an instruction through the JTAG IR (Instruction Register)
   *
   * \param instruction The instruction code to be sent
   * \param length The length of the instruction in bits
   */
  void ir(uint16_t instruction, uint16_t length);

  /**
   * \brief Send data through the JTAG DR (Data Register)
   *
   * \param data Pointer to the data array to be sent
   * \param length The length of the data in bits
   * \param output Pointer to the buffer where the response will be stored
   */
  void dr(uint8_t *data, uint32_t length, uint8_t *output);

  /**
   * \brief Perform a sequence of JTAG operations (a series of bit manipulations on TMS and TDI, reading TDO)
   *
   * \param n Number of operations in the sequence
   * \param tms Array of TMS values for the sequence
   * \param tdi Array of TDI values for the sequence
   * \param tdo Pointer to the array where TDO values will be stored
   * \return JTAG::ERROR Status of the sequence operation
   */
  JTAG::ERROR sequence(size_t n, const uint8_t tms[], const uint8_t tdi[], uint8_t *tdo);

  /**
   * \brief Resets the JTAG state machine, typically setting it to the Test-Logic-Reset state.
   *
   */
  void reset();

  /**
   * \brief Set the Speed of the JTAG communication in kilohertz
   *
   * \param khz Desired speed in kHz
   * \return JTAG::ERROR Status of the speed setting operation
   */
  JTAG::ERROR setSpeed(uint32_t khz);

private:
  JtagBus bus;
};
