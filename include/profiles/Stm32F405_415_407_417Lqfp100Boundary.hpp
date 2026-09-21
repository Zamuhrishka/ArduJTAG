#pragma once

#include <profiles/Stm32F405_415_407_417Lqfp100.hpp>

/**
 * @brief Package-specific BSR layout from the same ST BSDL V1.1 as Stm32F405_415_407_417Lqfp100.
 * Cell numbers are BitBuffer indices (cell 0 is shifted first).
 * Only ports with boundary cells are listed; power, reset and TAP pins are not.
 * Internal cells remain part of the 406-bit vector. No runtime mapping table.
 */
struct Stm32F405_415_407_417Lqfp100Boundary
{
  using Profile = Stm32F405_415_407_417Lqfp100;
  using Values = BitBuffer<Profile::BoundaryLength>;
  static constexpr uint16_t NoCell = 0xFFFF;

  enum class Pin : uint8_t
  {
    Pe2,
    Pe3,
    Pe4,
    Pe5,
    Pe6,
    Pc13,
    Pc14,
    Pc15,
    Ph0,
    Ph1,
    Pc0,
    Pc1,
    Pc2,
    Pc3,
    Pa0,
    Pa1,
    Pa2,
    Pa3,
    Pa4,
    Pa5,
    Pa6,
    Pa7,
    Pc4,
    Pc5,
    Pb0,
    Pb1,
    Pb2,
    Pe7,
    Pe8,
    Pe9,
    Pe10,
    Pe11,
    Pe12,
    Pe13,
    Pe14,
    Pe15,
    Pb10,
    Pb11,
    Pb12,
    Pb13,
    Pb14,
    Pb15,
    Pd8,
    Pd9,
    Pd10,
    Pd11,
    Pd12,
    Pd13,
    Pd14,
    Pd15,
    Pc6,
    Pc7,
    Pc8,
    Pc9,
    Pa8,
    Pa9,
    Pa10,
    Pa11,
    Pa12,
    Pc10,
    Pc11,
    Pc12,
    Pd0,
    Pd1,
    Pd2,
    Pd3,
    Pd4,
    Pd5,
    Pd6,
    Pd7,
    Pb5,
    Pb6,
    Pb7,
    Boot0,
    Pb8,
    Pb9,
    Pe0,
    Pe1,
    Count
  };

  struct Cells
  {
    uint16_t input;
    uint16_t output;
    uint16_t control;
    uint8_t packagePin; // Physical LQFP100 lead, not a Discovery connector pin.
    bool valid() const { return input != NoCell; }
    bool hasOutput() const { return output != NoCell && control != NoCell; }
  };

