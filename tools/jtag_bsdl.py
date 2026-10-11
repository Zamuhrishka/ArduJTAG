"""BSDL metadata and host-side boundary scan (literal IEEE 1149.1 subset).

This is deliberately not a VHDL evaluator: unsupported expressions and cell
semantics are rejected rather than guessed. Strings in BSDL are MSB first;
all strings handed to the transport are in first-bit-on-wire order.
"""
from dataclasses import dataclass
from pathlib import Path
from functools import lru_cache
import json
import re
import sys


VENDORS_PATH = Path(__file__).resolve().parent / 'data' / 'jtag_vendors.json'


@lru_cache(maxsize=1)
def load_vendors():
    """Load the optional name table once per process, independently of cwd."""
    try:
        data = json.loads(VENDORS_PATH.read_text(encoding='utf-8-sig'),
                          object_pairs_hook=_unique_object)
        if not isinstance(data, dict):
            raise ValueError('expected an object mapping hexadecimal codes to names')
        vendors = {}
        for key, name in data.items():
            if not re.fullmatch(r'0x[0-9A-Fa-f]{3}', key) or int(key, 16) > 0x7FF:
                raise ValueError(f'invalid 11-bit vendor code: {key}')
            code = int(key, 16)
            if code in vendors or not isinstance(name, str) or not name.strip():
                raise ValueError(f'duplicate code or invalid vendor name: {key}')
            vendors[code] = name.strip()
        return vendors
    except (OSError, ValueError) as error:
        print(f'Warning: cannot load vendor names from {VENDORS_PATH}: {error}; '
              'using numeric vendor codes only.', file=sys.stderr)
        return {}


def format_idcode(value):
    """Decode the captured IDCODE, including version bits masked out by BSDL."""
    version = (value >> 28) & 0xF
    device = (value >> 12) & 0xFFFF
    vendor = (value >> 1) & 0x7FF
    vendor_name = load_vendors().get(vendor, 'Unknown')
    return (f'0x{value:08X}\n'
            f'  Version: {version} (0x{version:X})\n'
            f'  Device number: 0x{device:04X}\n'
            f'  Vendor: {vendor_name} (0x{vendor:03X})')


def split_fields(text):
    fields, start, depth = [], 0, 0
    for i, char in enumerate(text):
        if char == '(':
            depth += 1
        elif char == ')':
            depth -= 1
        elif char == ',' and depth == 0:
            fields.append(text[start:i].strip())
            start = i + 1
        if depth < 0:
            raise ValueError('unbalanced BSDL parentheses')
    if depth:
        raise ValueError('unbalanced BSDL parentheses')
    fields.append(text[start:].strip())
    return fields


@dataclass(frozen=True)
class Cell:
    kind: str
    port: str
    function: str
    safe: str
    control: int = -1
    disable: str = ''
    result: str = ''


