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

Метод `dr(uint8_t *data, uint32_t length, uint8_t *output)` принимает в качестве в качестве вхожных и выходных данных для передачи массив типа `uint8_t` и в его заполнении есть определенные особенности. Например если необходимо записать в регистр `DR` следующую последовательность бит:

```shell

[0][1][2][3][4][5][6][7]    [8][9][10][11][12][13][14][15]  [16][17][18][19][20][21][22][23]    [24][25][26][27][28][29][30][31]
1   0  1  0  1  0  1  0     0  0  0   1   1    1   1   1    0   0   0   0   0   0   0   0       1   1   1   1   1   1   1   1
```

то массив `data` будет выглячдить следуюшим образом:

data[0]:
data[1]:
data[2]:
data[3]:

data[0]     data[1]     data[2]         data[3]
11111111    00000000    01111100         001010101

То же самое врено и для массива `output`.


## Contributing

Bug reports and/or pull requests are welcome.

## Disclaimer

This software is provided 'as-is', without any express or implied warranty. In no event will the authors be held liable for any damages arising from the use of this software.

## License

Permission is granted to anyone to use this software for any purpose, including commercial applications.
