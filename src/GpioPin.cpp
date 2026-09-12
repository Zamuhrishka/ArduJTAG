/**
 * \file         GpioPin.cpp
 * \author       Aliaksander Kavalchuk (aliaksander.kavalchuk@gmail.com)
 * \brief        This file contains the prototypes for the GpioPin class which is used for managing individual pins in
 *               JTAG interface.
 */

//_____ I N C L U D E S _______________________________________________________
#include "GpioPin.hpp"

#include <Arduino.h>

#include <assert.h>
//_____ C O N F I G S  ________________________________________________________
//_____ D E F I N I T I O N S _________________________________________________
//_____ C L A S S E S _________________________________________________________
GpioPin::GpioPin(int pin, int dir)
{
  assert(dir == INPUT || dir == OUTPUT);
  this->assign(pin, dir);
}

void GpioPin::setHigh()
{
  this->setValue(HIGH);
}

void GpioPin::setLow()
{
  this->setValue(LOW);
}

int GpioPin::get() const
{
  assert(this->dir == INPUT);
  return digitalRead(this->pin);
}

void GpioPin::pulseHigh(int us)
{
  this->setHigh();
  delayMicroseconds(us);
  this->setLow();
}

void GpioPin::pulseLow(int us)
{
  this->setHigh();
  delayMicroseconds(us);
  this->setLow();
}

void GpioPin::setValue(int value)
{
  assert(value == LOW || value == HIGH);
  assert(this->dir == OUTPUT);

  digitalWrite(this->pin, value);
}

void GpioPin::assign(int pin, int dir)
{
  assert(dir == INPUT || dir == OUTPUT);

  this->pin = pin;
  this->dir = dir;

  digitalWrite(this->pin, LOW);
  pinMode(this->pin, this->dir);
}

void GpioPin::setDir(int dir)
{
  assert(dir == INPUT || dir == OUTPUT);
  this->dir = dir;
}

int GpioPin::getDir() const
{
  return this->dir;
}
