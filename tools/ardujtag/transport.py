"""SerialTransfer request framing and adapter commands."""
import struct
import time

VERSION = 1
STATUS = {1: 'unsupported protocol version', 2: 'unknown command',
          3: 'invalid payload length', 4: 'invalid argument',
          5: 'TAP state unknown: run reset first', 6: 'JTAG operation failed'}


def pack_bits(text, limit):
    if not text or len(text) > limit or any(c not in '01' for c in text):
        raise ValueError(f'expected 1..{limit} bits, first character shifted first')
    data = bytearray((len(text) + 7) // 8)
    for i, digit in enumerate(text):
        data[i // 8] |= int(digit) << (i % 8)
    return bytes(data)


class Client:
    def __init__(self, link, timeout=3):
        self.link, self.timeout, self.sequence = link, timeout, 0
        self.limits = None
        self.usable = True

    def request(self, command, data=b''):
        if not self.usable:
            raise RuntimeError('session unusable after communication failure; reconnect explicitly')
        sequence = self.sequence
        self.sequence = (sequence + 1) & 0xFFFF
        header = struct.pack('<BBH', VERSION, command, sequence)
        packet = header + data
        try:
            self.link.tx_buff[:len(packet)] = packet
            if not self.link.send(len(packet)):
                raise RuntimeError('serial send failed')
            deadline = time.monotonic() + self.timeout
            while time.monotonic() < deadline:
                if self.link.available():
                    response = bytes(self.link.rx_buff[:self.link.bytes_read])
                    if len(response) < 5 or response[:4] != header:
                        raise RuntimeError('unexpected response header')
                    if response[4]:
                        if len(response) != 5:
                            raise RuntimeError('invalid error response')
                        # A valid error reply completes the exchange.
                        return self._error(response[4])
                    return response[5:]
                status = getattr(self.link.status, 'value', self.link.status)
                if status <= 0:
                    raise RuntimeError(f'SerialTransfer receive error: {status}')
                time.sleep(0.001)
            raise TimeoutError('no response; command may have executed; not retried')
        except ValueError:
            raise
        except Exception:
            self.usable = False
            raise

    @staticmethod
    def _error(status):
        raise ValueError(STATUS.get(status, f'unknown adapter status {status}'))

    def info(self):
        data = self.request(1)
        if len(data) != 9:
            raise RuntimeError('invalid INFO response')
        ir, dr, clocks, speed, idle = struct.unpack('<HHHHB', data)
        self.limits = (ir, dr, clocks, speed)
        return f'protocol=1 IR={ir} DR={dr} clocks={clocks} max_kHz={speed} idle_known={bool(idle)}'

    def command(self, name, *values):
        if name == 'info' and not values:
            return self.info()
        if self.limits is None:
            self.info()
        if name == 'reset' and not values:
            command, data = 3, b''
        elif name == 'speed' and len(values) == 1:
            speed = int(values[0], 0)
            if not 1 <= speed <= self.limits[3]:
                raise ValueError(f'speed must be 1..{self.limits[3]} kHz')
            command, data = 2, struct.pack('<I', speed)
        elif name in ('ir', 'dr') and len(values) == 1:
            command = 4 if name == 'ir' else 5
            bits = values[0]
            packed = pack_bits(bits, self.limits[command - 4])
            data = struct.pack('<H', len(bits)) + packed
        elif name == 'clocks' and len(values) == 2:
            if len(values[0]) != len(values[1]):
                raise ValueError('TMS and TDI must have the same bit count')
            command, bits = 6, values[0]
            packed = b''.join(pack_bits(v, self.limits[2]) for v in values)
            data = struct.pack('<H', len(bits)) + packed
        else:
            raise ValueError('commands: info | reset | speed KHZ | ir BITS | dr BITS | clocks TMS TDI')
        reply = self.request(command, data)
        if command in (5, 6):
            count = len(bits)
            if len(reply) != 2 + (count + 7) // 8 or struct.unpack('<H', reply[:2])[0] != count:
                raise RuntimeError('invalid scan response length')
            return ''.join(str((reply[2 + i // 8] >> (i % 8)) & 1) for i in range(count))
        if reply:
            raise RuntimeError('unexpected response payload')
        return 'OK'
