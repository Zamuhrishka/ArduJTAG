"""Shared per-TAP scans and whole-chain state coordination."""
from .idcode import format_idcode
from .boundary import BoundarySession
from .dap.jtag_dp import ArmJtagDp, COMMANDS


class TapSession:
    """Scan one TAP while all other TAPs use BYPASS."""
    def __init__(self, client, bsdl, lengths, target):
        self.client, self.bsdl = client, bsdl
        self.lengths, self.target = lengths, target
        self.invalidate()

    def invalidate(self):
        self.ready = False
        self.mode = None

    def check_limits(self, bits):
        if sum(self.lengths) > self.client.limits[0] or bits + len(self.lengths) - 1 > self.client.limits[1]:
            raise ValueError('whole-chain scan exceeds adapter limits')

    def exchange(self, *args):
        try:
            return self.client.command(*args)
        except (ValueError, RuntimeError, OSError) as error:
            self.invalidate()
            raise RuntimeError(f'JTAG exchange failed; session state invalidated: {error}') from error

    def select(self, instruction):
        if not self.ready:
            self.exchange('reset')
            self.ready = True
        ir = ''.join(instruction if i == self.target else '1' * self.lengths[i]
                     for i in reversed(range(len(self.lengths))))
        self.exchange('ir', ir)

    def scan(self, bits):
        offset = len(self.lengths) - self.target - 1
        result = self.exchange('dr', '0' * offset + bits + '0' * self.target)
        return result[offset:offset + len(bits)]

    def read_idcode(self):
        if self.bsdl is None:
            raise ValueError('TAP has no BSDL for IDCODE validation')
        opcode = self.bsdl.instruction('IDCODE', 'DEVICE_ID')
        if self.bsdl.id_pattern is None:
            raise ValueError('BSDL has no IDCODE_REGISTER')
        self.check_limits(32)
        self.select(opcode)
        self.mode = 'IDCODE'
        value = int(self.scan('0' * 32)[::-1], 2)
        pattern = self.bsdl.id_pattern
        mask = int(''.join('0' if b == 'X' else '1' for b in pattern), 2)
        expected = int(pattern.replace('X', '0'), 2)
        if value & mask != expected:
            raise ValueError(f'IDCODE mismatch: 0x{value:08X} (BSDL {pattern})')
        return format_idcode(value)


class _TapTransport:
    """Coordinate effects of per-target IR scans on the chain."""
    def __init__(self, owner, index):
        self.owner, self.index = owner, index

    @property
    def limits(self):
        return self.owner.client.limits

    def command(self, name, *args):
        try:
            result = self.owner.client.command(name, *args)
        except (ValueError, RuntimeError, OSError):
            self.owner.invalidate()
            raise
        if name == 'reset':
            self.owner.ready = True
            for session in self.owner.taps:
                if session is not None:
                    session.ready, session.mode = True, None
        elif name == 'ir':
            for i, session in enumerate(self.owner.taps):
                if session is not None and i != self.index:
                    session.mode = 'BYPASS'
        return result


class ChainSession:
    """One physical chain, with independent retained vectors for described TAPs."""
    def __init__(self, client, config):
        self.client, self.config = client, config
        self.lengths = [tap.ir_length for tap in config.taps]
        if sum(self.lengths) > client.limits[0] or len(self.lengths) > client.limits[1]:
            raise ValueError('whole-chain scan exceeds adapter limits')
        self.names = [tap.name for tap in config.taps]
        self.target = self.names.index(config.default_target)
        self.taps = [TapSession(_TapTransport(self, i), tap.bsdl, self.lengths, i)
                     for i, tap in enumerate(config.taps)]
        self.sessions = [BoundarySession(self.taps[i], tap.aliases) if tap.bsdl else None
                         for i, tap in enumerate(config.taps)]
        self.daps = [ArmJtagDp(self.taps[i], tap.dap) if tap.dap else None
                     for i, tap in enumerate(config.taps)]
        self.ready = False

    def invalidate(self):
        self.ready = False
        for dap in self.daps:
            if dap is not None:
                dap.invalidate()
        for tap in self.taps:
            tap.invalidate()
        for session in self.sessions:
            if session is not None:
                session.invalidate()

    def command(self, name, *args):
        if name == 'target':
            if args[0] not in self.names:
                raise ValueError(f'unknown TAP {args[0]}; available: {", ".join(self.names)}')
            self.target = self.names.index(args[0])
            return f'Target: {args[0]}'  # Host selection only: no clocks or mode changes.
        if name == 'targets':
            return '\n'.join(f'{"*" if i == self.target else " "} {tap.name}: IR={tap.ir_length}, '
                             f'{tap.bsdl.name if tap.bsdl else "BYPASS only"}'
                             for i, tap in enumerate(self.config.taps))
        if name in ('reset', 'ir', 'dr', 'clocks'):
            self.invalidate()
            return self.client.command(name, *args)
        if name in ('info', 'speed'):
            return self.client.command(name, *args)
        if name in COMMANDS:
            dap = self.daps[self.target]
            if dap is None:
                raise ValueError('selected TAP has no DAP protocol; configure protocol: stm32f4-adiv5')
            try:
                return dap.command(name, *args)
            except (RuntimeError, OSError):
                self.invalidate()
                raise
        # A non-DAP scan may change the active instruction or pipeline.
        for dap in self.daps:
            if dap is not None:
                dap.invalidate()
        if name == 'bypass':
            transport = _TapTransport(self, self.target)
            if not self.ready:
                transport.command('reset')
            transport.command('ir', '1' * sum(self.lengths))
            for tap in self.taps:
                tap.mode = 'BYPASS'
            return 'OK'
        session = self.sessions[self.target]
        if session is None:
            raise ValueError(f'TAP {self.names[self.target]} has no BSDL; only BYPASS and raw operations are available')
        try:
            return session.command(name, *args)
        except (RuntimeError, OSError):
            self.invalidate()
            raise
