#pragma once

#include <BitBuffer.hpp>
#include <JtagProfile.hpp>

// Teaching profile only: these values do NOT describe a particular device.
// Replace IR/BSR lengths, supported instructions and encodings using its BSDL.
// If a command is absent on the device, remove its call from the sketch.
struct ExampleBoundaryProfile
{
  static constexpr size_t IrLength = 4;
  static constexpr size_t BoundaryLength = 10;

  enum class Instruction : uint8_t
  {
    Extest = 0x0,
    Sample = 0x2,
    Preload = 0x2,
    SamplePreload = 0x2, // This example uses a combined SAMPLE/PRELOAD opcode.
    Intest = 0x3,
    HighZ = 0x4,
    Bypass = 0xF,
  };

  static constexpr size_t drLength(Instruction instruction)
  {
    return instruction == Instruction::HighZ || instruction == Instruction::Bypass
               ? 1 : BoundaryLength;
  }

  static BitBuffer<IrLength> encode(Instruction instruction)
  {
    switch (instruction) {
      case Instruction::Extest:
      case Instruction::SamplePreload: // Also matches Sample and Preload aliases.
      case Instruction::Intest:
      case Instruction::HighZ:
      case Instruction::Bypass:
        return BitBuffer<IrLength>::fromBytes({static_cast<uint8_t>(instruction)}, IrLength);
    }
    return BitBuffer<IrLength>();
  }
};

template <>
struct JtagInstructionProfile<ExampleBoundaryProfile::Instruction> : ExampleBoundaryProfile {};
