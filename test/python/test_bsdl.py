from pathlib import Path
import sys
import tempfile
import unittest

sys.path.insert(0, str(Path(__file__).resolve().parents[2] / 'tools'))
from jtag_bsdl import Bsdl, BoundarySession, load_chain

FIXTURE = Path(__file__).parent / 'fixtures/boundary.bsdl'
TEXT = FIXTURE.read_text()
DEBUG_TEXT = (Path(__file__).resolve().parents[2] /
              'tools/boards/bsdl/ARM_JTAG_DP.bsd').read_text()


class Transport:
    limits = (512, 512, 256, 500)

    def __init__(self):
        self.calls = []
        self.reply = None

    def command(self, *args):
        self.calls.append(args)
        if args[0] == 'dr':
            return self.reply if self.reply is not None else '1' * len(args[1])
        return 'OK'


class BsdlTests(unittest.TestCase):
    def test_debug_idcode_and_bypass_without_boundary(self):
        model = Bsdl(DEBUG_TEXT)
        self.assertEqual(0, model.boundary_length)
        self.assertEqual({}, model.cells)
        for value in (0x4BA00477, 0x2BA00477):
            transport = Transport()
            transport.reply = f'{value:032b}'[::-1] + '0'
            session = BoundarySession(transport, model, [5, 4], 1)
            self.assertEqual(f'0x{value:08X}', session.command('idcode'))
            self.assertEqual([('reset',), ('ir', '011111111'), ('dr', '0' * 33)], transport.calls)
            session.command('bypass')
            self.assertEqual(('ir', '111111111'), transport.calls[-1])
        transport.reply = f'{0x06413041:032b}'[::-1] + '0'
        with self.assertRaisesRegex(ValueError, 'IDCODE mismatch'):
            session.command('idcode')

    def test_debug_boundary_operations_fail_without_clocks(self):
        transport = Transport()
        session = BoundarySession(transport, Bsdl(DEBUG_TEXT), [5, 4], 1)
        for operation in [('sample',), ('preload',), ('extest',), ('intest',),
                          ('pin-read', 'PA0'), ('pin-write', 'PA0', '1'), ('pin-disable', 'PA0')]:
            with self.subTest(operation=operation), self.assertRaisesRegex(ValueError, 'no boundary-scan register'):
                session.command(*operation)
        self.assertEqual([], transport.calls)

    def test_incomplete_boundary_metadata_is_rejected(self):
        declarations = [
            'attribute BOUNDARY_LENGTH of ARM_JTAG_DP : entity is 1;',
            'attribute BOUNDARY_REGISTER of ARM_JTAG_DP : entity is "0 (BC_1, *, INTERNAL, 0)";',
            'attribute BOUNDARY_LENGTH of ARM_JTAG_DP : entity is 0;'
            'attribute BOUNDARY_REGISTER of ARM_JTAG_DP : entity is "";']
        for declaration in declarations:
            with self.subTest(declaration=declaration), self.assertRaises(ValueError):
                Bsdl(DEBUG_TEXT.replace('end ARM_JTAG_DP;', declaration + '\nend ARM_JTAG_DP;'))
        with self.assertRaisesRegex(ValueError, 'references BOUNDARY'):
            Bsdl(DEBUG_TEXT.replace('DEVICE_ID (IDCODE)', 'BOUNDARY (IDCODE)'))

    def setUp(self):
        self.bsdl = Bsdl(TEXT)
        self.transport = Transport()
        self.session = BoundarySession(self.transport, self.bsdl, [5, 4], 0, {'USER': 'PA0'})

    def test_metadata_and_bit_order(self):
        self.assertEqual('10000', self.bsdl.instruction('IDCODE', 'DEVICE_ID'))
        self.assertEqual('01000', self.bsdl.instruction('SAMPLE', 'BOUNDARY'))
        self.assertEqual(list('00010000'), self.bsdl.initial_vector())
        self.assertEqual('(NRST) (0)', self.bsdl.compliance)
        # Literal comments and concatenations, including parentheses in bus ports.
        model = Bsdl(TEXT.replace('PA0', 'PA(0)'))
        self.assertEqual(0, model.pin_cell('PA(0)', 'INPUT')[0])

    def test_idcode_mask_and_chain_offsets(self):
        for lengths, target in [([5, 4], 0), ([4, 5], 1), ([2, 5, 4], 1)]:
            with self.subTest(target=target, lengths=lengths):
                t = Transport()
                session = BoundarySession(t, self.bsdl, lengths, target)
                offset = len(lengths) - target - 1
                t.reply = '0' * offset + f'{0x16413041:032b}'[::-1] + '0' * target
                self.assertEqual('0x16413041', session.command('idcode'))
                expected = ''.join('10000' if i == target else '1' * lengths[i]
                                   for i in reversed(range(len(lengths))))
                self.assertEqual(('ir', expected), t.calls[1])
                self.assertEqual(32 + len(lengths) - 1, len(t.calls[2][1]))
        self.transport.reply = '0' * 33
        with self.assertRaisesRegex(ValueError, 'IDCODE mismatch'):
            self.session.command('idcode')

    def test_write_preloads_before_extest_and_retains_outputs(self):
        self.assertEqual('OK', self.session.command('pin-write', 'PD12', '1'))
        self.assertEqual([('reset',), ('ir', '111101000'), ('dr', '000100000'),
                          ('ir', '111100000'), ('dr', '000100000')], self.transport.calls)
        self.transport.calls.clear()
        self.session.command('pin-write', 'PD13', '1')
        self.assertEqual([('ir', '111100000'), ('dr', '000100110')], self.transport.calls)
        self.assertEqual('1', self.session.vector[2])
        self.assertEqual('1', self.session.vector[5])
        self.assertEqual('1', self.session.vector[6])  # active-low disable polarity

    def test_read_in_extest_does_not_replace_vector_with_capture(self):
        self.session.command('pin-write', 'PD12', '1')
        original = list(self.session.vector)
        self.transport.calls.clear()
        self.assertEqual('1', self.session.command('pin-read', 'USER'))
        self.assertEqual(('ir', '111100000'), self.transport.calls[0])
        self.assertEqual(original, self.session.vector)
        self.session.command('pin-disable', 'PD12')
        self.assertEqual('1', self.session.vector[3])
        self.assertEqual('1', self.session.vector[2])

    def test_sample_preload_and_bypass(self):
        self.assertEqual('11111111', self.session.command('sample'))
        self.assertEqual(list('00010000'), self.session.vector)
        self.assertEqual('OK', self.session.command('preload'))
        self.session.command('extest')
        self.transport.calls.clear()
        self.session.command('bypass')
        self.assertEqual([('ir', '111111111')], self.transport.calls)
        self.session.command('pin-write', 'PD12', '0')
        self.assertEqual(('ir', '111101000'), self.transport.calls[1])

    def test_unsupported_operations_never_clock(self):
        for operation in [('intest',), ('pin-write', 'PA0', '1'), ('pin-read', 'MISSING')]:
            with self.assertRaises(ValueError):
                self.session.command(*operation)
        self.assertEqual([], self.transport.calls)
        self.transport.limits = (8, 512, 256, 500)
        with self.assertRaisesRegex(ValueError, 'limits'):
            self.session.command('extest')
        self.assertEqual([], self.transport.calls)

    def test_raw_commands_invalidate_vector_and_force_reinitialization(self):
        self.session.command('pin-write', 'PD12', '1')
        self.session.command('dr', '0')
        self.assertIsNone(self.session.vector)
        self.transport.calls.clear()
        self.session.command('sample')
        self.assertEqual(('reset',), self.transport.calls[0])
        self.assertEqual(list('00010000'), self.session.vector)

    def test_fault_during_preload_invalidates_state(self):
        def fail(*args):
            if args[0] == 'dr':
                raise ValueError('adapter failure')
            return 'OK'
        self.transport.command = fail
        with self.assertRaisesRegex(RuntimeError, 'invalidated'):
            self.session.command('extest')
        self.assertFalse(self.session.ready)
        self.assertIsNone(self.session.vector)

    def test_invalid_bsdl_is_rejected(self):
        replacements = [('INSTRUCTION_LENGTH of SYNTHETIC : entity is 5',
                         'INSTRUCTION_LENGTH of SYNTHETIC : entity is WIDTH'),
                        ('BYPASS (11111)', 'BYPASS (1111)'),
                        ('0 (BC_4, PA0', '1 (BC_4, PA0'),
                        ('"0 (BC_4, PA0, INPUT, X)"', 'CELL_MAP'),
                        ('5 (BC_1, PD13, OUTPUT3, X, 6', '5 (BC_1, PD13, OUTPUT3, X, FOO')]
        for before, after in replacements:
            with self.subTest(before=before), self.assertRaises(ValueError):
                Bsdl(TEXT.replace(before, after))

    def test_unsupported_cells_and_shared_controls_fail_closed(self):
        for text in [TEXT.replace('BC_4', 'BC_99'),
                     TEXT.replace('X, 3, 1, Z', 'X, 99, 1, Z'),
                     TEXT.replace('X, 3, 1, Z', 'X, 3, 1, PULL1')]:
            session = BoundarySession(self.transport, Bsdl(text), [5, 4], 0)
            with self.assertRaises(ValueError):
                session.command('extest')
            self.assertEqual([], self.transport.calls)
        shared = Bsdl(TEXT.replace('X, 6, 0, Z', 'X, 3, 1, Z'))
        session = BoundarySession(self.transport, shared, [5, 4], 0)
        with self.assertRaisesRegex(ValueError, 'shared'):
            session.command('pin-write', 'PD12', '1')
        self.assertEqual([], self.transport.calls)

    def test_intest_and_combined_sample_preload_alias(self):
        text = TEXT.replace('PRELOAD (00010)', 'PRELOAD (00010), INTEST (00100)').replace(
            'BOUNDARY (EXTEST, SAMPLE, PRELOAD)', 'BOUNDARY (EXTEST, SAMPLE, PRELOAD, INTEST)')
        session = BoundarySession(self.transport, Bsdl(text), [5, 4], 0)
        self.assertEqual('11111111', session.command('intest'))
        self.assertEqual(('ir', '111100100'), self.transport.calls[3])
        text = TEXT.replace('SAMPLE (00010), PRELOAD (00010)', 'SAMPLE/PRELOAD (00010)').replace(
            'EXTEST, SAMPLE, PRELOAD', 'EXTEST, SAMPLE/PRELOAD')
        self.assertEqual('01000', Bsdl(text).instruction('PRELOAD', 'BOUNDARY'))

    def test_large_boundary_vector_and_input_offset(self):
        # Synthetic 406-cell register to exercise the STM32-sized transfer.
        records = ','.join(f'{i} (BC_1, *, INTERNAL, 0)' for i in range(8, 406))
        text = TEXT.replace('entity is 8;', 'entity is 406;').replace(
            '"7 (BC_1, *, INTERNAL, 0),"', '"' + records + ',7 (BC_1, *, INTERNAL, 0),"')
        session = BoundarySession(self.transport, Bsdl(text), [5, 4], 0)
        self.assertEqual('1', session.command('pin-read', 'PA0'))
        self.assertEqual(407, len(self.transport.calls[-1][1]))
        self.assertEqual(406, len(session.vector))
        self.transport.limits = (512, 406, 256, 500)
        self.transport.calls.clear()
        with self.assertRaisesRegex(ValueError, 'limits'):
            session.command('sample')
        self.assertEqual([], self.transport.calls)

    def test_chain_config_validation_and_aliases(self):
        self.assertEqual(([5, 4], 0, {}), load_chain(self.bsdl, '5,4'))
        with self.assertRaises(ValueError):
            load_chain(self.bsdl, '4,5', 0)
        with tempfile.TemporaryDirectory() as d:
            path = Path(d) / 'board.json'
            path.write_text('{"ir_lengths":[5,4],"target":0,"aliases":{"USER":"PA0"}}')
            self.assertEqual('PA0', load_chain(self.bsdl, config=path)[2]['USER'])
            with self.assertRaises(ValueError):
                load_chain(self.bsdl, '5,4', config=path)
            path.write_text('{"ir_lengths":[5,4],"target":0,"aliases":{"USER":"UNKNOWN"}}')
            with self.assertRaises(ValueError):
                load_chain(self.bsdl, config=path)


if __name__ == '__main__':
    unittest.main()
