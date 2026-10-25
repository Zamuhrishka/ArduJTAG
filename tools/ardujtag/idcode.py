"""IDCODE formatting and optional vendor names."""
from pathlib import Path
from functools import lru_cache
import json
import re
import sys
from ._json import _unique_object

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
