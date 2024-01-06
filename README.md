# ArduJtag: Simple Library for Working with JTAG

ArduJtag is a lightweight and easy-to-use library for interfacing with JTAG devices using Arduino boards. This library provides a convenient way to communicate with JTAG compatible devices for debugging, programming, or testing purposes. It is designed to be as intuitive and straightforward as possible, making it ideal for both beginners and experienced users.

## Features

- Control JTAG pins (TCK, TMS, TDI, TDO) directly from your Arduino
- Simple API for sending and receiving data
- Support for JTAG sequences and operations
- Configurable communication speed

## Install

- Clone this repository into Arduino/Libraries or use the built-in Arduino IDE Library manager to install a copy of this library.

- Include in your sketch

```c
#include "Jtag.hpp"
```

### Install Using PlatformIO

Install ArduJtag using the platformio library manager in your editor, or using the PlatformIO Core CLI, or by adding it to your platformio.ini as shown below:

```shell
[env]
lib_deps =
    ArduJtag
[env]
lib_deps =
    https://github.com/zamuhrishka/ArduJtag.git
```

## Usage

- Create object

```c
Jtag jtag = Jtag(TMS, TDI, TDO, TCK, RST);
```

- Call neccessory methods

```c
jtag.reset();
```

Also see [examples](./examples/).

## Data Buffer Format

The method `dr(uint8_t *data, uint32_t length, uint8_t *output)` accepts arrays of type `uint8_t` for input and output data. The data is organized in a specific way. For example, to write the following bit sequence to the DR register:

```c
uint32_t A = 0x07F8F00F;
```

|A[31]|A[30]|A[29]|A[28]|A[27]|A[26]|A[25]|A[24]|A[23]|A[22]|A[21]|A[20]|A[19]|A[18]|A[17]|A[16]|A[15]|A[14]|A[13]|A[12]|A[11]|A[10]|A[9]|A[8]|A[7]|A[6]|A[5]|A[4]|A[3]|A[2]|A[1]|A[0]|
|---|---|---|---|---|---|---|---|---|---|---|---|---|---|---|---|---|---|---|---|---|---|---|---|---|---|---|---|---|---|---|---|
|0|0|0|0|0|1|1|1|1|1|1|1|1|0|0|0|1|1|1|1|0|0|0|0|0|0|0|0|1|1|1|1|

You would represent it in the data array as follows:

```c
uint8_t data[] = {0x0F, 0xF0, 0xF8, 0x07};
```

data[0]:

|A[7]|A[6]|A[5]|A[4]|A[3]|A[2]|A[1]|A[0]|
|---|---|---|---|---|---|---|---|
|1|1|1|1|0|0|0|0|

data[1]:

|A[15]|A[14]|A[13]|A[12]|A[11]|A[10]|A[9]|A[8]|
|---|---|---|---|---|---|---|---|
|0|0|0|0|1|1|1|1|

data[2]:

|A[23]|A[22]|A[21]|A[20]|A[19]|A[18]|A[17]|A[16]|
|---|---|---|---|---|---|---|---|
|0|0|0|1|1|1|1|1|

data[3]:

|A[31]|A[30]|A[29]|A[28]|A[27]|A[26]|A[25]|A[24]|
|---|---|---|---|---|---|---|---|
|1|1|1|0|0|0|0|0|

This representation is also true for the output array.

## Contributing

Bug reports and/or pull requests are welcome.

## Disclaimer

This software is provided 'as-is', without any express or implied warranty. In no event will the authors be held liable for any damages arising from the use of this software.

## License

Permission is granted to anyone to use this software for any purpose, including commercial applications.
