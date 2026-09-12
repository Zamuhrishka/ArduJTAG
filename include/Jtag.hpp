/**
 * \file         Jtag.hpp
 * \author       Aliaksander Kavalchuk (aliaksander.kavalchuk@gmail.com)
 * \brief        This file contains the class definition for the JTAG interface used to communicate with devices
 *               supporting the JTAG protocol.
 */

#pragma once

//_____ I N C L U D E S _______________________________________________________
#include <BitBuffer.hpp>
#include <JtagGpio.hpp>
#include <JtagCommon.hpp>
#include <Arduino.h>
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
  explicit Jtag(uint8_t tms, uint8_t tdi, uint8_t tdo, uint8_t tck, uint8_t trst):
      bus(GpioPin(tms, OUTPUT), GpioPin(tdi, OUTPUT), GpioPin(tdo, INPUT), GpioPin(tck, OUTPUT), GpioPin(trst, OUTPUT))
  {
  }

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
    uint8_t tms_pre[1] = {IR_TMS_PRE};
    uint8_t tms_post[1] = {IR_TMS_POST};
    uint8_t tdi_pre[1] = {0x00};
    uint8_t tdi_post[1] = {0x00};

    if (!instruction.valid()) {
      return JTAG::ERROR::INVALID_BUFFER;
    }

    /* Goto `Shift-IR` state */
    for (uint16_t i_seq = 0; i_seq < IR_TMS_PRE_LEN; i_seq++)
    {
      bus.clock(JTAG::getBitArray(i_seq, &tms_pre[0]), JTAG::getBitArray(i_seq, &tdi_pre[0]));
    }

    uint16_t length = instruction.bitCount();

    /* Shifting bits into IR register except last bit */
    for (uint16_t i_seq = 0; i_seq < length - 1; i_seq++)
    {
      bus.clock(0, instruction.getBit(i_seq));
    }

    /* Shifting the last bit into IR register */
    bus.clock(1, instruction.getBit(length - 1));

    /* Goto `Run-Test/Idle` state */
    for (uint16_t i_seq = 0; i_seq < IR_TMS_POST_LEN; i_seq++)
    {
      bus.clock(JTAG::getBitArray(i_seq, &tms_post[0]), JTAG::getBitArray(i_seq, &tdi_post[0]));
    }

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

    const size_t length = input.bitCount();
    output.resize(length);

    // Enter Shift-DR.
    for (size_t i = 0; i < DR_TMS_PRE_LEN; ++i) {
      bus.clock((DR_TMS_PRE >> i) & 1U, 0);
    }

    // Assert TMS on the final data bit to leave Shift-DR.
    for (size_t i = 0; i < length; ++i) {
      output.set(i, bus.clock(i == length - 1, input.getBit(i)));
    }

    // Return to Run-Test/Idle.
    for (size_t i = 0; i < DR_TMS_POST_LEN; ++i) {
      bus.clock((DR_TMS_POST >> i) & 1U, 0);
    }

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
    for (size_t i = 0; i < cycleCount; ++i) {
      tdo.set(i, bus.clock(tms.getBit(i), tdi.getBit(i)));
    }
    return JTAG::ERROR::NO;
  }

  /**
   * \brief Enter Test-Logic-Reset by generating five TCK cycles with TMS high.
   *
   * This protocol reset does not pulse the physical TRST pin.
   */
  void reset()
  {
    for (size_t i = 0; i < RESET_TMS_LEN; ++i) {
      bus.clock(1, 0);
    }
  }

  /**
   * \brief Set the Speed of the JTAG communication in kilohertz
   *
   * \param khz Desired speed in kHz
   * \return JTAG::ERROR Status of the speed setting operation
   */
  JTAG::ERROR setSpeed(uint32_t khz)
  {
    return bus.setSpeed(khz);
  }

private:
  static constexpr uint8_t RESET_TMS_LEN = 5;

  static constexpr uint8_t IR_TMS_PRE = 6;
  static constexpr uint8_t IR_TMS_POST = 1;
  static constexpr uint8_t IR_TMS_PRE_LEN = 5;
  static constexpr uint8_t IR_TMS_POST_LEN = 2;

  static constexpr uint8_t DR_TMS_PRE = 1;
  static constexpr uint8_t DR_TMS_POST = 1;
  static constexpr uint8_t DR_TMS_PRE_LEN = 3;
  static constexpr uint8_t DR_TMS_POST_LEN = 2;

  JtagGpio bus;
};
