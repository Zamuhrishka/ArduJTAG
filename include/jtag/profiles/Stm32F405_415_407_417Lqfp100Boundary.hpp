#pragma once

#include <jtag/boundary/BoundaryTypes.hpp>
#include <jtag/profiles/Stm32F405_415_407_417Lqfp100.hpp>

/**
 * @brief Package-specific BSR layout from the same ST BSDL V1.1 as Stm32F405_415_407_417Lqfp100.
 * Cell numbers are BitBuffer indices (cell 0 is shifted first).
 * Only ports with boundary cells are listed; power, reset and TAP pins are not.
 * Internal cells remain part of the 406-bit vector. This layout describes data;
 * BoundaryScan<Layout> implements buffer operations.
 */
struct Stm32F405_415_407_417Lqfp100Boundary
{
  using Profile = Stm32F405_415_407_417Lqfp100;
  static constexpr size_t BoundaryLength = Profile::BoundaryLength;
  static constexpr uint16_t NoCell = BoundaryTypes::NoCell;
  using Cells = BoundaryTypes::Cells;

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

  static Cells cells(Pin pin)
  {
    switch (pin) {
      case Pin::Pe2: return {403, 404, 405, 1, true};
      case Pin::Pe3: return {400, 401, 402, 2, true};
      case Pin::Pe4: return {397, 398, 399, 3, true};
      case Pin::Pe5: return {394, 395, 396, 4, true};
      case Pin::Pe6: return {391, 392, 393, 5, true};
      case Pin::Pc13: return {385, 386, 387, 7, true};
      case Pin::Pc14: return {382, 383, 384, 8, true};
      case Pin::Pc15: return {379, 380, 381, 9, true};
      case Pin::Ph0: return {334, 335, 336, 12, true};
      case Pin::Ph1: return {331, 332, 333, 13, true};
      case Pin::Pc0: return {328, 329, 330, 15, true};
      case Pin::Pc1: return {325, 326, 327, 16, true};
      case Pin::Pc2: return {322, 323, 324, 17, true};
      case Pin::Pc3: return {319, 320, 321, 18, true};
      case Pin::Pa0: return {316, 317, 318, 23, true};
      case Pin::Pa1: return {313, 314, 315, 24, true};
      case Pin::Pa2: return {310, 311, 312, 25, true};
      case Pin::Pa3: return {295, 296, 297, 26, true};
      case Pin::Pa4: return {292, 293, 294, 29, true};
      case Pin::Pa5: return {289, 290, 291, 30, true};
      case Pin::Pa6: return {286, 287, 288, 31, true};
      case Pin::Pa7: return {283, 284, 285, 32, true};
      case Pin::Pc4: return {280, 281, 282, 33, true};
      case Pin::Pc5: return {277, 278, 279, 34, true};
      case Pin::Pb0: return {274, 275, 276, 35, true};
      case Pin::Pb1: return {271, 272, 273, 36, true};
      case Pin::Pb2: return {268, 269, 270, 37, true};
      case Pin::Pe7: return {244, 245, 246, 38, true};
      case Pin::Pe8: return {241, 242, 243, 39, true};
      case Pin::Pe9: return {238, 239, 240, 40, true};
      case Pin::Pe10: return {235, 236, 237, 41, true};
      case Pin::Pe11: return {232, 233, 234, 42, true};
      case Pin::Pe12: return {229, 230, 231, 43, true};
      case Pin::Pe13: return {226, 227, 228, 44, true};
      case Pin::Pe14: return {223, 224, 225, 45, true};
      case Pin::Pe15: return {220, 221, 222, 46, true};
      case Pin::Pb10: return {217, 218, 219, 47, true};
      case Pin::Pb11: return {214, 215, 216, 48, true};
      case Pin::Pb12: return {190, 191, 192, 51, true};
      case Pin::Pb13: return {187, 188, 189, 52, true};
      case Pin::Pb14: return {184, 185, 186, 53, true};
      case Pin::Pb15: return {181, 182, 183, 54, true};
      case Pin::Pd8: return {178, 179, 180, 55, true};
      case Pin::Pd9: return {175, 176, 177, 56, true};
      case Pin::Pd10: return {172, 173, 174, 57, true};
      case Pin::Pd11: return {169, 170, 171, 58, true};
      case Pin::Pd12: return {166, 167, 168, 59, true};
      case Pin::Pd13: return {163, 164, 165, 60, true};
      case Pin::Pd14: return {160, 161, 162, 61, true};
      case Pin::Pd15: return {157, 158, 159, 62, true};
      case Pin::Pc6: return {133, 134, 135, 63, true};
      case Pin::Pc7: return {130, 131, 132, 64, true};
      case Pin::Pc8: return {127, 128, 129, 65, true};
      case Pin::Pc9: return {124, 125, 126, 66, true};
      case Pin::Pa8: return {121, 122, 123, 67, true};
      case Pin::Pa9: return {118, 119, 120, 68, true};
      case Pin::Pa10: return {115, 116, 117, 69, true};
      case Pin::Pa11: return {112, 113, 114, 70, true};
      case Pin::Pa12: return {109, 110, 111, 71, true};
      case Pin::Pc10: return {85, 86, 87, 78, true};
      case Pin::Pc11: return {82, 83, 84, 79, true};
      case Pin::Pc12: return {79, 80, 81, 80, true};
      case Pin::Pd0: return {76, 77, 78, 81, true};
      case Pin::Pd1: return {73, 74, 75, 82, true};
      case Pin::Pd2: return {70, 71, 72, 83, true};
      case Pin::Pd3: return {67, 68, 69, 84, true};
      case Pin::Pd4: return {64, 65, 66, 85, true};
      case Pin::Pd5: return {61, 62, 63, 86, true};
      case Pin::Pd6: return {58, 59, 60, 87, true};
      case Pin::Pd7: return {55, 56, 57, 88, true};
      case Pin::Pb5: return {31, 32, 33, 91, true};
      case Pin::Pb6: return {28, 29, 30, 92, true};
      case Pin::Pb7: return {25, 26, 27, 93, true};
      case Pin::Boot0: return {24, NoCell, NoCell, 94, true};
      case Pin::Pb8: return {21, 22, 23, 95, true};
      case Pin::Pb9: return {18, 19, 20, 96, true};
      case Pin::Pe0: return {15, 16, 17, 97, true};
      case Pin::Pe1: return {12, 13, 14, 98, true};
      default: return {NoCell, NoCell, NoCell, 0, true};
    }
  }

