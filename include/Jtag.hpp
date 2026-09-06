/**
 * \file         Jtag.hpp
 * \author       Aliaksander Kavalchuk (aliaksander.kavalchuk@gmail.com)
 * \brief        This file contains the class definition for the JTAG interface used to communicate with devices
 *               supporting the JTAG protocol.
 */

#pragma once

//_____ I N C L U D E S _______________________________________________________
#include <BitBuffer.hpp>
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
   * @brief Send the low bits of a numeric instruction through JTAG IR.
   * @param instruction Instruction value, transmitted least significant bit first.
   * @param length Number of bits to transmit, from 1 to 16. Higher bits are ignored.
   * @retval JTAG::ERROR::NO The transfer completed.
   * @retval JTAG::ERROR::INVALID_SEQUENCE_LEN The length is outside 1..16;
   *         no JTAG clocks are generated.
   * @note Delegates to the BitBuffer overload and finishes in Run-Test/Idle.
   */
  JTAG::ERROR ir(uint16_t instruction, uint16_t length);

  /**
   * @brief Send a packed instruction, including instructions longer than 16 bits.
   * @tparam Capacity Maximum instruction buffer capacity in bits.
   * @param[in] instruction Valid buffer in transmission order. Its active bit
   *                        count determines the instruction length.
   * @retval JTAG::ERROR::NO The transfer completed.
   * @retval JTAG::ERROR::INVALID_BUFFER The buffer is empty; no clocks are generated.
   * @note Bytes are sent in array order, least significant bit first. TMS is
   *       asserted on the last data bit to leave Shift-IR; the transfer finishes
   *       in Run-Test/Idle. The input buffer is unchanged and TDO is discarded.
   */
  template <size_t Capacity>
  JTAG::ERROR ir(const BitBuffer<Capacity> &instruction)
  {
    if (!instruction.valid()) {
      return JTAG::ERROR::INVALID_BUFFER;
    }
    shiftIr(instruction.data(), instruction.bitCount());
    return JTAG::ERROR::NO;
  }

  /**
   * \brief Send a bit buffer through the JTAG DR and capture the response.
   *
   * \tparam InputCapacity Maximum input buffer capacity in bits.
   * \tparam OutputCapacity Maximum output buffer capacity in bits.
   * \param[in] input Valid buffer containing the bits to transmit on TDI.
   * \param[out] output Buffer receiving the bits sampled from TDO. Its capacity
   *                    must be at least input.bitCount(); its active length is
   *                    set to input.bitCount() before the transfer.
   * \retval JTAG::ERROR::NO The transfer completed.
   * \retval JTAG::ERROR::INVALID_BUFFER The input is empty or the output
   *         capacity is insufficient. No JTAG clocks are generated and output
   *         is left unchanged.
   *
   * \note Bits are transferred in BitBuffer order: bytes in array order,
   *       least significant bit first within each byte. Exactly input.bitCount()
   *       data bits are shifted, including any partial final byte.
   */
  template <size_t InputCapacity, size_t OutputCapacity>
  JTAG::ERROR dr(const BitBuffer<InputCapacity> &input, BitBuffer<OutputCapacity> &output)
  {
    if (!input.valid() || input.bitCount() > output.capacity()) {
      return JTAG::ERROR::INVALID_BUFFER;
    }

    output.resize(input.bitCount());
    dr(input.data(), input.bitCount(), output.data());

    return JTAG::ERROR::NO;
  }

  /**
   * @brief Generate TCK cycles from packed TMS and TDI buffers and capture TDO.
   * @tparam TmsCapacity Maximum TMS buffer capacity in bits.
   * @tparam TdiCapacity Maximum TDI buffer capacity in bits.
   * @tparam TdoCapacity Maximum TDO buffer capacity in bits.
   * @param[in] tms Valid TMS buffer; its active length determines the cycle count.
   * @param[in] tdi Valid TDI buffer with the same active length as tms.
   * @param[out] tdo Response buffer with capacity for every cycle. Its active
   *                 length is set to the input length after validation.
   * @retval JTAG::ERROR::NO The transfer completed.
   * @retval JTAG::ERROR::INVALID_BUFFER An input is empty, input lengths differ,
   *         or the output capacity is insufficient.
   * @retval JTAG::ERROR::INVALID_SEQUENCE_LEN The cycle count exceeds
   *         JTAG::CONSTANTS::MAX_SEQUENCE_LEN.
   * @note All validation precedes changes to tdo and generation of JTAG clocks.
   *       On validation failure, buffers are unchanged. Bits follow array order,
   *       least significant bit first in each byte, including a partial last byte.
   */
  template <size_t TmsCapacity, size_t TdiCapacity, size_t TdoCapacity>
  JTAG::ERROR clockCycles(const BitBuffer<TmsCapacity> &tms,
                          const BitBuffer<TdiCapacity> &tdi,
                          BitBuffer<TdoCapacity> &tdo)
  {
    const size_t cycleCount = tms.bitCount();

    if (!tms.valid() || !tdi.valid() || cycleCount != tdi.bitCount() ||
        cycleCount > tdo.capacity()) {
      return JTAG::ERROR::INVALID_BUFFER;
    }

    if (cycleCount > static_cast<uint32_t>(JTAG::CONSTANTS::MAX_SEQUENCE_LEN)) {
      return JTAG::ERROR::INVALID_SEQUENCE_LEN;
    }

    tdo.resize(cycleCount);
    return bus.clockCycles(cycleCount, tms.data(), tdi.data(), tdo.data());
  }

  /**
   * \brief Enter Test-Logic-Reset by generating five TCK cycles with TMS high.
   *
   * This protocol reset does not pulse the physical TRST pin.
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
  /**
   * @brief Shift validated packed instruction bits and return to Run-Test/Idle.
   * @param instruction Readable packed bytes for all requested bits.
   * @param length Nonzero bit count within the supported BitBuffer capacity.
   */
  void shiftIr(const uint8_t *instruction, size_t length);

  /**
   * \brief Send data through the JTAG DR (Data Register)
   *
   * \param data Pointer to the data array to be sent
   * \param length The length of the data in bits
   * \param output Pointer to the buffer where the response will be stored
   */
  void dr(const uint8_t *data, uint32_t length, uint8_t *output);



  JtagBus bus;
};
