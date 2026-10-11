"""Behavioral JTAG-DP model: independent fixed register map and delayed replies."""
import json
from pathlib import Path
import sys
import tempfile
import unittest
from unittest.mock import patch

sys.path.insert(0, str(Path(__file__).resolve().parents[2] / 'tools'))
from jtag_bsdl import Bsdl, BoundarySession, ChainSession, ChainConfig, TapConfig, load_config
from jtag_dap import DapProfile, ArmJtagDp, PROFILE_PATH
from test_bsdl import DEBUG_TEXT


class DapTransport:
    limits = (512, 512, 256, 500)

    def __init__(self):
        self.calls, self.accepted = [], []
        self.ir, self.pending, self.select = 15, 0, 0
        self.ctrl, self.sticky = 0, 0
        self.power = True
        self.waits, self.bad_ack = 0, None
        self.fault, self.io_error = False, False
        self.idr = 0x24770011
        self.apregs, self.memory = {}, {0x20000000: 0x12345678}

    def command(self, name, *args):
        self.calls.append((name, *args))
        if self.io_error:
            raise OSError('disconnected')
        if name == 'reset':
            self.ir, self.pending = 14, 0
        if name == 'ir':
            assert len(args[0]) == 9 and args[0][4:] == '11111'
            self.ir = int(args[0][:4][::-1], 2)
        if name != 'dr':
            return 'OK'
        bits = args[0]
        if self.ir == 14:
            return f'{0x4BA00477:032b}'[::-1] + '0'
        assert len(bits) == 36 and bits[-1] == '0'
        if self.bad_ack is not None:
            return f'{self.bad_ack:035b}'[::-1] + '0'
        if self.waits:
            self.waits -= 1
            return '100' + '0' * 33
        response = ((self.pending << 3) | 2)
        request = int(bits[:35][::-1], 2)
        read, address, value = bool(request & 1), ((request >> 1) & 3) * 4, request >> 3
        self.accepted.append((self.ir, read, address, value))
        if self.ir == 10:
            if address == 8:
                if not read:
                    self.select = value
                self.pending = self.select
            elif address == 4:
                if not read:
                    self.sticky &= ~(value & 0x32)
                    self.ctrl = value & 0x50000000
                self.pending = self.ctrl | (0xA0000000 if self.power else 0) | self.sticky
            else:
                self.pending = 0
        else:
            assert self.ir == 11
            ap, address = self.select >> 24, (self.select & 0xF0) | address
            if self.fault:
                self.sticky |= 0x20
                self.pending = 0
            elif address == 0xFC:
                self.pending = self.idr
            elif address == 0x0C:
                addr = self.apregs.get((ap, 4), 0)
                if not read:
                    self.memory[addr] = value
                self.pending = self.memory.get(addr, 0)
            else:
                if not read:
                    self.apregs[ap, address] = value
                self.pending = self.apregs.get((ap, address), 0)
        return f'{response:035b}'[::-1] + '0'


