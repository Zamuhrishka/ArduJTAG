#pragma once

#include <boundary/BoundaryTypes.hpp>
#include <buffers/BitBuffer.hpp>

/**
 * @brief Stateless operations on a device's complete boundary-register vector.
 * Layout supplies BoundaryLength, Pin, cells(Pin) and initialValue(bitIndex).
 * Initial values include internal cells and concrete choices for BSDL X values.
 * Operations only edit/read buffers; JtagDeviceAccess performs JTAG exchanges.
 * Controlled outputs require distinct data/control cells and binary enable
 * polarity. Shared control cells affect all outputs connected to that control.
 */
template <typename Layout>
class BoundaryScan
{
public:
  using Pin = typename Layout::Pin;
  using Values = BitBuffer<Layout::BoundaryLength>;

  /** @brief Build the starting vector exactly as specified by the layout. */
  static Values initialValues()
  {
    Values values;
    values.resize(Layout::BoundaryLength);
    for (size_t i = 0; i < Layout::BoundaryLength; ++i) {
      values.set(i, Layout::initialValue(i));
    }
    return values;
  }

  /** @brief Set data and enable a controlled output; no change on failure. */
  template <size_t Capacity>
  static bool setOutput(BitBuffer<Capacity> &values, Pin pin, bool level)
  {
    const auto cell = Layout::cells(pin);
    if (values.bitCount() != Layout::BoundaryLength || !controlledOutput(cell)) return false;
    values.set(cell.output, level);
    values.set(cell.control, !cell.disableValue);
    return true;
  }

  /** @brief Disable an output, retaining its data bit; no change on failure. */
  template <size_t Capacity>
  static bool disableOutput(BitBuffer<Capacity> &values, Pin pin)
  {
    const auto cell = Layout::cells(pin);
    if (values.bitCount() != Layout::BoundaryLength || !controlledOutput(cell)) return false;
    values.set(cell.control, cell.disableValue);
    return true;
  }

  /** @brief Read a captured input; leave level unchanged on failure. */
  template <size_t Capacity>
  static bool readInput(const BitBuffer<Capacity> &captured, Pin pin, bool &level)
  {
    const auto cell = Layout::cells(pin);
    if (captured.bitCount() != Layout::BoundaryLength || cell.input >= Layout::BoundaryLength)
      return false;
    level = captured.getBit(cell.input);
    return true;
  }

private:
  static bool controlledOutput(const BoundaryTypes::Cells &cell)
  {
    return cell.output < Layout::BoundaryLength && cell.control < Layout::BoundaryLength &&
           cell.output != cell.control;
  }
};