class Bsdl:
    def __init__(self, text):
        # Preserve quoted strings while removing VHDL line comments.
        text = re.sub(r'"(?:[^"\n]|"")*"|--[^\n]*',
                      lambda m: '' if m[0].startswith('--') else m[0], text)
        entities = re.findall(r'\bentity\s+(\w+)\s+is\b', text, re.I)
        if len(entities) != 1:
            raise ValueError('expected exactly one BSDL entity')
        self.name = entities[0]

        def attribute(name, optional=False):
            values = re.findall(r'\battribute\s+' + name + r'\s+of\s+' +
                                re.escape(self.name) + r'\s*:\s*entity\s+is\s+(.*?);',
                                text, re.I | re.S)
            if optional and not values:
                return None
            if len(values) != 1:
                raise ValueError(f'missing or duplicate BSDL attribute {name}')
            return values[0].strip()

        def string(name, optional=False):
            value = attribute(name, optional)
            if value is None:
                return None
            if not re.fullmatch(r'"[^"]*"(?:\s*&\s*"[^"]*")*', value, re.S):
                raise ValueError(f'{name}: only literal strings joined by & are supported')
            return ''.join(re.findall(r'"([^"]*)"', value)).upper()

        def number(name, optional=False):
            value = attribute(name, optional)
            if value is None:
                return None
            if not re.fullmatch(r'\d+', value):
                raise ValueError(f'{name}: expected literal integer')
            return int(value)

        self.ir_length = number('INSTRUCTION_LENGTH')
        boundary_length = number('BOUNDARY_LENGTH', optional=True)
        boundary_register = string('BOUNDARY_REGISTER', optional=True)
        if (boundary_length is None) != (boundary_register is None):
            raise ValueError('BOUNDARY_LENGTH and BOUNDARY_REGISTER must be provided together')
        self.boundary_length = boundary_length if boundary_length is not None else 0
        if not 1 <= self.ir_length <= 512 or (boundary_length is not None and
                                               not 1 <= boundary_length <= 32767):
            raise ValueError('unsupported IR or boundary length')
        self.opcodes = {}
        for entry in split_fields(string('INSTRUCTION_OPCODE')):
            match = re.fullmatch(r'([\w/]+)\s*\(\s*([01]+)\s*\)', entry)
            if not match or len(match[2]) != self.ir_length or match[1] in self.opcodes:
                raise ValueError(f'invalid/ambiguous instruction opcode: {entry}')
            self.opcodes[match[1]] = match[2][::-1]
        if self.opcodes.get('BYPASS') != '1' * self.ir_length:
            raise ValueError('this client requires all-ones BYPASS')
        self.registers = {}
        for entry in split_fields(string('REGISTER_ACCESS')):
            match = re.fullmatch(r'(\w+)\s*\((.*)\)', entry, re.S)
            if not match:
                raise ValueError(f'unsupported register declaration: {entry}')
            for instruction in split_fields(match[2]):
                if instruction not in self.opcodes or instruction in self.registers:
                    raise ValueError(f'invalid register access for {instruction}')
                self.registers[instruction] = match[1]
        if 'BOUNDARY' in self.registers.values() and boundary_length is None:
            raise ValueError('REGISTER_ACCESS references BOUNDARY without a boundary register')
        self.id_pattern = string('IDCODE_REGISTER', optional=True)
        if self.id_pattern is not None and not re.fullmatch('[01X]{32}', self.id_pattern):
            raise ValueError('IDCODE_REGISTER must contain exactly 32 bits (0, 1 or X)')
        self.compliance = string('COMPLIANCE_PATTERNS', optional=True)
        self.cells = {}
        for entry in split_fields(boundary_register) if boundary_register is not None else []:
            match = re.fullmatch(r'(\d+)\s*\((.*)\)', entry, re.S)
            if not match:
                raise ValueError(f'invalid boundary cell: {entry}')
            index, fields = int(match[1]), split_fields(match[2])
            if len(fields) not in (4, 7) or index in self.cells or index >= self.boundary_length:
                raise ValueError(f'invalid/duplicate boundary index {index}')
            if len(fields) == 7 and not fields[4].isdigit():
                raise ValueError('boundary control index must be an integer')
            if fields[3] not in ('0', '1', 'X'):
                raise ValueError('unsupported safe value')
            control_fields = [int(fields[4]), *fields[5:]] if len(fields) == 7 else []
            cell = Cell(*fields[:4], *control_fields)
            self.cells[index] = cell
        if set(self.cells) != set(range(self.boundary_length)):
            raise ValueError('BOUNDARY_REGISTER must describe every boundary cell once')

    @classmethod
    def load(cls, path):
        return cls(Path(path).read_text(encoding='utf-8-sig'))

    def instruction(self, name, register):
        # Some files use a single combined SAMPLE/PRELOAD name.
        if name in ('SAMPLE', 'PRELOAD') and name not in self.opcodes:
            name = 'SAMPLE/PRELOAD'
        if name not in self.opcodes:
            raise ValueError(f'{self.name} does not support {name}')
        if self.registers.get(name) != register:
            raise ValueError(f'{name} does not select {register} in this BSDL')
        return self.opcodes[name]

    def initial_vector(self):
        if not self.boundary_length:
            raise ValueError(f'{self.name} has no boundary-scan register; pin and boundary operations are unavailable')
        vector = []
        for i in range(self.boundary_length):
            c = self.cells[i]
            if (c.kind == 'BC_4' and c.function != 'INPUT') or c.kind not in ('BC_1', 'BC_4') or c.function not in (
                    'INPUT', 'OUTPUT3', 'CONTROL', 'CONTROLR', 'INTERNAL'):
                raise ValueError(f'unsupported cell semantics at {i}: {c.kind}/{c.function}')
            if c.function in ('INTERNAL', 'CONTROL', 'CONTROLR') and c.safe == 'X':
                raise ValueError(f'no defined initial value for cell {i}')
            vector.append('0' if c.safe == 'X' else c.safe)
            if c.function != 'OUTPUT3' and c.control != -1:
                raise ValueError(f'unexpected control reference at cell {i}')
        controls = {}
        for i, c in self.cells.items():
            if c.function != 'OUTPUT3':
                continue
            control = self.cells.get(c.control)
            if (control is None or control.function not in ('CONTROL', 'CONTROLR') or
                    c.disable not in ('0', '1') or c.result != 'Z'):
                raise ValueError(f'unsupported output-disable description at cell {i}')
            if c.control in controls and controls[c.control] != c.disable:
                raise ValueError('conflicting shared output-control polarity')
            controls[c.control] = c.disable
            vector[c.control] = c.disable
        return vector

    def pin_cell(self, name, function):
        matches = [(i, c) for i, c in self.cells.items() if c.port == name and c.function == function]
        if len(matches) != 1:
            raise ValueError(f'{name}: expected exactly one {function} cell')
        return matches[0]


