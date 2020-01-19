# DQCsim OpenQL-mapper operator

[![PyPi](https://badgen.net/pypi/v/dqcsim-openql-mapper)](https://pypi.org/project/dqcsim-openql-mapper/)

See also: [DQCsim](https://github.com/mbrobbel/dqcsim) and
[OpenQL](https://github.com/QE-Lab/OpenQL/).

This repository contains some glue code to use the OpenQL Mapper as a DQCsim
operator.

## Compile-time mapping (OpenQL) vs runtime mapping (DQCsim + OpenQL)

It is very important to note that DQCsim is a *simulator* framework, and
operators are placed in the purely-quantum gatestream that results from
executing any and all classical and mixed quantum-classical instructions, such
as loops, if statements, and anything based on measurement results. This
results in two major differences between mapping a program with OpenQL and then
simulating versus using DQCsim to do it.

 - As a DQCsim operator, the mapper does not have access to the whole program
   before it is executed; it can only see gates up to the first measurement.
   This is because any subsequent gate may depend on the measurement result
   through the classical instructions in the frontend; trying to receive gates
   past this point may result in a deadlock. This may adversely affect the
   quality of the mapping.

 - As a DQCsim operator, the mapper maps exactly the actually executed gates.
   If you for instance have an if-else statement based on a measurement result,
   the mapper will only see the block that was actually executed based on that
   measurement. This also means that the virtual-to-physical mappings may
   change based on the stochastic measurement results from the quantum
   simulation. Furthermore, if you have a loop, the mapper will be invoked for
   each loop iteration; this may make it significantly slower, but may in fact
   improve the mapping result as the mapper doesn't need to insert swaps at
   the end of the loop body to return to the mapping at the start of the loop.
   The effect is as if all loops (even those with dynamic conditions) are
   unrolled. Ultimately, these things should improve the mapping result as more
   information is available, but may result in longer simulation times.

The computer engineers among us may note that this is exactly the difference
between compile-time scheduling (including predication, loop unrolling, etc)
and runtime scheduling (Tomasulo, speculation, etc.) in classical computer
architecture. Neither is necessarily better than the other, but the results
are different. Please keep this in mind when evaluating the mapper results.

## Installation

Installation is done through `pip`, even though this is largely not a Python
package. It's just convenient to use it regardlessly. Install as follows:

    sudo pip3 install dqcsim-openql-mapper

This allows you to use the `openql-mapper` operator in DQCsim, and exposes the
`platform2gates` command (more on this in a bit).

### Build/install from source using setup.py

This mimics a complete installation.

    git clone https://github.com/QE-Lab/dqcsim-openql-mapper.git --recursive

Note the `--recursive` there. A working version of OpenQL is included as a
subrepo; if you don't check out recursively (or init the subrepo afterwards)
compilation will fail.

    export DQCSIM_DEBUG=yes # only if you want to do a debug instead of release build.
    python3 setup.py build
    python3 setup.py bdist_wheel

You'll have to uninstall the previous version (if any) before installation will
work:

    sudo pip3 uninstall dqcsim-openql-mapper -y

Now you can install:

    sudo pip3 install target/python/dist/*

### Build from source using CMake

The `setup.py` script simply defers to CMake for the build. So you can build
with any tool based on that for building as well. Your mileage may vary with
the install target though, it is not tested.

### Running tests

A very rudimentary test is included, which you can run using

    python3 setup.py test

## Usage

This section assumes you know how DQCsim works. If you don't, start reading
[here](https://mbrobbel.github.io/dqcsim/).

To use the `openql-mapper` operator you need two things:

 - an OpenQL platform description JSON file;
 - a DQCsim <-> OpenQL gatemap JSON file.

The former describes the quantum platform you're compiling for. I'm assuming
you already have it, considering you're trying to map an algorithm to some
architecture. If you don't, well, look for the relevant OpenQL documentation.
Probably its source files.

The latter provides a mapping between DQCsim's gate format (based on
non-controlled submatrices and a number of control qubits) and OpenQL's format
(based on names). Basically, it tells the operator what for instance a gate
with the name `"prepz"` means.

### Initialization arbs

In order to tell the mapper where it should look for the JSON files, you have
to either pass it some initialization arbs or set environment variables. Here's
the list of commands you can pass:

 - `openql_mapper.hardware_config`: specifies the hardware config JSON file.
   The filename must be specified through the first binary string argument.

 - `openql_mapper.gatemap`: specifies the gatemap JSON file. The filename must
   be specified through the first binary string argument.

 - `openql_mapper.option`: sets some OpenQL option (`ql::options::set()`). The
   key is specified through the first binary string argument; the value through
   the second. This command can be specified zero or more times.

If you're working from the command line, using environment variables is easier.
The following variables are queried if the above initialization arbs are
missing:

 - `DQCSIM_OPENQL_HARDWARE_CONFIG`: default path for the hardware config file.

 - `DQCSIM_OPENQL_GATEMAP`: default path for the gatemap config file.

### Gatemap JSON files

The format of a gatemap JSON file is quite simple compared to the platform JSON
file. It's just a single object mapping from the OpenQL gate name to a DQCsim
gate description. Like this:

```json
{
    "<openql-gate-1>": "<dqcsim-gate-1",
    "<openql-gate-2>": "<dqcsim-gate-2",
    "<openql-gate-3>": "<dqcsim-gate-3"
}
```

The DQCsim gate description can be one of the following:

 - A dictionary with the entries described in the following sections;
 - A string, which is just a simplification of the above. If the string has
   "C-" prefixes, they are counted and the result is mapped to the "controlled"
   key; the remainder is interpreted as the type.

Most of the time, the DQCsim gate descriptions are also just simple strings;
you should only have to write a more complicated description for exotic gates.

To get started quickly, you can use the `platform2gates` command-line tool to
heuristically convert your platform JSON file into a gatemap file. Usually the
tool will guess correctly, but it's worth checking the result even when it
manages to generate the complete file. If it doesn't recognize a gate, it just
outputs a placeholder for you to fill in.

#### File format

As stated, the file consists of a mapping from OpenQL gate names to DQCsim gate
descriptions. Such a description is one of the following:

 - A dictionary with the entries described in the following sections;
 - A string, which is just a simplification of the above. If the string has
   "C-" prefixes, they are counted and the result is mapped to the "controlled"
   key; the remainder is interpreted as the type.

#### Types

The dictionary described above must contain at least a "type" key, mapping to
one of the following built-in strings (case-insensitive):

 - "I" - single-qubit identity gate.
 - "X" - Pauli X.
 - "Y" - Pauli Y.
 - "Z" - Pauli Z.
 - "H" - Hadamard.
 - "S" - 90-degree Z rotation.
 - "S_DAG" - -90-degree Z rotation.
 - "T" - 45-degree Z rotation.
 - "T_DAG" - -45-degree Z rotation.
 - "RX_90" - RX(90).
 - "RX_M90" - RX(-90).
 - "RX_180" - RX(180).
 - "RX" - RX gate with custom angle in radians.
 - "RY_90" - RY(90).
 - "RY_M90" - RY(-90).
 - "RY_180" - RY(180).
 - "RY" - RY gate with custom angle in radians.
 - "RZ_90" - RZ(90).
 - "RZ_M90" - RZ(-90).
 - "RZ_180" - RZ(180).
 - "RZ" - RZ gate with custom angle in radians.
 - "PHASE" - Z rotation with custom angle in radians, affecting the
   bottom-right matrix entry only.
   "matrix" or "submatrix" key.
 - "SWAP" - swap gate.
 - "SQSWAP" - square-root-of-swap gate.
 - "unitary" - custom unitary gate. Refer to the section on custom unitaries
   for more info.
 - "measure" - measurement gate. Refer to the section on measurements for more
   info.
 - "prep" - state preparation gate. Refer to the section on prep gates for more
   info.

#### Custom unitary gates

The "unitary" type allows you to specify the unitary matrix directly, using the
"matrix" key. Matrices are specified as a list of lists, where each inner list
contains two float entries representing the real and imaginary value of the
matrix entry, and the outer list represents the matrix entries in row-major
form. Integers are coerced to floats, so you can omit the decimal separator for
-1, 0, and 1. The size of the matrix (plus the number of control qubits, if any -
see next section) implies the number of qubits affected by the gate. To prevent
having to write out irrationals like 1/sqrt(2), the matrices will automatically
be normalized. After that, a unitary check is done to detect most typos.

For instance, RX(90) could be written like this:

    {
        "type": "unitary",
        "matrix": [
            [1,  0], [0, -1],
            [0, -1], [1,  0]
        ]
    }

It becomes:

                /  1  -i \
    1/sqrt(2) * |        |
                \ -i   1 /

Of course, you can just use "RX_90" for this.

Don't try to specify controlled gates by giving the full matrix, because then
DQCsim won't detect them properly. Controlled gates are specified as follows.

#### Controlled gates

To make controlled gates, the above predefined or custom matrices are
interpreted as the non-controlled submatrix, automatically extended for the
number of control qubits specified in the "controlled" key, which must be a
positive integer. If not specified, a non-controlled gate is implied. For
example,

    {
        "controlled": 1,
        "type": "X",
    }

represents a CNOT gate. You can also use the "C-" shorthand in the string
notation to do this; just using "C-X" instead of the dictionary is equivalent.

Beware the "global" phase of the submatrix; it matters due to DQCsim
synthesizing the controlled matrix by padding at the top-left side with the
identity matrix. This is why the difference between "rz" and "phase" exists.

#### Measurements

"measure" gates by default represent a measurement in the Z basis. You can
specify a different Pauli basis by supplying the "basis" key, which must then
be "X", "Y", or "Z". Alternatively, you can specify an arbitrary basis by
specifying a 2x2 matrix using "matrix". The operation then becomes equivalent
to the following:

 - apply a unitary gate to the qubit defined by the Hermitian transpose of the
   given matrix;
 - measure the qubit in the Z axis;
 - apply a unitary gate to the qubit defined by the given matrix.

#### Prep gates

"prep" gates, like measurements, default to the Z basis, accept a "basis" key
to select a different Pauli basis, or accept a 2x2 matrix, making the operation
equivalent to the following:

 - set the state of the qubit to |0>;
 - apply a unitary gate to the qubit defined by the given matrix.

#### Example file

Here's an example, generated from the QX platform file.

```json
{
    "prep_x": {
        "type": "prep",
        "basis": "x"
    },
    "prep_y": {
        "type": "prep",
        "basis": "y"
    },
    "prep_z": "prep",
    "i": "I",
    "h": "H",
    "x": "X",
    "y": "Y",
    "z": "Z",
    "x90": "RX_90",
    "y90": "RY_90",
    "x180": "RX_180",
    "y180": "RY_180",
    "mx90": "RX_M90",
    "my90": "RY_M90",
    "rx": "RZ",
    "ry": "RY",
    "rz": "RZ",
    "s": "S",
    "sdag": "S_DAG",
    "t": "T",
    "tdag": "T_DAG",
    "cnot": "C-X",
    "toffoli": "C-C-X",
    "cz": "C-Z",
    "swap": "SWAP",
    "measure": "measure",
    "measure_x": {
        "type": "measure",
        "basis": "x"
    },
    "measure_y": {
        "type": "measure",
        "basis": "y"
    },
    "measure_z": "measure"
}
```
