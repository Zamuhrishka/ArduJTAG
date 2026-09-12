/**
 * \file         Jtag.hpp
 * \author       Aliaksander Kavalchuk (aliaksander.kavalchuk@gmail.com)
 * \brief        This file contains the class definition for the JTAG interface used to communicate with devices
 *               supporting the JTAG protocol.
 */

//_____ I N C L U D E S _______________________________________________________
#include "Jtag.hpp"

#include <Arduino.h>

#include "JtagBus.hpp"
#include "JtagCommon.hpp"

#include <assert.h>
//_____ C O N F I G S  ________________________________________________________
//_____ D E F I N I T I O N S _________________________________________________
const uint8_t RESET_TMS_LEN = 5;

const uint8_t IR_TMS_PRE = 6;
const uint8_t IR_TMS_POST = 1;
const uint8_t IR_TMS_PRE_LEN = 5;
const uint8_t IR_TMS_POST_LEN = 2;

const uint8_t DR_TMS_PRE = 1;
const uint8_t DR_TMS_POST = 1;
const uint8_t DR_TMS_PRE_LEN = 3;
const uint8_t DR_TMS_POST_LEN = 2;
//_____ C L A S S E S __________________________________________________________
Jtag::Jtag(uint8_t tms, uint8_t tdi, uint8_t tdo, uint8_t tck, uint8_t rst):
    bus(JtagPin(tms, OUTPUT), JtagPin(tdi, OUTPUT), JtagPin(tdo, INPUT), JtagPin(tck, OUTPUT), JtagPin(rst, OUTPUT))
{
}

JTAG::ERROR Jtag::ir(uint16_t instruction, uint16_t length)
{
  constexpr size_t InstructionBits = 16; // Width of the numeric instruction argument.
  if (length == 0 || length > InstructionBits) {
    return JTAG::ERROR::INVALID_SEQUENCE_LEN;
  }

  BitBuffer<InstructionBits> bits;
  bits.resize(length);
  for (size_t i = 0; i < length; ++i) {
    bits.set(i, (instruction >> i) & 1U);
  }
  return ir(bits);
}

void Jtag::shiftIr(const uint8_t *instruction, size_t length)
{
  assert(length != 0);

  uint8_t tms_pre[1] = {IR_TMS_PRE};
  uint8_t tms_post[1] = {IR_TMS_POST};
  uint8_t tdi_pre[1] = {0x00};
  uint8_t tdi_post[1] = {0x00};

  /* Goto `Shift-IR` state */
  for (uint16_t i_seq = 0; i_seq < IR_TMS_PRE_LEN; i_seq++)
  {
    this->bus.clock(JTAG::getBitArray(i_seq, &tms_pre[0]), JTAG::getBitArray(i_seq, &tdi_pre[0]));
  }

  /* Shifting bits into IR register except last bit */
  for (uint16_t i_seq = 0; i_seq < length - 1; i_seq++)
  {
    this->bus.clock(0, JTAG::getBitArray(i_seq, instruction));
  }

  /* Shifting the last bit into IR register */
  this->bus.clock(1, JTAG::getBitArray(length - 1, instruction));

  /* Goto `Run-Test/Idle` state */
  for (uint16_t i_seq = 0; i_seq < IR_TMS_POST_LEN; i_seq++)
  {
    this->bus.clock(JTAG::getBitArray(i_seq, &tms_post[0]), JTAG::getBitArray(i_seq, &tdi_post[0]));
  }
}

void Jtag::dr(const uint8_t *data, uint32_t length, uint8_t *output)
{
  assert(data != nullptr);
  assert(length != 0);

  uint8_t tms_pre[1] = {DR_TMS_PRE};
  uint8_t tms_post[1] = {DR_TMS_POST};
  uint8_t tdi_pre[1] = {0x00};
  uint8_t tdi_post[1] = {0x00};
  uint16_t bit_offset = 0;
  uint8_t tdo = 0;

  /* Goto `Shift-DR` state */
  for (uint16_t i_seq = 0; i_seq < DR_TMS_PRE_LEN; i_seq++)
  {
    this->bus.clock(JTAG::getBitArray(i_seq, &tms_pre[0]), JTAG::getBitArray(i_seq, &tdi_pre[0]));
  }

  /* Shifting bits into DR register except last bit */
  for (uint16_t i_seq = 0; i_seq < length - 1; i_seq++, bit_offset++)
  {
    tdo = this->bus.clock(0, JTAG::getBitArray(i_seq, data));

    if (output != nullptr)
    {
      JTAG::setBitArray(bit_offset, &output[0], tdo);
    }
  }

  /* Shifting the last bit into DR register */
  tdo = this->bus.clock(1, JTAG::getBitArray(length - 1, data));
  if (output != nullptr)
  {
    JTAG::setBitArray(bit_offset, &output[0], tdo);
  }

  // The unused high bits of a partial final byte are always zero.
  if (output != nullptr && length % 8 != 0)
    output[length / 8] &= uint8_t((1U << (length % 8)) - 1U);

  /* Goto `Run-Test/Idle` state */
  for (uint16_t i_seq = 0; i_seq < DR_TMS_POST_LEN; i_seq++)
  {
    this->bus.clock(JTAG::getBitArray(i_seq, &tms_post[0]), JTAG::getBitArray(i_seq, &tdi_post[0]));
  }
}

void Jtag::reset()
{
  for (size_t i = 0; i < RESET_TMS_LEN; ++i) {
    bus.clock(1, 0);
  }
}

JTAG::ERROR Jtag::setSpeed(uint32_t khz)
{
  return this->bus.setSpeed(khz);
}