def load_chain(bsdl, chain=None, target=None, config=None):
    aliases = {}
    if config:
        if chain is not None or target is not None:
            raise ValueError('--chain-config cannot be combined with --chain/--target')
        data = json.loads(Path(config).read_text())
        if not isinstance(data, dict) or set(data) - {'ir_lengths', 'target', 'aliases'}:
            raise ValueError('chain config expects ir_lengths, target and optional aliases')
        lengths, target = data.get('ir_lengths'), data.get('target')
        aliases = data.get('aliases', {})
    else:
        lengths = [int(v) for v in chain.split(',')] if chain else [bsdl.ir_length]
        target = 0 if target is None else target
    if (not isinstance(lengths, list) or not lengths or
            any(type(v) is not int or not 1 <= v <= 512 for v in lengths) or
            type(target) is not int or not 0 <= target < len(lengths) or
            lengths[target] != bsdl.ir_length):
        raise ValueError('invalid chain/target or target IR length does not match BSDL')
    if not isinstance(aliases, dict) or any(not isinstance(k, str) or not isinstance(v, str)
                                            for k, v in aliases.items()):
        raise ValueError('aliases must map names to BSDL port names')
    normalized = {k.upper(): v.upper() for k, v in aliases.items()}
    ports = {c.port for c in bsdl.cells.values()} - {'*'}
    if len(normalized) != len(aliases) or any(v not in ports for v in normalized.values()):
        raise ValueError('duplicate alias or alias references an unknown BSDL port')
    if any(k in ports and k != v for k, v in normalized.items()):
        raise ValueError('aliases may not redefine BSDL port names')
    return lengths, target, normalized


class BoundarySession:
    """Owns the intended output vector; captured inputs never replace it."""
    def __init__(self, client, bsdl, lengths, target, aliases=None):
        self.client, self.bsdl = client, bsdl
        self.lengths, self.target, self.aliases = lengths, target, aliases or {}
        self.invalidate()

    def invalidate(self):
        self.ready = False
        self.mode = None
        self.vector = None

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

    def boundary(self, name, vector):
        opcode = self.bsdl.instruction(name, 'BOUNDARY')
        self.check_limits(self.bsdl.boundary_length)
        # Selecting EXTEST/INTEST immediately activates preloaded outputs.
        if name in ('EXTEST', 'INTEST') and self.mode != name:
            preload = self.bsdl.instruction('PRELOAD', 'BOUNDARY')
            self.select(preload)
            self.scan(''.join(vector))
        self.select(opcode)
        captured = self.scan(''.join(vector))
        self.vector, self.mode = list(vector), name
        return captured

    def command(self, name, *args):
        if name in ('reset', 'ir', 'dr', 'clocks'):
            self.invalidate()
            return self.client.command(name, *args)
        if name in ('info', 'speed'):
            return self.client.command(name, *args)
        try:
            return self._command(name, *args)
        except (RuntimeError, OSError):
            self.invalidate()
            raise

    def _command(self, name, *args):
        if name == 'idcode':
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
        if name == 'bypass':
            opcode = self.bsdl.instruction('BYPASS', 'BYPASS')
            self.check_limits(1)
            self.select(opcode)
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


@dataclass(frozen=True)
class TapConfig:
    name: str
    ir_length: int
    bsdl: object = None
    aliases: object = None
    dap: object = None


@dataclass(frozen=True)
class ChainConfig:
    taps: tuple
    default_target: str


def _unique_object(pairs):
    result = {}
    for key, value in pairs:
        if key in result:
            raise ValueError(f'duplicate JSON field: {key}')
        result[key] = value
    return result


