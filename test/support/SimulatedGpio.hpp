#pragma once

#include <Arduino.h>
#include <stdint.h>

// Each protocol suite supplies the response and trace for one sampled clock.
static uint8_t simulateClock(uint8_t tms, uint8_t tdi);

// Protocol suites use Jtag(1, 2, 3, 4, 5): TMS, TDI, TDO, TCK, TRST.
namespace SimulatedGpio
{
  static uint8_t levels[6] = {};
  static unsigned long nowMicros = 0;
}

inline void pinMode(unsigned int, int) {}
inline void digitalWrite(unsigned int pin, int value)
{
  SimulatedGpio::levels[pin] = value;
}
inline int digitalRead(unsigned int)
{
  return simulateClock(SimulatedGpio::levels[1], SimulatedGpio::levels[2]);
}
inline unsigned long micros() { return SimulatedGpio::nowMicros; }
inline void delayMicroseconds(unsigned int us) { SimulatedGpio::nowMicros += us; }
