from pathlib import Path
import json
import sys
import tempfile
import unittest

sys.path.insert(0, str(Path(__file__).resolve().parents[2] / 'tools'))
from ardujtag.bsdl import Bsdl
from ardujtag.config import TapConfig
from ardujtag.config import ChainConfig
from ardujtag.chain import ChainSession
from ardujtag.config import load_config
from test_bsdl import TEXT, DEBUG_TEXT, Transport


class ChainConfigTests(unittest.TestCase):
    def test_switch_between_boundary_and_debug_idcodes(self):
        (self.root / 'debug.bsd').write_text(DEBUG_TEXT)
        self.data['taps'][1] = {'name': 'debug', 'bsdl': 'debug.bsd'}
        transport = Transport()
        session = ChainSession(transport, self.load())
        transport.reply = '0' + f'{0x06413041:032b}'[::-1]
        self.assertEqual('0x06413041', session.command('idcode').splitlines()[0])
        session.command('target', 'debug')
        transport.reply = f'{0x4BA00477:032b}'[::-1] + '0'
        self.assertEqual('0x4BA00477', session.command('idcode').splitlines()[0])
        self.assertEqual(('ir', '011111111'), transport.calls[-2])
        self.assertEqual('BYPASS', session.sessions[0].mode)
        self.assertEqual(1, transport.calls.count(('reset',)))
        calls = list(transport.calls)
        with self.assertRaisesRegex(ValueError, 'no boundary-scan register'):
            session.command('extest')
        self.assertEqual(calls, transport.calls)
        session.command('bypass')
        self.assertEqual(('ir', '111111111'), transport.calls[-1])

    def setUp(self):
        self.temp = tempfile.TemporaryDirectory()
        self.addCleanup(self.temp.cleanup)
        self.root = Path(self.temp.name)
        (self.root / 'model.bsd').write_text(TEXT)
        self.path = self.root / 'chain.json'
        self.data = {'version': 1, 'taps': [
            {'name': 'boundary', 'bsdl': 'model.bsd', 'aliases': {'LED': 'PD12'}},
            {'name': 'debug', 'ir_length': 4}], 'default_target': 'boundary'}

    def load(self, data=None):
        self.path.write_text(json.dumps(self.data if data is None else data))
        return load_config(self.path)

    def test_relative_paths_and_defaults(self):
        config = self.load()
        self.assertEqual(5, config.taps[0].ir_length)
        self.assertEqual({'LED': 'PD12'}, config.taps[0].aliases)
        self.assertIsNone(config.taps[1].bsdl)
        del self.data['default_target']
        self.assertEqual('boundary', self.load().default_target)

    def test_bad_schema_names_widths_aliases_and_targets(self):
        bad = [[], {}, {'version': True, 'taps': []},
               {'version': 1, 'taps': [], 'extra': 1}]
        for patch in [{'default_target': 'missing'}, {'version': 2}, {'extra': 1}]:
            bad.append(dict(self.data, **patch))
        for tap in [{'name': 'a'}, {'name': 'a', 'ir_length': True},
                    {'name': 'a', 'ir_length': 0}, {'name': 'a', 'ir_length': 513},
                    {'name': 'a', 'bsdl': 'model.bsd', 'ir_length': 4},
                    {'name': 'a', 'ir_length': 4, 'aliases': {'LED': 'PD12'}},
                    {'name': 'a', 'bsdl': 'missing.bsd'},
                    {'name': 'a', 'bsdl': 'model.bsd', 'aliases': {'LED': 'missing'}},
                    {'name': 'a', 'bsdl': 'model.bsd', 'unexpected': 1}]:
            bad.append({'version': 1, 'taps': [tap]})
        bad.append({'version': 1, 'taps': [{'name': 'a', 'ir_length': 4}] * 2})
        for data in bad:
            with self.subTest(data=data), self.assertRaises(ValueError):
                self.load(data)
        self.path.write_text('{"version":1,"version":1,"taps":[]}')
        with self.assertRaisesRegex(ValueError, 'duplicate JSON'):
            load_config(self.path)

    def test_missing_bsdl_tap_can_bypass_but_not_read_id(self):
        transport = Transport()
        session = ChainSession(transport, self.load())
        self.assertIn('* boundary', session.command('targets'))
        session.command('target', 'debug')
        self.assertEqual([], transport.calls)
        with self.assertRaisesRegex(ValueError, 'no BSDL'):
            session.command('idcode')
        self.assertEqual([], transport.calls)
        session.command('bypass')
        self.assertEqual([('reset',), ('ir', '1' * 9)], transport.calls)
        with self.assertRaises(ValueError):
            session.command('target', 'missing')
        self.assertEqual(1, session.target)

    def two_targets(self):
        profile = Bsdl(TEXT)
        config = ChainConfig((TapConfig('a', 5, profile, {'LED': 'PD12'}),
                              TapConfig('b', 5, profile, {'LED': 'PD13'})), 'a')
        transport = Transport()
        return transport, ChainSession(transport, config)

    def test_switch_preserves_vectors_but_forces_preload_on_return(self):
        transport, chain = self.two_targets()
        chain.command('pin-write', 'LED', '1')
        first = list(chain.sessions[0].vector)
        count = len(transport.calls)
        chain.command('target', 'b')
        self.assertEqual(count, len(transport.calls))
        self.assertEqual('EXTEST', chain.sessions[0].mode)
        chain.command('pin-write', 'LED', '1')
        self.assertEqual('BYPASS', chain.sessions[0].mode)
        self.assertEqual(first, chain.sessions[0].vector)
        self.assertEqual('1', chain.sessions[1].vector[5])
        self.assertEqual(1, transport.calls.count(('reset',)))
        transport.calls.clear()
        chain.command('target', 'a')
        chain.command('extest')
        self.assertEqual(('ir', '1111101000'), transport.calls[0])
        self.assertEqual(('dr', '0' + ''.join(first)), transport.calls[1])
        self.assertEqual(('ir', '1111100000'), transport.calls[2])
        self.assertEqual('BYPASS', chain.sessions[1].mode)
        self.assertEqual('1', chain.sessions[1].vector[5])

    def test_raw_commands_invalidate_every_target(self):
        transport, chain = self.two_targets()
        chain.command('pin-write', 'LED', '1')
        chain.command('target', 'b')
        chain.command('pin-write', 'LED', '1')
        chain.command('clocks', '0', '0')
        self.assertFalse(chain.ready)
        self.assertTrue(all(s.vector is None and s.mode is None for s in chain.sessions))
        transport.calls.clear()
        chain.command('sample')
        self.assertEqual(('reset',), transport.calls[0])

    def test_transport_failure_invalidates_whole_chain(self):
        transport, chain = self.two_targets()
        chain.command('pin-write', 'LED', '1')
        chain.command('target', 'b')
        def fail(*args):
            raise TimeoutError('lost response')
        transport.command = fail
        with self.assertRaises(RuntimeError):
            chain.command('extest')
        self.assertFalse(chain.ready)
        self.assertTrue(all(s.vector is None for s in chain.sessions))


if __name__ == '__main__':
    unittest.main()
