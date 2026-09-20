#pragma once
#define INPUT 0
#define OUTPUT 1

// Match Arduino's macro to catch public API naming conflicts.
#define bit(b) (1UL << (b))

#define LOW 0
#define HIGH 1
unsigned long micros();
void delayMicroseconds(unsigned int us);

struct TestSerial
{
  void print(unsigned char value);
  void println(unsigned char value);
};
extern TestSerial Serial;
