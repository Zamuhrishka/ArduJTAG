/**
 * \file         JtagPin.cpp
 * \author       Aliaksander Kavalchuk (aliaksander.kavalchuk@gmail.com)
 * \brief        This file contains the prototypes for the JtagPin class which is used for managing individual pins in
 *               JTAG interface.
 */

//_____ I N C L U D E S _______________________________________________________
#include "JtagPin.hpp"

#include <Arduino.h>

#include <assert.h>
//_____ C O N F I G S  ________________________________________________________
//_____ D E F I N I T I O N S _________________________________________________
//_____ C L A S S E S _________________________________________________________
JtagPin::JtagPin(int pin, int dir)
{
  assert(dir == INPUT || dir == OUTPUT);
  this->assign(pin, dir);
}

void JtagPin::setHigh()
{
  this->setValue(HIGH);
}

void JtagPin::setLow()
{
  this->setValue(LOW);
}

int JtagPin::get() const
{
  assert(this->dir == INPUT);
  return digitalRead(this->pin);
}

void JtagPin::pulseHigh(int us)
{
  this->setHigh();
  delayMicroseconds(us);
  this->setLow();
}

void JtagPin::pulseLow(int us)
{
  this->setHigh();
  delayMicroseconds(us);
  this->setLow();
}

void JtagPin::setValue(int value)
{
  assert(value == LOW || value == HIGH);
  assert(this->dir == OUTPUT);

  digitalWrite(this->pin, value);
}

void JtagPin::assign(int pin, int dir)
{
  assert(dir == INPUT || dir == OUTPUT);

  this->pin = pin;
  this->dir = dir;

  digitalWrite(this->pin, LOW);
  pinMode(this->pin, this->dir);
}

void JtagPin::setDir(int dir)
{
  assert(dir == INPUT || dir == OUTPUT);
  this->dir = dir;
}

int JtagPin::getDir() const
{
  return this->dir;
}
