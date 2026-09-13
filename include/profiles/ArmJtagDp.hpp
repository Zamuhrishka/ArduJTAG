#pragma once

#include <BitBuffer.hpp>
#include <JtagProfile.hpp>

/**
 * @brief Standard ARM JTAG-DP instructions for a four-bit IR.
 * Eight-bit IR variants are not covered by this profile.
 * Encodings: Arm CoreSight SoC-400 TRM, JTAG-DP register summary.
 * This describes IR codes and DR widths, not payload formats, ACK handling or pipelining.
 */
struct ArmJtagDp
{
  static constexpr size_t IrLength = 4;

  enum class Instruction : uint8_t
  {
    Abort = 0x8,
    Dpacc = 0xA,
    Apacc = 0xB,
    Idcode = 0xE,
    Bypass = 0xF,
  };

  /** @brief Required target DR width; zero denotes an unsupported command here. */
  static constexpr size_t drLength(Instruction instruction)
  {
    return instruction == Instruction::Abort || instruction == Instruction::Dpacc ||
           instruction == Instruction::Apacc ? 35 :
           instruction == Instruction::Idcode ? 32 :
           instruction == Instruction::Bypass ? 1 : 0;
  }

  /** @brief Encode a supported command; invalid enum values yield an empty buffer. */
  static BitBuffer<IrLength> encode(Instruction instruction)
  {
    switch (instruction) {
      case Instruction::Abort:
      case Instruction::Dpacc:
      case Instruction::Apacc:
      case Instruction::Idcode:
      case Instruction::Bypass:
        return BitBuffer<IrLength>::fromBytes({static_cast<uint8_t>(instruction)}, IrLength);
    }
    return BitBuffer<IrLength>();
  }
};

template <>
struct JtagInstructionProfile<ArmJtagDp::Instruction> : ArmJtagDp {};
