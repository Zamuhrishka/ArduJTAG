"""ADIv5 JTAG-DP exchange, power-up and single-word AHB-AP access."""
import time
from .profile import uint

COMMANDS = {'dap-init', 'dp-read', 'dp-write', 'ap-read', 'ap-write',
            'mem-read32', 'mem-write32'}


class ArmJtagDp:
    def __init__(self, tap, profile, attempts=100):
        self.tap, self.profile, self.attempts = tap, profile, attempts
        self.initialized = False

    def invalidate(self):
        self.initialized = False

    def scan(self, ap, address, read, value=0):
        p = self.profile.data
        opcode = p['instructions']['APACC' if ap else 'DPACC']
        self.tap.select(f'{opcode:04b}'[::-1])
        # Response belongs to the previous request. WAIT discards this request;
        # repeat it unchanged, never replay a request after acceptance.
        request = int(read) | ((address >> 2) << 1) | (value << 3)
        bits = f'{request:035b}'[::-1]
        for _ in range(self.attempts):
            response = self.tap.scan(bits)
            if len(response) != 35 or set(response) - {'0', '1'}:
                raise RuntimeError('malformed JTAG-DP response')
            result = int(response[::-1], 2)
            ack = result & 7
            if ack == p['ack']['OK_FAULT']:
                return result >> 3
            if ack != p['ack']['WAIT']:
                raise RuntimeError(f'invalid JTAG-DP ACK: {ack}')
            time.sleep(0.001)
        raise RuntimeError('JTAG-DP WAIT timeout; request completion is uncertain')

    def access(self, ap, reg, read, value=0):
        self.scan(ap, reg['address'], read, value)
        # Drain the pipeline with a harmless RDBUFF read. This is JTAG-DP,
        # not the SWD AP-read/RDBUFF data-return convention.
        return self.scan(False, self.profile.reg('dp', 'RDBUFF')['address'], True)

    def dp(self, name, read=True, value=0):
        return self.access(False, self.profile.reg('dp', name, not read), read, value)

    def status(self):
        status = self.dp('CTRL_STAT')
        fields = self.profile.decode('dp', 'CTRL_STAT', status)
        errors = [k for k in ('STICKYORUN', 'STICKYCMP', 'STICKYERR') if fields[k]]
        if errors:
            raise RuntimeError(f'DP sticky error ({", ".join(errors)}), CTRL_STAT=0x{status:08X}; run dap-init to recover')
        return status

    def ap(self, index, name, read=True, value=0):
        reg = self.profile.reg('ap', name, not read)
        select = self.profile.bits('dp', 'SELECT', APSEL=index, APBANKSEL=reg['bank'])
        self.dp('SELECT', False, select)
        result = self.access(True, reg, read, value)
        self.status()  # ACK=OK/FAULT alone does not establish success.
        return result

    def initialize(self):
        self.invalidate()
        self.tap.read_idcode()  # Verify the target before power requests.
        self.dp('SELECT', False, 0)
        request = self.profile.bits('dp', 'CTRL_STAT', CDBGPWRUPREQ=1, CSYSPWRUPREQ=1,
                                    STICKYORUN=1, STICKYCMP=1, STICKYERR=1)
        self.dp('CTRL_STAT', False, request)  # JTAG sticky flags are W1C.
        for _ in range(self.attempts):
            fields = self.profile.decode('dp', 'CTRL_STAT', self.status())
            if fields['CDBGPWRUPACK'] and fields['CSYSPWRUPACK']:
                self.initialized = True
                return 'DAP ready'
            time.sleep(0.01)
        raise RuntimeError('DAP power-up timeout')

    def memory(self, address, read, value, index):
        identity = self.profile.decode('ap', 'IDR', self.ap(index, 'IDR'))
        if any(identity[k] != v for k, v in self.profile.data['memory']['idr_fields'].items()):
            raise RuntimeError('selected AP is not the supported AHB MEM-AP')
        csw = self.profile.bits('ap', 'CSW', **self.profile.data['memory']['csw_fields'])
        self.ap(index, 'CSW', False, csw)
        actual = self.profile.decode('ap', 'CSW', self.ap(index, 'CSW'))
        if any(actual[k] != v for k, v in self.profile.data['memory']['csw_fields'].items()):
            raise RuntimeError('AHB-AP did not accept the requested CSW configuration')
        self.ap(index, 'TAR', False, address)
        return self.ap(index, 'DRW', read, value)

    def command(self, name, *args):
        # Validate user arguments before any scan, including AP selection.
        if name in ('dp-read', 'dp-write', 'ap-read', 'ap-write'):
            space = name[:2]
            write = name.endswith('write')
            index = uint(args[0], 255) if space == 'ap' else None
            regname = args[1] if space == 'ap' else args[0]
            self.profile.reg(space, regname, write)
            value = uint(args[-1]) if write else 0
        elif name in ('mem-read32', 'mem-write32'):
            write = name == 'mem-write32'
            address = uint(args[0])
            if address % 4:
                raise ValueError('memory address must be aligned to four bytes')
            value = uint(args[1]) if write else 0
            index = uint(self.profile.data['memory']['default_ap'] if args[-1] is None else args[-1], 255)
        elif name != 'dap-init':
            raise ValueError(f'unknown DAP command {name}')
        self.tap.check_limits(35)
        if name != 'dap-init' and not self.initialized:
            raise ValueError('DAP is not initialized; run dap-init first')
        try:
            if name == 'dap-init':
                return self.initialize()
            if name.startswith('dp-'):
                result = self.dp(regname, not write, value)
                if write:
                    self.status()
                    if regname.upper() == 'CTRL_STAT':
                        self.invalidate()  # Power/error policy may have changed.
            elif name.startswith('ap-'):
                result = self.ap(index, regname, not write, value)
            else:
                result = self.memory(address, not write, value, index)
            return 'OK' if write else f'0x{result:08X}'
        except (RuntimeError, OSError, ValueError):
            self.invalidate()
            raise
