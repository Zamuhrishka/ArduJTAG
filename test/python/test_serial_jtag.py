"""Client tests using the real pySerialTransfer encoder/parser without hardware."""
import importlib.util
from pathlib import Path
import struct
import tempfile
import json
import sys
sys.path.insert(0, str(Path(__file__).resolve().parents[2] / "tools"))
import unittest
from unittest.mock import Mock, call, patch

from click.testing import CliRunner

from pySerialTransfer import pySerialTransfer as transfer

spec = importlib.util.spec_from_file_location('serial_jtag', Path(__file__).resolve().parents[2] / 'tools/serial_jtag.py')
module = importlib.util.module_from_spec(spec)
spec.loader.exec_module(module)


class Connection:
    is_open = True

    def __init__(self):
        self.pending = bytearray()
        self.frames = []
        self.on_write = lambda data: None

    @property
    def in_waiting(self):
        return len(self.pending)

    def read(self, size=1):
        value = bytes(self.pending[:size])
        del self.pending[:size]
        return value

    def write(self, data):
        self.frames.append(bytes(data))
        self.on_write(data)
        return len(data)


def link():
    result = transfer.SerialTransfer('unused', restrict_ports=False, debug=False)
    result.connection = Connection()
    return result


class ClientTests(unittest.TestCase):
    def setUp(self):
        self.host, self.peer = link(), link()
        self.client = module.Client(self.host, timeout=0.02)
        self.requests = []
        self.status, self.corrupt, self.drop = 0, False, False

        def respond(frame):
            self.peer.connection.pending.extend(frame)
            self.assertTrue(self.peer.available())
            request = bytes(self.peer.rx_buff[:self.peer.bytes_read])
            self.requests.append(request)
            if self.drop:
                return
            command = request[1]
            payload = b''
            if command == 1:
                payload = struct.pack('<HHHHB', 512, 512, 256, 500, 0)
            elif command in (5, 6):
                # Return request TDI as captured data, including partial bytes.
                count = struct.unpack('<H', request[4:6])[0]
                payload = request[4:6] + request[-((count + 7) // 8):]
            response = request[:4] + bytes([self.status]) + (b'' if self.status else payload)
            self.peer.tx_buff[:len(response)] = response
            self.assertTrue(self.peer.send(len(response)))
            encoded = bytearray(self.peer.connection.frames[-1])
            if self.corrupt:
                encoded[-2] ^= 1
            self.host.connection.pending.extend(encoded)
        self.host.connection.on_write = respond

    def test_commands_and_byte_order(self):
        self.assertIn('IR=512', self.client.info())
        self.assertEqual('OK', self.client.command('reset'))
        self.assertEqual('OK', self.client.command('speed', '100'))
        self.assertEqual('OK', self.client.command('ir', '011111111'))
        # Contains 0x7E, the SerialTransfer delimiter, exercising byte stuffing.
        self.assertEqual('011111101', self.client.command('dr', '011111101'))
        self.assertEqual('101', self.client.command('clocks', '000', '101'))
        self.assertEqual(bytes([9, 0, 0xFE, 1]), self.requests[3][4:])
        self.assertEqual(bytes([100, 0, 0, 0]), self.requests[2][4:])
        self.assertEqual(list(range(6)), [struct.unpack('<H', r[2:4])[0] for r in self.requests])

    def test_local_validation(self):
        self.client.info()
        for command in ['dr 012', 'ir ' + '1' * 513, 'clocks 00 0', 'speed 0', 'speed 65536']:
            with self.assertRaises(ValueError):
                self.client.command(*command.split())
        self.assertEqual(1, len(self.requests))

    def test_adapter_error_is_not_retried(self):
        self.client.info()
        self.status = 5
        with self.assertRaisesRegex(ValueError, 'reset first'):
            self.client.command('dr', '0')
        self.assertTrue(self.client.usable)
        self.assertEqual(2, len(self.requests))

    def test_crc_error_invalidates_session(self):
        self.corrupt = True
        with self.assertRaisesRegex(RuntimeError, 'receive error'):
            self.client.info()
        self.assertFalse(self.client.usable)
        with self.assertRaises(RuntimeError):
            self.client.info()
        self.assertEqual(1, len(self.requests))

    def test_timeout_never_resends(self):
        self.drop = True
        with self.assertRaisesRegex(TimeoutError, 'may have executed'):
            self.client.info()
        self.assertFalse(self.client.usable)
        self.assertEqual(1, len(self.requests))


class ClickTests(unittest.TestCase):
    def use_patch(self, patcher):
        result = patcher.start()
        self.addCleanup(patcher.stop)
        return result

    def setUp(self):
        self.runner = CliRunner()
        self.link = Mock()
        self.link.open.return_value = True
        self.client = Mock()
        self.client.info.return_value = 'adapter info'
        self.client.command.return_value = 'OK'
        self.factory = self.use_patch(patch.object(transfer, 'SerialTransfer', return_value=self.link))
        self.client_factory = self.use_patch(patch.object(module, 'Client', return_value=self.client))
        self.use_patch(patch.object(module.time, 'sleep'))

    def test_help_and_invalid_options_do_not_open_port(self):
        result = self.runner.invoke(module.main, ['--help'])
        self.assertEqual(0, result.exit_code)
        self.assertIn('--timeout', result.output)
        for args in [[], ['--port', 'COM5', '--timeout', '0'],
                     ['--port', 'COM5', '--timeout', '-1'],
                     ['--port', 'COM5', '--timeout', 'nan'],
                     ['--port', 'COM5', '--timeout', 'inf']]:
            self.assertEqual(2, self.runner.invoke(module.main, args).exit_code)
        self.factory.assert_not_called()

    def test_batch_uses_one_session(self):
        result = self.runner.invoke(module.main, ['--port', 'COM5', '--timeout', '1.5',
                                                  'reset', 'ir', '011111111', 'dr', '000'])
        self.assertEqual(0, result.exit_code, result.output)
        self.factory.assert_called_once()
        self.client_factory.assert_called_once_with(self.link, 1.5)
        self.assertEqual([call('reset'), call('ir', '011111111'), call('dr', '000')],
                         self.client.command.call_args_list)
        self.link.open.assert_called_once()
        self.link.close.assert_called_once()

    def test_invalid_commands_do_not_open_port(self):
        for commands in [['reset', 'dr', '012'], ['ir'], ['unknown'],
                         ['clocks', '00', '1'], ['speed', '0']]:
            result = self.runner.invoke(module.main, ['--port', 'COM5'] + commands)
            self.assertEqual(2, result.exit_code, result.output)
        self.factory.assert_not_called()

    def test_subcommand_help_does_not_open_port(self):
        for command in ['info', 'reset', 'speed', 'ir', 'dr', 'clocks', 'shell']:
            result = self.runner.invoke(module.main, ['--port', 'COM5', command, '--help'])
            self.assertEqual(0, result.exit_code, result.output)
            self.assertIn('Usage:', result.output)
        self.factory.assert_not_called()

    def test_no_commands_shows_help_without_opening_port(self):
        result = self.runner.invoke(module.main, ['--port', 'COM5'])
        self.assertEqual(0, result.exit_code, result.output)
        self.assertIn('Usage:', result.output)
        self.factory.assert_not_called()

    def test_batch_failure_stops_without_retry(self):
        self.client.command.side_effect = TimeoutError('command may have executed')
        result = self.runner.invoke(module.main, ['--port', 'COM5', 'reset', 'dr', '000'])
        self.assertEqual(1, result.exit_code)
        self.assertIn('Error: command may have executed', result.output)
        self.client.command.assert_called_once_with('reset')
        self.link.close.assert_called_once()

    def test_shell_reuses_session_after_batch(self):
        result = self.runner.invoke(module.main, ['--port', 'COM5', 'reset', 'shell'],
                                    input='ir 011111111\ndr 000\nquit\n')
        self.assertEqual(0, result.exit_code, result.output)
        self.assertEqual([call('reset'), call('ir', '011111111'), call('dr', '000')],
                         self.client.command.call_args_list)
        self.factory.assert_called_once()
        self.link.open.assert_called_once()
        self.link.close.assert_called_once()

    def test_shell_help_and_bad_input_allow_correction(self):
        self.client.command.side_effect = [ValueError('reset required'), 'OK']
        result = self.runner.invoke(module.main, ['--port', 'COM5', 'shell'],
                                    input='\nhelp\nir --help\ndr 012\nshell\n"\ndr 0\nreset\nexit\n')
        self.assertEqual(0, result.exit_code, result.output)
        self.assertIn('Usage: ir', result.output)
        self.assertIn('reset required', result.output)
        self.assertEqual([call('dr', '0'), call('reset')], self.client.command.call_args_list)
        self.link.close.assert_called_once()

    def test_shell_eof_and_interrupt_close_port(self):
        result = self.runner.invoke(module.main, ['--port', 'COM5', 'shell'], input='')
        self.assertEqual(0, result.exit_code, result.output)
        self.link.close.assert_called_once()
        self.link.close.reset_mock()
        with patch.object(module.click, 'prompt', side_effect=KeyboardInterrupt):
            result = self.runner.invoke(module.main, ['--port', 'COM5', 'shell'])
        self.assertEqual(130, result.exit_code)
        self.link.close.assert_called_once()

    def test_shell_timeout_stops_without_retry(self):
        self.client.command.side_effect = TimeoutError('command may have executed')
        result = self.runner.invoke(module.main, ['--port', 'COM5', 'shell'],
                                    input='dr 0\nreset\n')
        self.assertEqual(1, result.exit_code)
        self.client.command.assert_called_once_with('dr', '0')
        self.link.close.assert_called_once()

    def test_shell_must_be_last_before_opening_port(self):
        result = self.runner.invoke(module.main, ['--port', 'COM5', 'shell', 'reset'])
        self.assertEqual(2, result.exit_code)
        self.factory.assert_not_called()

    def test_bsdl_commands_share_shell_and_chain_state(self):
        fixture = str(Path(__file__).parent / 'fixtures/boundary.bsdl')
        self.client.limits = (512, 512, 256, 500)
        def response(*args):
            if args[0] == 'dr':
                if len(args[1]) == 33:
                    return '0' + f'{0x16413041:032b}'[::-1]
                return '1' * len(args[1])
            return 'OK'
        self.client.command.side_effect = response
        result = self.runner.invoke(module.main,
            ['--port', 'COM5', '--bsdl', fixture, '--chain', '5,4',
             'idcode', 'pin-write', 'PD12', '1', 'shell'],
            input='pin-read PA0\npin-write PD13 1\nbypass\nquit\n')
        self.assertEqual(0, result.exit_code, result.output)
        self.assertIn('0x16413041', result.output)
        self.assertIn('(NRST) (0)', result.output)
        self.factory.assert_called_once()
        self.link.close.assert_called_once()
        calls = self.client.command.call_args_list
        self.assertEqual(call('ir', '111111111'), calls[-1])
        self.assertIn(call('dr', '000100110'), calls)
        self.assertEqual(1, calls.count(call('reset')))

    def test_bsdl_required_and_chain_mismatch_before_connecting(self):
        result = self.runner.invoke(module.main, ['--port', 'COM5', 'idcode'])
        self.assertEqual(1, result.exit_code)
        self.assertIn('require --bsdl', result.output)
        fixture = str(Path(__file__).parent / 'fixtures/boundary.bsdl')
        result = self.runner.invoke(module.main,
            ['--port', 'COM5', '--bsdl', fixture, '--chain', '4,5', 'sample'])
        self.assertEqual(1, result.exit_code)
        self.factory.assert_not_called()

    def test_named_config_targets_in_batch_and_shell(self):
        fixture = str((Path(__file__).parent / 'fixtures/boundary.bsdl').resolve())
        self.client.limits = (512, 512, 256, 500)
        self.client.command.side_effect = lambda *args: '1' * len(args[1]) if args[0] == 'dr' else 'OK'
        with tempfile.TemporaryDirectory() as directory:
            config = Path(directory) / 'chain.json'
            config.write_text(json.dumps({'version': 1, 'taps': [
                {'name': 'a', 'bsdl': fixture}, {'name': 'b', 'bsdl': fixture}]}))
            result = self.runner.invoke(module.main,
                ['--port', 'COM5', '--config', str(config), 'pin-write', 'PD12', '1', 'shell'],
                input='targets\ntarget b\npin-write PD13 1\ntarget a\nextest\nquit\n')
        self.assertEqual(0, result.exit_code, result.output)
        self.assertIn('Target: b', result.output)
        self.assertIn('Target: a', result.output)
        self.factory.assert_called_once()
        self.link.close.assert_called_once()
        self.assertEqual(1, self.client.command.call_args_list.count(call('reset')))
        self.assertEqual(2, self.client.command.call_args_list.count(call('ir', '1111101000')))

    def test_config_conflicts_and_bad_files_do_not_connect(self):
        fixture = str(Path(__file__).parent / 'fixtures/boundary.bsdl')
        with tempfile.TemporaryDirectory() as directory:
            config = Path(directory) / 'chain.json'
            config.write_text('{"version":1,"taps":[{"name":"debug","ir_length":4}]}')
            for extra in [['--bsdl', fixture], ['--chain', '4'], ['--target', '0']]:
                result = self.runner.invoke(module.main,
                    ['--port', 'COM5', '--config', str(config)] + extra + ['bypass'])
                self.assertEqual(1, result.exit_code, result.output)
            config.write_text('{"version":1,"taps":[{"name":"a","bsdl":"missing.bsd"}]}')
            result = self.runner.invoke(module.main, ['--port', 'COM5', '--config', str(config), 'shell'])
            self.assertEqual(1, result.exit_code)
        self.factory.assert_not_called()

    def test_failed_open_closes_port(self):
        self.link.open.return_value = False
        result = self.runner.invoke(module.main, ['--port', 'COM5', 'info'])
        self.assertEqual(1, result.exit_code)
        self.assertIn('cannot open serial port', result.output)
        self.client_factory.assert_not_called()
        self.link.close.assert_called_once()

    def test_keyboard_interrupt_closes_port(self):
        self.client.command.side_effect = KeyboardInterrupt
        result = self.runner.invoke(module.main, ['--port', 'COM5', 'reset'])
        self.assertEqual(130, result.exit_code)
        self.link.close.assert_called_once()


if __name__ == '__main__':
    unittest.main()