  static Cells cells(Pin pin)
  {
    switch (pin) {
      case Pin::Pe2: return {403, 404, 405, 1};
      case Pin::Pe3: return {400, 401, 402, 2};
      case Pin::Pe4: return {397, 398, 399, 3};
      case Pin::Pe5: return {394, 395, 396, 4};
      case Pin::Pe6: return {391, 392, 393, 5};
      case Pin::Pc13: return {385, 386, 387, 7};
      case Pin::Pc14: return {382, 383, 384, 8};
      case Pin::Pc15: return {379, 380, 381, 9};
      case Pin::Ph0: return {334, 335, 336, 12};
      case Pin::Ph1: return {331, 332, 333, 13};
      case Pin::Pc0: return {328, 329, 330, 15};
      case Pin::Pc1: return {325, 326, 327, 16};
      case Pin::Pc2: return {322, 323, 324, 17};
      case Pin::Pc3: return {319, 320, 321, 18};
      case Pin::Pa0: return {316, 317, 318, 23};
      case Pin::Pa1: return {313, 314, 315, 24};
      case Pin::Pa2: return {310, 311, 312, 25};
      case Pin::Pa3: return {295, 296, 297, 26};
      case Pin::Pa4: return {292, 293, 294, 29};
      case Pin::Pa5: return {289, 290, 291, 30};
      case Pin::Pa6: return {286, 287, 288, 31};
      case Pin::Pa7: return {283, 284, 285, 32};
      case Pin::Pc4: return {280, 281, 282, 33};
      case Pin::Pc5: return {277, 278, 279, 34};
      case Pin::Pb0: return {274, 275, 276, 35};
      case Pin::Pb1: return {271, 272, 273, 36};
      case Pin::Pb2: return {268, 269, 270, 37};
      case Pin::Pe7: return {244, 245, 246, 38};
      case Pin::Pe8: return {241, 242, 243, 39};
      case Pin::Pe9: return {238, 239, 240, 40};
      case Pin::Pe10: return {235, 236, 237, 41};
      case Pin::Pe11: return {232, 233, 234, 42};
      case Pin::Pe12: return {229, 230, 231, 43};
      case Pin::Pe13: return {226, 227, 228, 44};
      case Pin::Pe14: return {223, 224, 225, 45};
      case Pin::Pe15: return {220, 221, 222, 46};
      case Pin::Pb10: return {217, 218, 219, 47};
      case Pin::Pb11: return {214, 215, 216, 48};
      case Pin::Pb12: return {190, 191, 192, 51};
      case Pin::Pb13: return {187, 188, 189, 52};
      case Pin::Pb14: return {184, 185, 186, 53};
      case Pin::Pb15: return {181, 182, 183, 54};
      case Pin::Pd8: return {178, 179, 180, 55};
      case Pin::Pd9: return {175, 176, 177, 56};
      case Pin::Pd10: return {172, 173, 174, 57};
      case Pin::Pd11: return {169, 170, 171, 58};
      case Pin::Pd12: return {166, 167, 168, 59};
      case Pin::Pd13: return {163, 164, 165, 60};
      case Pin::Pd14: return {160, 161, 162, 61};
      case Pin::Pd15: return {157, 158, 159, 62};
      case Pin::Pc6: return {133, 134, 135, 63};
      case Pin::Pc7: return {130, 131, 132, 64};
      case Pin::Pc8: return {127, 128, 129, 65};
      case Pin::Pc9: return {124, 125, 126, 66};
      case Pin::Pa8: return {121, 122, 123, 67};
      case Pin::Pa9: return {118, 119, 120, 68};
      case Pin::Pa10: return {115, 116, 117, 69};
      case Pin::Pa11: return {112, 113, 114, 70};
      case Pin::Pa12: return {109, 110, 111, 71};
      case Pin::Pc10: return {85, 86, 87, 78};
      case Pin::Pc11: return {82, 83, 84, 79};
      case Pin::Pc12: return {79, 80, 81, 80};
      case Pin::Pd0: return {76, 77, 78, 81};
      case Pin::Pd1: return {73, 74, 75, 82};
      case Pin::Pd2: return {70, 71, 72, 83};
      case Pin::Pd3: return {67, 68, 69, 84};
      case Pin::Pd4: return {64, 65, 66, 85};
      case Pin::Pd5: return {61, 62, 63, 86};
      case Pin::Pd6: return {58, 59, 60, 87};
      case Pin::Pd7: return {55, 56, 57, 88};
      case Pin::Pb5: return {31, 32, 33, 91};
      case Pin::Pb6: return {28, 29, 30, 92};
      case Pin::Pb7: return {25, 26, 27, 93};
      case Pin::Boot0: return {24, NoCell, NoCell, 94};
      case Pin::Pb8: return {21, 22, 23, 95};
      case Pin::Pb9: return {18, 19, 20, 96};
      case Pin::Pe0: return {15, 16, 17, 97};
      case Pin::Pe1: return {12, 13, 14, 98};
      default: return {NoCell, NoCell, NoCell, 0};
    }
  }

  /**
   * @brief All output drivers disabled: control cells 1, internal cells 0.
   * BSDL X values are chosen as zero. This is a starting vector, not a guarantee
   * of board-level safety. NRST must be low as required by the BSDL.
   */
  static Values initialValues()
  {
    Values values;
    values.resize(Profile::BoundaryLength);
    for (uint8_t i = 0; i < static_cast<uint8_t>(Pin::Count); ++i) {
      const Cells cell = cells(static_cast<Pin>(i));
      if (cell.hasOutput()) values.set(cell.control, true);
    }
    return values;
  }

  /** @brief Set output data and enable its driver (control = 0). No JTAG clocks. */
  template <size_t Capacity>
  static bool setOutput(BitBuffer<Capacity> &values, Pin pin, bool level)
  {
    const Cells cell = cells(pin);
    if (values.bitCount() != Profile::BoundaryLength || !cell.hasOutput()) return false;
    values.set(cell.output, level);
    values.set(cell.control, false);
    return true;
  }

  /** @brief Disable an output driver (control = 1), retaining its data bit. */
  template <size_t Capacity>
  static bool disableOutput(BitBuffer<Capacity> &values, Pin pin)
  {
    const Cells cell = cells(pin);
    if (values.bitCount() != Profile::BoundaryLength || !cell.hasOutput()) return false;
    values.set(cell.control, true);
    return true;
  }

  /** @brief Read a captured input cell; leave level unchanged on invalid input. */
  template <size_t Capacity>
  static bool readInput(const BitBuffer<Capacity> &captured, Pin pin, bool &level)
  {
    const Cells cell = cells(pin);
    if (captured.bitCount() != Profile::BoundaryLength || !cell.valid()) return false;
    level = captured.getBit(cell.input);
    return true;
  }
};
