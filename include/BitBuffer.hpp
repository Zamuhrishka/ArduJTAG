#pragma once

/**
 * @file BitBuffer.hpp
 * @brief Fixed-capacity storage for JTAG bit sequences.
 */

#include <stddef.h>
#include <stdint.h>

/** @brief Compile-time capacity defaults and limits for BitBuffer. */
namespace BitBufferConfig
{
  constexpr size_t DefaultCapacity = 256; ///< Default capacity in bits.
  constexpr size_t MaxCapacity = 32767; ///< Maximum supported capacity in bits.
}

/**
 * @brief Owns a packed bit sequence without heap allocation.
 * @tparam Capacity Maximum number of bits, from 1 to 32767 (default: 256).
 *
 * Bit zero is stored in the least significant bit of the first byte and is
 * transmitted first. Bytes follow array order, least significant bit first
 * within each byte. The active length may be smaller than the capacity and
 * need not be a multiple of eight.
 *
 * Default construction and failed factory calls produce an empty buffer for
 * which valid() is false. Factories copy their input into owned storage.
 */
template <size_t Capacity = BitBufferConfig::DefaultCapacity>
class BitBuffer
{
  static_assert(Capacity > 0 && Capacity <= BitBufferConfig::MaxCapacity, "BitBuffer capacity must be 1..32767 bits");

  static constexpr size_t BitsPerByte = 8; ///< Number of bits in a uint8_t storage element.

public:
  /** @brief Creates an empty, invalid buffer with zero-initialized storage. */
  BitBuffer() = default;

  /**
   * @brief Copies a textual bit sequence in transmission order.
   * @param bits Null-terminated string containing only '0' and '1'.
   * @return A buffer containing the sequence, or an empty, invalid buffer if
   *         the input is null, empty, contains other characters, or exceeds Capacity.
   * @note The leftmost character is bit zero: "10000000" stores byte 0x01.
   */
  static BitBuffer fromBits(const char *bits)
  {
    BitBuffer result;
    size_t count = 0;

    if (bits == nullptr) {
      return result;
    }

    while (bits[count] != '\0')
    {
      if (count == Capacity || (bits[count] != '0' && bits[count] != '1')) {
        return BitBuffer();
      }

      if (bits[count] == '1') {
        result.storage[count / BitsPerByte] |= uint8_t(1U << (count % BitsPerByte));
      }
      ++count;
    }
    result.length = count;
    return result;
  }

  /**
   * @brief Copies all bits from a byte array.
   * @tparam ByteCount Number of source bytes, deduced from the array.
   * @param bytes Source array, in transmission order.
   * @return A buffer of ByteCount * BitsPerByte bits, or an empty, invalid buffer if
   *         that length exceeds Capacity.
   * @note Array references support existing arrays and braced lists without
   *       requiring the C++ standard library on AVR.
   */
  template <size_t ByteCount>
  static BitBuffer fromBytes(const uint8_t (&bytes)[ByteCount])
  {
    if (ByteCount > Capacity / BitsPerByte) {
      return BitBuffer();
    }
    return fromBytes(bytes, ByteCount, ByteCount * BitsPerByte);
  }

  /**
   * @brief Copies a bit prefix from a byte array.
   * @tparam ByteCount Number of available source bytes, deduced from the array.
   * @param bytes Source array, in transmission order.
   * @param bits Number of bits to copy; must be in 1..Capacity and fit the array.
   * @return The copied prefix, or an empty, invalid buffer for an invalid length.
   * @note Unused high bits in the final byte are cleared.
   */
  template <size_t ByteCount>
  static BitBuffer fromBytes(const uint8_t (&bytes)[ByteCount], size_t bits)
  {
    return fromBytes(bytes, ByteCount, bits);
  }

  /**
   * @brief Copies all bytes from an integer array after validating their range.
   * @tparam ByteCount Number of source elements, deduced from the array.
   * @param bytes Source values, each in 0..255, in transmission order.
   * @return A buffer of ByteCount * BitsPerByte bits, or an empty, invalid buffer if
   *         the length exceeds Capacity or any value is outside 0..255.
   * @note Supports older AVR GCC versions that deduce braced integer literals
   *       as int before byte conversion.
   */
  template <size_t ByteCount>
  static BitBuffer fromBytes(const int (&bytes)[ByteCount])
  {
    if (ByteCount > Capacity / BitsPerByte) {
      return BitBuffer();
    }
    return fromBytes(bytes, ByteCount * BitsPerByte);
  }

  /**
   * @brief Copies a bit prefix from an integer array after validating its values.
   * @tparam ByteCount Number of available source elements, deduced from the array.
   * @param bytes Source values in transmission order. Every element, including
   *              elements outside the selected prefix, must be in 0..255.
   * @param bits Number of bits to copy; must be in 1..Capacity and fit the array.
   * @return The copied prefix, or an empty, invalid buffer for an invalid
   *         length or source value.
   * @note Unused high bits in the final byte are cleared.
   */
  template <size_t ByteCount>
  static BitBuffer fromBytes(const int (&bytes)[ByteCount], size_t bits)
  {
    BitBuffer result;

    if (bits == 0 || bits > Capacity || (bits + BitsPerByte - 1) / BitsPerByte > ByteCount) {
      return result;
    }

    for (size_t i = 0; i < ByteCount; ++i) {
      if (bytes[i] < 0 || bytes[i] > UINT8_MAX) {
        return BitBuffer();
      }
    }

    result.length = bits;

    for (size_t i = 0; i < result.byteCount(); ++i) {
      result.storage[i] = uint8_t(bytes[i]);
    }

    result.maskTail();

    return result;
  }