  /**
   * @brief BSDL starting bit: control cells 1, internal cells 0, X chosen as 0.
   * Defined for indices below BoundaryLength; returns false otherwise.
   * This describes the vector, not board-level safety. NRST must be low.
   */
  static bool initialValue(size_t index)
  {
    switch (index) {
      case 405:
      case 402:
      case 399:
      case 396:
      case 393:
      case 387:
      case 384:
      case 381:
      case 336:
      case 333:
      case 330:
      case 327:
      case 324:
      case 321:
      case 318:
      case 315:
      case 312:
      case 297:
      case 294:
      case 291:
      case 288:
      case 285:
      case 282:
      case 279:
      case 276:
      case 273:
      case 270:
      case 246:
      case 243:
      case 240:
      case 237:
      case 234:
      case 231:
      case 228:
      case 225:
      case 222:
      case 219:
      case 216:
      case 192:
      case 189:
      case 186:
      case 183:
      case 180:
      case 177:
      case 174:
      case 171:
      case 168:
      case 165:
      case 162:
      case 159:
      case 135:
      case 132:
      case 129:
      case 126:
      case 123:
      case 120:
      case 117:
      case 114:
      case 111:
      case 87:
      case 84:
      case 81:
      case 78:
      case 75:
      case 72:
      case 69:
      case 66:
      case 63:
      case 60:
      case 57:
      case 33:
      case 30:
      case 27:
      case 23:
      case 20:
      case 17:
      case 14:
        return true;
      default: return false;
    }
  }
};
