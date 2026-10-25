"""Boundary vectors, standard instructions and pin operations."""


class BoundarySession:
    """Owns the intended output vector; captured inputs never replace it."""
    def __init__(self, tap, aliases=None):
        self.tap, self.bsdl = tap, tap.bsdl
        self.aliases = aliases or {}
        self.vector = None

    @property
    def mode(self):
        return self.tap.mode

    @mode.setter
    def mode(self, value):
        self.tap.mode = value

    @property
    def ready(self):
        return self.tap.ready

    def invalidate(self):
        self.tap.invalidate()
        self.vector = None

    def boundary(self, name, vector):
        opcode = self.bsdl.instruction(name, 'BOUNDARY')
        self.tap.check_limits(self.bsdl.boundary_length)
        # Selecting EXTEST/INTEST immediately activates preloaded outputs.
        if name in ('EXTEST', 'INTEST') and self.mode != name:
            preload = self.bsdl.instruction('PRELOAD', 'BOUNDARY')
            self.tap.select(preload)
            self.tap.scan(''.join(vector))
        self.tap.select(opcode)
        captured = self.tap.scan(''.join(vector))
        self.vector, self.mode = list(vector), name
        return captured

    def command(self, name, *args):
        if name in ('reset', 'ir', 'dr', 'clocks'):
            self.invalidate()
            return self.tap.client.command(name, *args)
        if name in ('info', 'speed'):
            return self.tap.client.command(name, *args)
        try:
            return self._command(name, *args)
        except (RuntimeError, OSError):
            self.invalidate()
            raise

    def _command(self, name, *args):
        if name == 'idcode':
            return self.tap.read_idcode()
        if name == 'bypass':
            opcode = self.bsdl.instruction('BYPASS', 'BYPASS')
            self.tap.check_limits(1)
            self.tap.select(opcode)
            self.mode = 'BYPASS'
            return 'OK'
        if name not in ('sample', 'preload', 'extest', 'intest', 'pin-read', 'pin-write', 'pin-disable'):
            raise ValueError(f'unknown command {name}')
        # Validate the complete layout and command before touching TAP.
        vector = self.bsdl.initial_vector() if self.vector is None else list(self.vector)
        if name.startswith('pin-'):
            pin = self.aliases.get(args[0].upper(), args[0].upper())
            if name == 'pin-read':
                index, _ = self.bsdl.pin_cell(pin, 'INPUT')
                mode = self.mode if self.mode in ('EXTEST', 'INTEST') else 'SAMPLE'
                return self.boundary(mode, vector)[index]
            index, cell = self.bsdl.pin_cell(pin, 'OUTPUT3')
            shared = [c for c in self.bsdl.cells.values() if c.function == 'OUTPUT3' and c.control == cell.control]
            if len(shared) != 1:
                raise ValueError(f'{pin}: shared output control is not supported for pin operations')
            if name == 'pin-write':
                if args[1] not in ('0', '1'):
                    raise ValueError('pin value must be 0 or 1')
                vector[index] = args[1]
                vector[cell.control] = '1' if cell.disable == '0' else '0'
            else:
                vector[cell.control] = cell.disable
            self.boundary('EXTEST', vector)
            return 'OK'
        # SAMPLE/PRELOAD explicitly leave EXTEST, returning pins to normal control.
        captured = self.boundary(name.upper(), vector)
        return captured if name in ('sample', 'intest') else 'OK'