  /**
   * @brief Handles null input or an empty braced list, whose array length cannot be deduced.
   * @return An empty, invalid buffer.
   */
  static BitBuffer fromBytes(decltype(nullptr)) { return BitBuffer(); }
  /**
   * @brief Handles null input or an empty braced list with any requested bit count.
   * @return An empty, invalid buffer; the bit count is ignored.
   */
  static BitBuffer fromBytes(decltype(nullptr), size_t) { return BitBuffer(); }

  /**
   * @brief Copies a bit prefix from a source memory region.
   * @param bytes Pointer to source bytes in transmission order.
   * @param byteCount Number of readable bytes available at bytes.
   * @param bits Number of bits to copy; must be in 1..Capacity and require
   *             no more than byteCount bytes.
   * @return The copied prefix, or an empty, invalid buffer if bytes is null
   *         or the requested length is invalid.
   * @note Only the required bytes are copied. Unused high bits in the final
   *       byte are cleared; the source memory is not retained.
   */
  static BitBuffer fromBytes(const uint8_t *bytes, size_t byteCount, size_t bits)
  {
    BitBuffer result;

    if (bytes == nullptr || bits == 0 || bits > Capacity || (bits + BitsPerByte - 1) / BitsPerByte > byteCount) {
      return result;
    }

    result.length = bits;

    for (size_t i = 0; i < result.byteCount(); ++i) {
      result.storage[i] = bytes[i];
    }

    result.maskTail();

    return result;
  }

  /** @return True if the active bit count is nonzero. */
  bool valid() const { return length != 0; }

  /** @return Number of active bits. */
  size_t bitCount() const { return length; }

  /** @return Number of bytes covering the active bits, rounded up. */
  size_t byteCount() const { return (length + BitsPerByte - 1) / BitsPerByte; }

  /** @return Maximum number of bits the buffer can hold. */
  size_t capacity() const { return Capacity; }

  /**
   * @brief Provides read-only access to the owned packed storage.
   * @return Pointer valid for the lifetime of this object; byteCount() bytes
   *         cover the active sequence, including any partial final byte.
   */
  const uint8_t *data() const { return storage; }

  /**
   * @brief Provides mutable access to the owned packed storage.
   * @return Pointer valid for the lifetime of this object.
   * @note Direct writes do not change bitCount() or check bounds. The caller
   *       must stay within the allocated (Capacity + BitsPerByte - 1) / BitsPerByte bytes and keep
   *       unused bits zero to preserve zero-filled growth through resize().
   */
  uint8_t *data() { return storage; }

  /**
   * @brief Reads a bit using its zero-based transmission index.
   * @param index Bit index in the active sequence.
   * @return The bit value, or false if index is outside the active sequence.
   */
  bool getBit(size_t index) const { return index < length && ((storage[index / BitsPerByte] >> (index % BitsPerByte)) & 1U); }

  /**
   * @brief Reads a packed byte.
   * @param index Zero-based byte index.
   * @return The stored byte, or zero if index is at least byteCount().
   */
  uint8_t byte(size_t index) const { return index < byteCount() ? storage[index] : 0; }

  /**
   * @brief Changes an existing bit without extending the sequence.
   * @param index Zero-based bit index in transmission order.
   * @param value New bit value.
   * @return True on success; false if index is outside the active sequence,
   *         leaving the buffer unchanged.
   */
  bool set(size_t index, bool value)
  {
    if (index >= length) {
      return false;
    }

    const uint8_t mask = uint8_t(1U << (index % BitsPerByte));

    if (value) {
      storage[index / BitsPerByte] |= mask;
    } else {
      storage[index / BitsPerByte] &= uint8_t(~mask);
    }

    return true;
  }

  /**
   * @brief Changes the active length and clears storage beyond the new length.
   * @param bits New active bit count, in 1..Capacity.
   * @return True on success; false for an invalid length, leaving the buffer unchanged.
   *
   * Bits within both the old and new lengths are preserved. Shrinking discards
   * the removed bits. Growing exposes zero bits provided unused storage has
   * not been modified through data(). Resizing an empty buffer makes it valid.
   * @note A length of zero is rejected; this method cannot clear the buffer
   *       to the empty state.
   */
  bool resize(size_t bits)
  {
    if (bits == 0 || bits > Capacity) {
      return false;
    }

    length = bits;
    maskTail();

    for (size_t i = byteCount(); i < sizeof(storage); ++i) {
      storage[i] = 0;
    }

    return true;
  }

private:
  /** @brief Clears unused high bits of the last active byte, if it is partial. */
  void maskTail()
  {
    if (length % BitsPerByte) storage[length / BitsPerByte] &= uint8_t((1U << (length % BitsPerByte)) - 1U);
  }

  uint8_t storage[(Capacity + BitsPerByte - 1) / BitsPerByte] = {}; ///< Owned bytes, packed least significant bit first.
  size_t length = 0; ///< Active bit count; zero denotes an empty, invalid buffer.
};
