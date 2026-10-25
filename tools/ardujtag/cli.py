"""Click commands and the optional interactive shell."""
import math
import shlex
import time
import click
from .transport import Client

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
    from .dap.profile import uint
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
            from .bsdl import Bsdl
            from .chain import ChainSession
            from .config import ChainConfig, TapConfig, load_config, load_chain
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
