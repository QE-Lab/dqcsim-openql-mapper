"""DQCsim operator wrapper for the OpenQL mapper

This operator is written in C++, without exposing a Python API for it. So this
package doesn't do much!

It does provide a function to heuristically convert an OpenQL platform JSON
file to a DQCsim <-> OpenQL gate map description file.
"""

import sys
import json

def platform2gates(platform_fname, gates_fname):
    """Tries to convert an OpenQL platform JSON file to a gatemap JSON file for
    use with the DQCsim operator wrapper for OpenQL. Heuristics are applied to
    convert common gate names to DQCsim equivalents, but you may need to do
    some manual adjusting."""

    # Load platform description file.
    with open(platform_fname, 'r') as f:
        data = json.loads(f.read())

    # Find all instruction names defined in the platform description file.
    insns = []
    for name in data.get('instructions', []):
        insns.append(name.split()[0])
    for name, decomp in data.get('gate_decomposition', {}).items():
        insns.append(name.split()[0])
        for name in decomp:
            insns.append(name.split()[0])
    if not insns:
        print('No instructions found!')

    # Uniquify without losing order.
    new_insns = []
    seen = set()
    for insn in insns:
        if insn not in seen:
            seen.add(insn)
            new_insns.append(insn)
    insns = new_insns

    # Try to map based on the OpenQL names.
    unknown_gates = set()
    def to_json_line(openql):
        dqcsim = {
            'i': '"I"',
            'x': '"X"',
            'y': '"Y"',
            'z': '"Z"',
            'h': '"H"',
            's': '"S"',
            'sdag': '"S_DAG"',
            't': '"T"',
            'tdag': '"T_DAG"',
            'x90': '"RX_90"',
            'xm90': '"RX_M90"',
            'mx90': '"RX_M90"',
            'x180': '"RX_180"',
            'rx90': '"RX_90"',
            'rxm90': '"RX_M90"',
            'rx180': '"RX_180"',
            'rx': '"RX"',
            'y90': '"RY_90"',
            'ym90': '"RY_M90"',
            'my90': '"RY_M90"',
            'y180': '"RY_180"',
            'ry90': '"RY_90"',
            'rym90': '"RY_M90"',
            'ry180': '"RY_180"',
            'ry': '"RY"',
            'z90': '"RZ_90"',
            'zm90': '"RZ_M90"',
            'mz90': '"RZ_M90"',
            'z180': '"RZ_180"',
            'rz90': '"RZ_90"',
            'rzm90': '"RZ_M90"',
            'rz180': '"RZ_180"',
            'rz': '"RZ"',
            'swap': '"SWAP"',
            'move': '"SWAP"',
            'sqswap': '"SQSWAP"',
            'sqrtswap': '"SQSWAP"',
            'cx': '"C-X"',
            'ccx': '"C-C-X"',
            'cy': '"C-Y"',
            'ccy': '"C-C-Y"',
            'cz': '"C-Z"',
            'ccz': '"C-C-Z"',
            'cphase': '"C-PHASE"',
            'ccphase': '"C-C-PHASE"',
            'cnot': '"C-X"',
            'ccnot': '"C-C-X"',
            'toffoli': '"C-C-X"',
            'cswap': '"C-SWAP"',
            'fredkin': '"C-SWAP"',
            'meas': '"measure"',
            'measx': '{\n        "type": "measure",\n        "basis": "x"\n    }',
            'measy': '{\n        "type": "measure",\n        "basis": "y"\n    }',
            'measz': '"measure"',
            'prep': '"prep"',
            'prepx': '{\n        "type": "prep",\n        "basis": "x"\n    }',
            'prepy': '{\n        "type": "prep",\n        "basis": "y"\n    }',
            'prepz': '"prep"',
        }.get(
            openql
                .replace('_', '')
                .replace('-', '')
                .replace('measure', 'meas')
                .lower(),
            None)
        if dqcsim is None:
            unknown_gates.add(openql)
            dqcsim = '{\n        UNKNOWN?\n    }'
        openql = '"{}":'.format(openql)
        return '    {} {},'.format(openql, dqcsim)

    # Construct the output file.
    output = ['{']
    for insn in insns:
        output.append(to_json_line(insn))
    if output:
        output[-1] = output[-1][:-1]
    output.append('}')
    output = '\n'.join(output)

    # Write the output file.
    with open(gates_fname, 'w') as f:
        f.write(output)

    # Report result.
    if unknown_gates:
        print('The following gates were not automatically recognized:')
        print()
        for gate in sorted(unknown_gates):
            print(' - {}'.format(gate))
        print()
        print('You\'ll need to edit the output file!')
    else:
        print('All gates were heuristically recognized! Double-check the file, though.')

def platform2gates_cli():
    """Command-line entry point for the platform2gates utility."""
    if len(sys.argv) != 3:
        print('Usage: platform2gates <input:platform.json> <output:gates.json>')
        print()
        print('\n'.join(map(str.strip, platform2gates.__doc__.split('\n'))))
        print()
        print('This is part of the dqcsim-openql-mapper Python3/pip package.')
        sys.exit(1)
    platform2gates(sys.argv[1], sys.argv[2])
