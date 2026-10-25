"""Validated external DAP register metadata."""
import json
from pathlib import Path
from .._json import _unique_object

PROFILE_PATH = Path(__file__).resolve().parents[1] / 'data/dap/stm32f4_adiv5.json'


def uint(value, maximum=0xFFFFFFFF):
    if isinstance(value, str):
        try:
            value = int(value, 0)
        except ValueError as error:
            raise ValueError('expected a decimal integer or 0x-prefixed hexadecimal value') from error
    if type(value) is not int or not 0 <= value <= maximum:
        raise ValueError(f'value must be in 0..{maximum}')
    return value


class DapProfile:
    def __init__(self, path=PROFILE_PATH):
        self.data = json.loads(Path(path).read_text(), object_pairs_hook=_unique_object)
        try:
            d = self.data
            if type(d['version']) is not int or d['version'] != 1 or uint(d['ir_length'], 4) != 4:
                raise ValueError('expected version 1 and four-bit JTAG-DP IR')
            for name in ('IDCODE', 'BYPASS', 'DPACC', 'APACC', 'ABORT'):
                uint(d['instructions'][name], 15)
            if len(set(d['instructions'].values())) != 5:
                raise ValueError('instruction codes must be distinct')
            for name in ('WAIT', 'OK_FAULT'):
                uint(d['ack'][name], 7)
            if d['ack']['WAIT'] == d['ack']['OK_FAULT']:
                raise ValueError('ACK codes must be distinct')
            for space, required in [('dp', ('CTRL_STAT', 'SELECT', 'RDBUFF')),
                                    ('ap', ('CSW', 'TAR', 'DRW', 'IDR'))]:
                for name in required:
                    self.reg(space, name)
                for reg in d[space].values():
                    if uint(reg['address'], 12) % 4 or reg['access'] not in ('RO', 'WO', 'RW'):
                        raise ValueError('invalid register address/access')
                    uint(reg['bank'], 15)
                    if space == 'dp' and reg['bank'] != 0:
                        raise ValueError('only unbanked STM32F4 JTAG-DP registers are supported')
                    used = 0
                    for field in reg['fields'].values():
                        lsb, width = uint(field['lsb'], 31), uint(field['width'], 32)
                        if not width or lsb + width > 32 or field['access'] not in ('RO', 'RW', 'W1C'):
                            raise ValueError('invalid field layout/access')
                        mask = ((1 << width) - 1) << lsb
                        if used & mask:
                            raise ValueError('overlapping register fields')
                        used |= mask
            for name in ('STICKYORUN', 'STICKYCMP', 'STICKYERR', 'CDBGPWRUPREQ',
                         'CDBGPWRUPACK', 'CSYSPWRUPREQ', 'CSYSPWRUPACK'):
                self.bits('dp', 'CTRL_STAT', **{name: 1})
            self.bits('dp', 'SELECT', APSEL=255, APBANKSEL=15)
            uint(d['memory']['default_ap'], 255)
            self.bits('ap', 'IDR', **d['memory']['idr_fields'])
            self.bits('ap', 'CSW', **d['memory']['csw_fields'])
            for key in d['memory']['csw_fields']:
                if self.reg('ap', 'CSW')['fields'][key]['access'] != 'RW':
                    raise ValueError('memory CSW settings must reference writable fields')
        except (KeyError, TypeError, AttributeError, ValueError) as error:
            raise ValueError(f'invalid DAP profile {path}: {error}') from error

    def reg(self, space, name, write=None):
        name = name.upper()
        if name not in self.data[space]:
            raise ValueError(f'unknown {space.upper()} register {name}')
        reg = self.data[space][name]
        if write is not None and ('W' if write else 'R') not in reg['access']:
            raise ValueError(f'{name} does not allow {"writes" if write else "reads"}')
        return reg

    def bits(self, space, name, **values):
        result = 0
        for key, value in values.items():
            field = self.reg(space, name)['fields'][key]
            result |= uint(value, (1 << field['width']) - 1) << field['lsb']
        return result

    def decode(self, space, name, value):
        return {key: (value >> f['lsb']) & ((1 << f['width']) - 1)
                for key, f in self.reg(space, name)['fields'].items()}