def load_config(path):
    """Load all TAP descriptions before connecting. Paths are relative to JSON."""
    path = Path(path)
    data = json.loads(path.read_text(encoding='utf-8-sig'), object_pairs_hook=_unique_object)
    if not isinstance(data, dict) or set(data) - {'version', 'taps', 'default_target'}:
        raise ValueError('config expects version, taps and optional default_target')
    if type(data.get('version')) is not int or data['version'] != 1:
        raise ValueError('unsupported chain configuration version (expected 1)')
    if not isinstance(data.get('taps'), list) or not data['taps']:
        raise ValueError('taps must be a nonempty array in TDI-to-TDO order')
    taps, names = [], set()
    for tap in data['taps']:
        if not isinstance(tap, dict) or set(tap) - {'name', 'bsdl', 'ir_length', 'aliases', 'protocol'}:
            raise ValueError('TAP expects name, bsdl or ir_length, and optional aliases/protocol')
        name = tap.get('name')
        if not isinstance(name, str) or not re.fullmatch(r'[A-Za-z][A-Za-z0-9_-]*', name) or name in names:
            raise ValueError('TAP names must be unique identifiers (letters, digits, _ and -)')
        names.add(name)
        profile = None
        if 'bsdl' in tap:
            if not isinstance(tap['bsdl'], str) or not tap['bsdl'].strip():
                raise ValueError(f'{name}: bsdl must be a nonempty path')
            try:
                profile = Bsdl.load(path.parent / tap['bsdl'])
            except (OSError, ValueError) as error:
                raise ValueError(f'{name}: cannot load BSDL {tap["bsdl"]}: {error}') from error
        width = tap.get('ir_length', profile.ir_length if profile else None)
        if type(width) is not int or not 1 <= width <= 512:
            raise ValueError(f'{name}: ir_length must be an integer in 1..512 or come from BSDL')
        if profile and width != profile.ir_length:
            raise ValueError(f'{name}: ir_length does not match BSDL')
        aliases = tap.get('aliases', {})
        if not isinstance(aliases, dict):
            raise ValueError(f'{name}: aliases must be an object')
        if aliases and not profile:
            raise ValueError(f'{name}: aliases require BSDL')
        # Validate aliases against this TAP only.
        if profile:
            ports = {c.port for c in profile.cells.values()} - {'*'}
            if any(not isinstance(k, str) or not k or not isinstance(v, str) for k, v in aliases.items()):
                raise ValueError(f'{name}: aliases must map names to BSDL ports')
            normalized = {k.upper(): v.upper() for k, v in aliases.items()}
            if len(normalized) != len(aliases) or any(v not in ports for v in normalized.values()):
                raise ValueError(f'{name}: duplicate alias or unknown BSDL port')
            if any(k in ports and k != v for k, v in normalized.items()):
                raise ValueError(f'{name}: aliases may not redefine BSDL port names')
            aliases = normalized
        dap = None
        if 'protocol' in tap:
            if tap['protocol'] != 'stm32f4-adiv5':
                raise ValueError(f'{name}: unsupported protocol (expected stm32f4-adiv5)')
            from jtag_dap import DapProfile
            dap = DapProfile()
            if not profile or width != dap.data['ir_length']:
                raise ValueError(f'{name}: DAP requires a matching four-bit BSDL with IDCODE')
            for instruction, register in [('IDCODE', 'DEVICE_ID'), ('BYPASS', 'BYPASS')]:
                expected = f'{dap.data["instructions"][instruction]:04b}'[::-1]
                if profile.instruction(instruction, register) != expected:
                    raise ValueError(f'{name}: BSDL instruction does not match DAP profile')
            if profile.id_pattern is None:
                raise ValueError(f'{name}: DAP requires BSDL IDCODE_REGISTER')
        taps.append(TapConfig(name, width, profile, aliases, dap))
    default = data.get('default_target', taps[0].name)
    if not isinstance(default, str) or default not in names:
        raise ValueError('default_target must name a configured TAP')
    return ChainConfig(tuple(taps), default)


class _TapTransport:
    """Coordinate effects of per-target BoundarySession IR scans on other TAPs."""
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
            for session in self.owner.sessions:
                if session is not None:
                    session.ready, session.mode = True, None
        elif name == 'ir':
            for i, session in enumerate(self.owner.sessions):
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
        self.sessions = [BoundarySession(_TapTransport(self, i), tap.bsdl,
                                         self.lengths, i, tap.aliases) if tap.bsdl else None
                         for i, tap in enumerate(config.taps)]
        from jtag_dap import ArmJtagDp
        self.daps = [ArmJtagDp(self.sessions[i], tap.dap) if tap.dap else None
                     for i, tap in enumerate(config.taps)]
        self.ready = False

    def invalidate(self):
        self.ready = False
        for dap in self.daps:
            if dap is not None:
                dap.invalidate()
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
        from jtag_dap import COMMANDS
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
            for session in self.sessions:
                if session is not None:
                    session.mode = 'BYPASS'
            return 'OK'
        session = self.sessions[self.target]
        if session is None:
            raise ValueError(f'TAP {self.names[self.target]} has no BSDL; only BYPASS and raw operations are available')
        try:
            return session.command(name, *args)
        except (RuntimeError, OSError):
            self.invalidate()
            raise
