# `gates.json` file description

The gates.json file required by the map operator defines the mapping between
DQCsim's gate format (based on non-controlled submatrices and a number of
control qubits) and OpenQL's format (based on names). You need it in addition
to an OpenQL platform JSON file.

To prevent a needless amount of typing, a Python script is provided to generate
the mapping file based on a platform JSON file using heuristics:
`platform2gates.py`. You only need to check and maybe update the file
afterwards. Furthermore, `qx-gates.json` is provided as an example.

The file consists of a mapping from OpenQL gate names to DQCsim gate
descriptions, where such a description is one of the following:

 - A dictionary with the entries described in the following sections;
 - A string, which is just a simplification of the above. If the string has
   "C-" prefixes, they are counted and the result is mapped to the "controlled"
   key; the remainder is interpreted as the type.

## Types

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

## Custom unitary gates

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

## Controlled gates

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

## Measurements

"measure" gates by default represent a measurement in the Z basis. You can
specify a different Pauli basis by supplying the "basis" key, which must then
be "X", "Y", or "Z". Alternatively, you can specify an arbitrary basis by
specifying a 2x2 matrix using "matrix". The operation then becomes equivalent
to the following:

 - apply a unitary gate to the qubit defined by the Hermitian transpose of the
   given matrix;
 - measure the qubit in the Z axis;
 - apply a unitary gate to the qubit defined by the given matrix.

## Prep gates

"prep" gates, like measurements, default to the Z basis, accept a "basis" key
to select a different Pauli basis, or accept a 2x2 matrix, making the operation
equivalent to the following:

 - set the state of the qubit to |0>;
 - apply a unitary gate to the qubit defined by the given matrix.
