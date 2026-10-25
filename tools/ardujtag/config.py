"""Chain configuration and board-relative BSDL paths."""
from dataclasses import dataclass
from pathlib import Path
import json
import re
from .bsdl import Bsdl
from ._json import _unique_object


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
            from .dap.profile import DapProfile
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