class DapTests(unittest.TestCase):
    def setUp(self):
        self.t = DapTransport()
        self.profile = DapProfile()
        config = ChainConfig((TapConfig('boundary', 5),
                              TapConfig('debug', 4, Bsdl(DEBUG_TEXT), {}, self.profile)), 'debug')
        self.chain = ChainSession(self.t, config)
        self.dap = self.chain.daps[1]
        self.dap.attempts = 3
        sleeper = patch('jtag_dap.time.sleep')
        sleeper.start()
        self.addCleanup(sleeper.stop)

    def init(self):
        self.assertEqual('DAP ready', self.chain.command('dap-init'))

    def test_initialize_and_banked_ap_reads(self):
        self.t.sticky = 0x32
        self.init()
        self.assertEqual('0xF0000000', self.chain.command('dp-read', 'CTRL_STAT'))
        self.assertEqual('0x24770011', self.chain.command('ap-read', 3, 'IDR'))
        self.assertIn((10, False, 8, 0x030000F0), self.t.accepted)
        self.assertEqual(0, self.t.sticky)

    def test_memory_roundtrip_and_restore(self):
        self.init()
        original = self.chain.command('mem-read32', 0x20000000, 0)
        self.assertEqual('0x12345678', original)
        self.chain.command('mem-write32', 0x20000000, 0xA5A55A5A, 0)
        self.assertEqual('0xA5A55A5A', self.chain.command('mem-read32', 0x20000000, 0))
        self.chain.command('mem-write32', 0x20000000, int(original, 16), 0)
        self.assertEqual(0x12345678, self.t.memory[0x20000000])
        self.assertEqual(0x23000002, self.t.apregs[0, 0])

    def test_wait_repeats_only_unaccepted_request(self):
        self.init()
        self.t.accepted.clear()
        original = self.t.command
        injected = []
        def delayed(name, *args):
            # Stall the drain after accepting the AP write. Replaying the write
            # here would duplicate side effects on a memory-mapped peripheral.
            if name == 'dr' and self.t.ir == 10 and self.t.accepted and self.t.accepted[-1][0] == 11 and not injected:
                self.t.waits = 2
                injected.append(True)
            return original(name, *args)
        with patch.object(self.t, 'command', side_effect=delayed):
            self.chain.command('ap-write', 0, 'TAR', 0x20000000)
        self.assertTrue(injected)
        self.assertEqual(1, self.t.accepted.count((11, False, 4, 0x20000000)))

    def test_fault_wait_timeout_and_bad_ack_invalidate(self):
        for attr, value, message in [('fault', True, 'sticky error'), ('waits', 99, 'WAIT timeout'),
                                      ('bad_ack', 7, 'invalid JTAG-DP ACK'), ('io_error', True, 'disconnected')]:
            with self.subTest(attr=attr):
                setattr(self.t, attr, False if attr in ('fault', 'io_error') else None if attr == 'bad_ack' else 0)
                self.init()
                setattr(self.t, attr, value)
                with self.assertRaisesRegex(RuntimeError, message):
                    self.chain.command('ap-read', 0, 'IDR')
                self.assertFalse(self.dap.initialized)
                self.assertFalse(self.chain.ready)
                setattr(self.t, attr, False if attr in ('fault', 'io_error') else None if attr == 'bad_ack' else 0)

    def test_power_timeout_and_wrong_ap(self):
        self.t.power = False
        with self.assertRaisesRegex(RuntimeError, 'power-up timeout'):
            self.chain.command('dap-init')
        self.t.power = True
        self.init()
        self.t.idr = 0
        with self.assertRaisesRegex(RuntimeError, 'not the supported'):
            self.chain.command('mem-write32', 0x20000000, 1, 0)
        self.assertEqual(0x12345678, self.t.memory[0x20000000])
        self.t.idr = 0x24770011
        self.init()
        access = self.dap.ap
        def reject_size(index, name, read=True, value=0):
            result = access(index, name, read, value)
            return result & ~7 if name == 'CSW' and read else result
        with patch.object(self.dap, 'ap', side_effect=reject_size):
            with self.assertRaisesRegex(RuntimeError, 'CSW configuration'):
                self.chain.command('mem-write32', 0x20000000, 1, 0)
        self.assertEqual(0x12345678, self.t.memory[0x20000000])

    def test_invalid_operations_do_not_clock(self):
        for operation in [('dp-read', 'CTRL_STAT'), ('ap-read', 256, 'IDR'),
                          ('ap-write', 0, 'IDR', 1), ('dp-read', 'MISSING'),
                          ('mem-read32', 1, 0), ('mem-write32', 0, 1 << 32, 0)]:
            with self.subTest(operation=operation), self.assertRaises(ValueError):
                self.chain.command(*operation)
        self.assertEqual([], self.t.calls)
        self.t.limits = (512, 35, 256, 500)
        with self.assertRaisesRegex(ValueError, 'limits'):
            self.chain.command('dap-init')
        self.assertEqual([], self.t.calls)

    def test_raw_and_boundary_scans_invalidate_dap(self):
        for operation in [('reset',), ('ir', '111111111'), ('dr', '0'), ('clocks', '0', '0'), ('bypass',), ('idcode',)]:
            self.init()
            # Raw DR in BYPASS has no DAP semantics; mock it for this state test.
            with patch.object(self.t, 'command', return_value='0' * 33):
                if operation[0] == 'idcode':
                    with self.assertRaises(ValueError):
                        self.chain.command(*operation)
                else:
                    self.chain.command(*operation)
            self.assertFalse(self.dap.initialized)

    def test_profile_validation_and_config(self):
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)
            (root / 'debug.bsd').write_text(DEBUG_TEXT)
            path = root / 'chain.json'
            data = {'version': 1, 'taps': [{'name': 'debug', 'bsdl': 'debug.bsd', 'protocol': 'stm32f4-adiv5'}]}
            path.write_text(json.dumps(data))
            self.assertIsNotNone(load_config(path).taps[0].dap)
            data['taps'][0]['protocol'] = 'unknown'
            path.write_text(json.dumps(data))
            with self.assertRaises(ValueError):
                load_config(path)
            data['taps'][0] = {'name': 'debug', 'ir_length': 4, 'protocol': 'stm32f4-adiv5'}
            path.write_text(json.dumps(data))
            with self.assertRaisesRegex(ValueError, 'requires a matching'):
                load_config(path)
            profile = json.loads(PROFILE_PATH.read_text())
            profile['ap']['CSW']['fields']['SIZE']['width'] = 33
            path.write_text(json.dumps(profile))
            with self.assertRaisesRegex(ValueError, 'invalid DAP profile'):
                DapProfile(path)


if __name__ == '__main__':
    unittest.main()
