#!/usr/bin/env python3
"""Small SerialTransfer JTAG client. Multiple commands share one serial session."""
import math
import shlex

import click
import struct
import time

VERSION = 1
STATUS = {1: 'unsupported protocol version', 2: 'unknown command',
          3: 'invalid payload length', 4: 'invalid argument',
          5: 'TAP state unknown: run reset first', 6: 'JTAG operation failed'}


def pack_bits(text, limit):
    if not text or len(text) > limit or any(c not in '01' for c in text):
        raise ValueError(f'expected 1..{limit} bits, first character shifted first')
    data = bytearray((len(text) + 7) // 8)
    for i, digit in enumerate(text):
        data[i // 8] |= int(digit) << (i % 8)
    return bytes(data)


class Client:
    def __init__(self, link, timeout=3):
        self.link, self.timeout, self.sequence = link, timeout, 0
        self.limits = None
        self.usable = True

    def request(self, command, data=b''):
        if not self.usable:
            raise RuntimeError('session unusable after communication failure; reconnect explicitly')
        sequence = self.sequence
        self.sequence = (sequence + 1) & 0xFFFF
        header = struct.pack('<BBH', VERSION, command, sequence)
        packet = header + data
        try:
            self.link.tx_buff[:len(packet)] = packet
            if not self.link.send(len(packet)):
                raise RuntimeError('serial send failed')
            deadline = time.monotonic() + self.timeout
            while time.monotonic() < deadline:
                if self.link.available():
                    response = bytes(self.link.rx_buff[:self.link.bytes_read])
                    if len(response) < 5 or response[:4] != header:
                        raise RuntimeError('unexpected response header')
                    if response[4]:
                        if len(response) != 5:
                            raise RuntimeError('invalid error response')
                        # A valid error reply completes the exchange.
                        return self._error(response[4])
                    return response[5:]
                status = getattr(self.link.status, 'value', self.link.status)
                if status <= 0:
                    raise RuntimeError(f'SerialTransfer receive error: {status}')
                time.sleep(0.001)
            raise TimeoutError('no response; command may have executed; not retried')
        except ValueError:
            raise
        except Exception:
            self.usable = False
            raise

    @staticmethod
    def _error(status):
        raise ValueError(STATUS.get(status, f'unknown adapter status {status}'))

    def info(self):
        data = self.request(1)
        if len(data) != 9:
            raise RuntimeError('invalid INFO response')
        ir, dr, clocks, speed, idle = struct.unpack('<HHHHB', data)
        self.limits = (ir, dr, clocks, speed)
        return f'protocol=1 IR={ir} DR={dr} clocks={clocks} max_kHz={speed} idle_known={bool(idle)}'

    def command(self, name, *values):
        if name == 'info' and not values:
            return self.info()
        if self.limits is None:
            self.info()
        if name == 'reset' and not values:
            command, data = 3, b''
        elif name == 'speed' and len(values) == 1:
            speed = int(values[0], 0)
            if not 1 <= speed <= self.limits[3]:
                raise ValueError(f'speed must be 1..{self.limits[3]} kHz')
            command, data = 2, struct.pack('<I', speed)
        elif name in ('ir', 'dr') and len(values) == 1:
            command = 4 if name == 'ir' else 5
            bits = values[0]
            packed = pack_bits(bits, self.limits[command - 4])
            data = struct.pack('<H', len(bits)) + packed
        elif name == 'clocks' and len(values) == 2:
            if len(values[0]) != len(values[1]):
                raise ValueError('TMS and TDI must have the same bit count')
            command, bits = 6, values[0]
            packed = b''.join(pack_bits(v, self.limits[2]) for v in values)
            data = struct.pack('<H', len(bits)) + packed
        else:
            raise ValueError('commands: info | reset | speed KHZ | ir BITS | dr BITS | clocks TMS TDI')
        reply = self.request(command, data)
        if command in (5, 6):
            count = len(bits)
            if len(reply) != 2 + (count + 7) // 8 or struct.unpack('<H', reply[:2])[0] != count:
                raise RuntimeError('invalid scan response length')
            return ''.join(str((reply[2 + i // 8] >> (i % 8)) & 1) for i in range(count))
        if reply:
            raise RuntimeError('unexpected response payload')
        return 'OK'


def positive_timeout(ctx, param, value):
    if not math.isfinite(value) or value <= 0:
        raise click.BadParameter('must be a finite positive number')
    return value


def bit_string(ctx, param, value):
    if not value or any(digit not in '01' for digit in value):
        raise click.BadParameter('expected a nonempty bit string containing only 0 and 1')
    return value


@click.group(chain=True, invoke_without_command=True, no_args_is_help=True,
             context_settings={'help_option_names': ['-h', '--help']})
@click.option('--port', required=True, help='Serial port, for example COM5 or /dev/ttyUSB0.')
@click.option('--timeout', type=float, default=3.0, show_default=True,
              callback=positive_timeout, help='Response timeout in seconds.')
@click.option('--config', type=click.Path(exists=True, dir_okay=False),
              help='JSON chain configuration with named TAPs and BSDL paths.')
@click.option('--bsdl', type=click.Path(exists=True, dir_okay=False), help='BSDL file for the target TAP.')
@click.option('--chain', help='IR lengths in physical TDI-to-TDO order, e.g. 5,4.')
@click.option('--target', type=click.IntRange(min=0), help='Zero-based target in the chain (default 0).')
@click.option('--chain-config', type=click.Path(exists=True, dir_okay=False),
              help='JSON with ir_lengths, target and optional pin aliases.')
@click.pass_context
def main(ctx, port, timeout, config, bsdl, chain, target, chain_config):
    """Control JTAG in one serial session: reset ir 011111111 dr 000.

    Put connection options before commands. Commands execute in order.
    """
    if ctx.invoked_subcommand is None:
        click.echo(ctx.get_help())


@main.command('info')
def info_command():
    """Query adapter capabilities and TAP state knowledge."""
    return ('info',)


@main.command('reset')
def reset_command():
    """Reset TAP through TMS and enter Run-Test/Idle."""
    return ('reset',)


@main.command('speed')
@click.argument('khz', type=click.IntRange(min=1))
def speed_command(khz):
    """Set the requested TCK frequency in kHz."""
    return ('speed', str(khz))


@main.command('ir')
@click.argument('bits', callback=bit_string)
def ir_command(bits):
    """Write whole-chain IR bits, leftmost bit first."""
    return ('ir', bits)


@main.command('dr')
@click.argument('bits', callback=bit_string)
def dr_command(bits):
    """Exchange whole-chain DR bits and print captured TDO."""
    return ('dr', bits)


@main.command('clocks')
@click.argument('tms', callback=bit_string)
@click.argument('tdi', callback=bit_string)
def clocks_command(tms, tdi):
    """Clock equally long TMS/TDI bit strings and print TDO."""
    if len(tms) != len(tdi):
        raise click.UsageError('TMS and TDI must have the same bit count')
    return ('clocks', tms, tdi)


# Flat commands work both with Click chain=True and inside shell.
def dap_number(ctx, param, value):
    from jtag_dap import uint
    try:
        return uint(value)
    except ValueError as error:
        raise click.BadParameter(str(error)) from error


def add_dap_register_command(space, write):
    name = f'{space}-{"write" if write else "read"}'

    def operation(**kwargs):
        args = [kwargs['register']]
        if space == 'ap':
            args.insert(0, kwargs['ap'])
        if write:
            args.append(kwargs['value'])
        return (name, *args)

    if write:
        operation = click.argument('value', callback=dap_number)(operation)
    operation = click.argument('register')(operation)
    if space == 'ap':
        operation = click.argument('ap', type=click.IntRange(0, 255))(operation)
    main.command(name, help=f'{"Write" if write else "Read"} a named {space.upper()} register; requires dap-init.')(operation)


for _space in ('dp', 'ap'):
    for _write in (False, True):
        add_dap_register_command(_space, _write)


@main.command('mem-read32')
@click.argument('address', callback=dap_number)
@click.option('--ap', type=click.IntRange(0, 255), help='AP index (default: profile setting, STM32F4 AP0).')
def mem_read_command(address, ap):
    """Read an aligned 32-bit word through AHB-AP; requires dap-init."""
    if address % 4:
        raise click.BadParameter('address must be aligned to four bytes')
    return ('mem-read32', address, ap)


@main.command('mem-write32')
@click.argument('address', callback=dap_number)
@click.argument('value', callback=dap_number)
@click.option('--ap', type=click.IntRange(0, 255), help='AP index (default: profile setting, STM32F4 AP0).')
def mem_write_command(address, value, ap):
    """Write an aligned 32-bit word through AHB-AP; requires dap-init."""
    if address % 4:
        raise click.BadParameter('address must be aligned to four bytes')
    return ('mem-write32', address, value, ap)


def add_instruction_command(name, help_text):
    @main.command(name, help=help_text)
    def instruction_command():
        return (name,)


for _name, _help in (
        ('dap-init', 'Validate Debug IDCODE, clear sticky errors and request DAP power; release NRST.'),
        ('idcode', 'Read and validate IDCODE; show hex, version, device number and vendor.'),
        ('bypass', 'Put all TAPs in BYPASS.'),
        ('sample', 'Capture the target boundary register; leave output-test mode.'),
        ('preload', 'Load the retained output vector without activating EXTEST.'),
        ('extest', 'Preload and activate the retained external-test vector.'),
        ('intest', 'Run supported internal boundary test and return captured bits.')):
    add_instruction_command(_name, _help)


@main.command('pin-read')
@click.argument('pin')
def pin_read_command(pin):
    """Read a BSDL pin or alias, preserving active EXTEST outputs."""
    return ('pin-read', pin)


@main.command('pin-write')
@click.argument('pin')
@click.argument('value', type=click.Choice(['0', '1']))
def pin_write_command(pin, value):
    """Drive one pin through PRELOAD/EXTEST, retaining other output values."""
    return ('pin-write', pin, value)


@main.command('pin-disable')
@click.argument('pin')
def pin_disable_command(pin):
    """Disable one output driver through EXTEST."""
    return ('pin-disable', pin)


@main.command('target')
@click.argument('name')
def target_command(name):
    """Select a configured TAP by name (no JTAG clocks)."""
    return ('target', name)


@main.command('targets')
def targets_command():
    """List TAP names and mark the current target."""
    return ('targets',)


@main.command('shell')
def shell_command():
    """Open an interactive session (must be the last command)."""
    return ('shell',)


def run_shell(client):
    click.echo('Commands: ' + ', '.join(name for name in main.commands if name != 'shell'))
    click.echo('Use help or COMMAND --help; quit/exit or EOF closes the session.')
    while True:
        try:
            line = click.prompt('jtag', prompt_suffix='> ', default='', show_default=False)
        except click.Abort as error:
            if isinstance(error.__context__, EOFError):
                return
            raise KeyboardInterrupt from error
        try:
            args = shlex.split(line)
            if not args:
                continue
            if args in (['quit'], ['exit']):
                return
            if args == ['help']:
                click.echo(' | '.join(name for name in main.commands if name != 'shell') + ' | quit')
                continue
            command = main.commands.get(args[0])
            if command is None or args[0] == 'shell':
                raise click.UsageError('Unknown shell command; use help')
            # Reuse Click validation and per-command help without reopening the port.
            operation = command.main(args=args[1:], prog_name=args[0], standalone_mode=False)
            if isinstance(operation, tuple):
                click.echo(client.command(*operation))
        except click.ClickException as error:
            error.show()
        except ValueError as error:
            click.echo(f'Error: {error}', err=True)
        # Communication failures escape: execution is uncertain; never retry.


@main.result_callback()
@click.pass_context
def execute_commands(ctx, commands, port, timeout, config, bsdl, chain, target, chain_config):
    # All subcommands are parsed before opening the serial port, including help.
    if not commands:
        return
    if any(command[0] == 'shell' for command in commands[:-1]):
        raise click.UsageError('shell must be the last command')
    configuration = None
    raw_commands = {'info', 'speed', 'reset', 'ir', 'dr', 'clocks', 'shell'}
    try:
        if config or bsdl:
            from jtag_bsdl import Bsdl, ChainSession, ChainConfig, TapConfig, load_config, load_chain
            if config:
                if bsdl or chain is not None or target is not None or chain_config:
                    raise ValueError('--config cannot be combined with --bsdl or legacy chain options')
                configuration = load_config(config)
            else:
                profile = Bsdl.load(bsdl)
                lengths, target_index, aliases = load_chain(profile, chain, target, chain_config)
                configuration = ChainConfig(tuple(
                    TapConfig(f'tap{i}', length, profile if i == target_index else None,
                              aliases if i == target_index else {})
                    for i, length in enumerate(lengths)), f'tap{target_index}')
        elif chain is not None or target is not None or chain_config or any(c[0] not in raw_commands for c in commands):
            raise ValueError('high-level commands and chain options require --bsdl or --config')
    except (OSError, ValueError) as error:
        raise click.ClickException(str(error)) from error
    try:
        from pySerialTransfer import pySerialTransfer
    except ImportError as error:
        raise click.ClickException(
            'Install dependencies: python -m pip install -r tools/requirements.txt') from error
    link = None
    try:
        link = pySerialTransfer.SerialTransfer(port, baud=115200, restrict_ports=False,
                                              debug=False, write_timeout=timeout)
        if not link.open():
            raise RuntimeError('cannot open serial port')
        time.sleep(2)  # Classic Nano normally resets when the serial port opens.
        link.connection.reset_input_buffer()
        client = Client(link, timeout)
        click.echo(client.info())
        if configuration is not None:
            for tap in configuration.taps:
                if tap.bsdl and tap.bsdl.compliance:
                    click.echo(f'{tap.name}: BSDL compliance must be provided externally: {tap.bsdl.compliance}', err=True)
            client = ChainSession(client, configuration)
        for command in commands:
            if command[0] == 'shell':
                run_shell(client)
            else:
                click.echo(client.command(*command))
    except (OSError, RuntimeError, ValueError) as error:
        raise click.ClickException(str(error)) from error
    except KeyboardInterrupt:
        ctx.exit(130)
    finally:
        if link is not None:
            link.close()


if __name__ == '__main__':
    main()
