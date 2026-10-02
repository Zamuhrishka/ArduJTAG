#pragma once

#include <jtag/buffers/BitBuffer.hpp>
#include <jtag/chain/JtagProfile.hpp>

/**
 * @brief BoundaryScan TAP for STM32F405/415/407/417 in LQFP100.
 * Source: STMicroelectronics STM32F405_415_407_417_LQFP100.bsd,
 * V1.1, 2014-05-30, supplied with this profile's implementation request.
 * This is not the ARM Debug TAP. Keep NRST low for the boundary-scan
 * behavior described by the BSDL; NRST is distinct from JTRST.
 */
struct Stm32F405_415_407_417Lqfp100
{
  static constexpr size_t IrLength = 5;
  static constexpr size_t BoundaryLength = 406;
  static constexpr uint32_t IdcodeValue = 0x06413041UL;
  static constexpr uint32_t IdcodeMask = 0x0FFFFFFFUL;
  static constexpr uint8_t InstructionCaptureValue = 0x01;
  static constexpr uint8_t InstructionCaptureMask = 0x03;
  static constexpr uint32_t MaxTckHz = 10000000UL;
  static constexpr bool RequiresNrstLow = true;

  enum class Instruction : uint8_t
  {
    Extest = 0x00,
    Idcode = 0x01,
    Sample = 0x02,
    Preload = 0x02,
    SamplePreload = 0x02,
    Bypass = 0x1F,
  };

  static constexpr size_t drLength(Instruction instruction)
  {
    return instruction == Instruction::Extest || instruction == Instruction::Sample
               ? BoundaryLength : instruction == Instruction::Idcode ? 32 :
                 instruction == Instruction::Bypass ? 1 : 0;
  }

  static BitBuffer<IrLength> encode(Instruction instruction)
  {
    switch (instruction) {
      case Instruction::Extest:
      case Instruction::Idcode:
      case Instruction::Sample: // Also matches Preload and SamplePreload.
      case Instruction::Bypass:
        return BitBuffer<IrLength>::fromBytes({static_cast<uint8_t>(instruction)}, IrLength);
    }
    return BitBuffer<IrLength>();
  }

  /** @brief Match the part/manufacturer bits, ignoring the four revision bits. */
  static constexpr bool matchesIdcode(uint32_t idcode)
  {
    return (idcode & IdcodeMask) == IdcodeValue;
  }
};

template <>
struct JtagInstructionProfile<Stm32F405_415_407_417Lqfp100::Instruction> : Stm32F405_415_407_417Lqfp100 {};
