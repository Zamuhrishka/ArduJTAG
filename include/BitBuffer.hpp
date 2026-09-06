#pragma once

#include <stddef.h>
#include <stdint.h>

// Capacity is measured in bits. Storage is owned; no heap allocation is used.
// Bit zero is transmitted first; each byte is transmitted least significant bit first.
template <size_t Capacity = 256>
class BitBuffer
{
  static_assert(Capacity > 0 && Capacity <= 32767, "BitBuffer capacity must be 1..32767 bits");

public:
  BitBuffer() = default;

  static BitBuffer fromBits(const char *bits)
  {
    BitBuffer result;
    if (bits == nullptr) return result;
    size_t count = 0;
    while (bits[count] != '\0')
    {
      if (count == Capacity || (bits[count] != '0' && bits[count] != '1')) return BitBuffer();
      if (bits[count] == '1') result.storage[count / 8] |= uint8_t(1U << (count % 8));
      ++count;
    }
    result.length = count;
    return result;
  }

  // Array references accept both {0x55, 0x60} and existing byte arrays.
  // This avoids a dependency on the C++ standard library on AVR.
  template <size_t ByteCount>
  static BitBuffer fromBytes(const uint8_t (&bytes)[ByteCount])
  {
    if (ByteCount > Capacity / 8) return BitBuffer();
    return fromBytes(bytes, ByteCount, ByteCount * 8);
  }

  template <size_t ByteCount>
  static BitBuffer fromBytes(const uint8_t (&bytes)[ByteCount], size_t bits)
  {
    return fromBytes(bytes, ByteCount, bits);
  }

  // Older AVR GCC deduces integer literals as int before byte conversion.
  template <size_t ByteCount>
  static BitBuffer fromBytes(const int (&bytes)[ByteCount])
  {
    if (ByteCount > Capacity / 8) return BitBuffer();
    return fromBytes(bytes, ByteCount * 8);
  }

  template <size_t ByteCount>
  static BitBuffer fromBytes(const int (&bytes)[ByteCount], size_t bits)
  {
    BitBuffer result;
    if (bits == 0 || bits > Capacity || (bits + 7) / 8 > ByteCount) return result;
    for (size_t i = 0; i < ByteCount; ++i)
      if (bytes[i] < 0 || bytes[i] > 255) return BitBuffer();
    result.length = bits;
    for (size_t i = 0; i < result.byteCount(); ++i) result.storage[i] = uint8_t(bytes[i]);
    result.maskTail();
    return result;
  }

  // An empty braced argument cannot deduce an array length.
  static BitBuffer fromBytes(decltype(nullptr)) { return BitBuffer(); }
  static BitBuffer fromBytes(decltype(nullptr), size_t) { return BitBuffer(); }

  // byteCount describes the available source memory; bits selects its prefix.
  static BitBuffer fromBytes(const uint8_t *bytes, size_t byteCount, size_t bits)
  {
    BitBuffer result;
    if (bytes == nullptr || bits == 0 || bits > Capacity || (bits + 7) / 8 > byteCount) return result;
    result.length = bits;
    for (size_t i = 0; i < result.byteCount(); ++i) result.storage[i] = bytes[i];
    result.maskTail();
    return result;
  }

  bool valid() const { return length != 0; }
  size_t bitCount() const { return length; }
  size_t byteCount() const { return (length + 7) / 8; }
  size_t capacity() const { return Capacity; }
  const uint8_t *data() const { return storage; }
  uint8_t *data() { return storage; }
  bool getBit(size_t index) const { return index < length && ((storage[index / 8] >> (index % 8)) & 1U); }
  uint8_t byte(size_t index) const { return index < byteCount() ? storage[index] : 0; }
  bool set(size_t index, bool value)
  {
    if (index >= length) return false;
    const uint8_t mask = uint8_t(1U << (index % 8));
    if (value) storage[index / 8] |= mask;
    else storage[index / 8] &= uint8_t(~mask);
    return true;
  }

  // Changes the active length, preserving existing bits and clearing unused bits.
  bool resize(size_t bits)
  {
    if (bits == 0 || bits > Capacity) return false;
    length = bits;
    maskTail();
    for (size_t i = byteCount(); i < sizeof(storage); ++i) storage[i] = 0;
    return true;
  }

private:
  void maskTail()
  {
    if (length % 8) storage[length / 8] &= uint8_t((1U << (length % 8)) - 1U);
  }
  uint8_t storage[(Capacity + 7) / 8] = {};
  size_t length = 0;
};
