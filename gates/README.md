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
 - "measure" - measurement gate. Refer to the section on measurements for more
   info.
 - "prep" - state preparation gate. Refer to the section on prep gates for more
   info.

## Controlled gates

To make controlled gates, the above matrices are interpreted as the
non-controlled submatrix, automatically extended for the number of control
qubits specified in the "controlled" key, which must be a positive integer.
If not specified, a non-controlled gate is implied. For example,

    {
        "controlled": 1,
        "type": "X",
    }

represents a CNOT gate.

Beware the "global" phase of the submatrix; it matters due to DQCsim
synthesizing the controlled matrix by padding at the top-left side with the
identity matrix. This is why the difference between "rz" and "phase" exists.

Don't try to specify controlled gates by giving the full matrix, because then
DQCsim won't detect them properly.

## Measurements

"measure" gates without a "matrix" argument specify Z-axis measurements.
To specify a non-Z measurement gate, you specify a 2x2 matrix representing the
basis, giving the measurement the following semantics:

 - apply a unitary gate to the qubit defined by the Hermitian transpose of the
   given matrix;
 - measure the qubit in the Z axis;
 - apply a unitary gate to the qubit defined by the given matrix.

This allows any axis/basis to be specified.

Currently DQCsim only supports Z-axis measurements natively, so only Z-axis
measurements can be detected in the DQCsim to OpenQL direction. This may change
in the future. If/when DQCsim is updated to support this, the `gates.json` file
format shouldn't have to change.

In the OpenQL to DQCsim direction, measurement gates are always done one qubit
at a time. In the DQCsim to OpenQL direction, multi-qubit measurements are
converted to single-qubit measurements automatically.

## Prep gates

"prep" gates without a "matrix" argument specify state initialization of the
qubit to the |0> state. If a 2x2 matrix is specified, it is semantically
applied as a gate immediately after the preparation, thus allowing a qubit to
be initialized with an arbitrary state.

DQCsim currently can't natively represent prep gates. This means a prep gate
can never be detected in the DQCsim to OpenQL direction. In the opposite
direction, DQCsim will decompose the prep to the following gates:

 - measure gate in the Z basis.
 - if the measurement returned 1, apply an X gate. The qubit is now |0>.
 - if a matrix was specified, apply it as a gate.

If/when DQCsim is updated to support this, the `gates.json` file format
shouldn't have to change.

## Matrix representation

Matrices are specified as a list of lists, where each inner list contains two
float entries representing the real and imaginary value of the matrix entry,
and the outer list represents the matrix entries in row-major form. The size
of the matrix (plus the number of control qubits, if any) implies the number
of qubits affected by the gate.

To prevent having to write out irrationals like 1/sqrt(2), the matrices will
automatically be normalized. After that, a unitary check is done to detect
most typos.
