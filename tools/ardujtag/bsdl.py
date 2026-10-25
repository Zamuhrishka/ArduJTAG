"""Literal BSDL subset; no VHDL evaluation."""
from dataclasses import dataclass
from pathlib import Path
import re

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
