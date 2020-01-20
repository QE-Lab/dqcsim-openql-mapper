import unittest
import dqcsim_openql_mapper
from dqcsim.plugin import *
from dqcsim.host import *
import tempfile
import os

TEST_HARDWARE_CFG = """
{
   "eqasm_compiler" : "qx",

   "hardware_settings": {
    "qubit_number": 7,
    "cycle_time" : 20,  
    "mw_mw_buffer": 0,
    "mw_flux_buffer": 0,
    "mw_readout_buffer": 0,
    "flux_mw_buffer": 0,
    "flux_flux_buffer": 0,
    "flux_readout_buffer": 0,
    "readout_mw_buffer": 0,
    "readout_flux_buffer": 0,
    "readout_readout_buffer": 0
   },

  "resources":
   {
    "qubits":
    {
        "count": 7
    },
    "qwgs" :
    {
      "count": 3,
      "connection_map":
      {
        "0" : [0, 1],
        "1" : [2, 3, 4],
        "2" : [5, 6]
      }
    },
    "meas_units" :
    {
      "count": 2,
      "connection_map":
      {
        "0" : [0, 2, 3, 5, 6],
        "1" : [1, 4]
      }
    },
    "edges":
    {
      "count": 16,
      "connection_map":
      {
        "0": [2, 10], 
        "1": [3, 11],
        "2": [0, 8],
        "3": [1, 9],
        "4": [6, 14],
        "5": [7, 15],
        "6": [4, 12],
        "7": [5, 13],
        "8": [2, 10],
        "9": [3, 11],
        "10": [0, 8],
        "11": [1, 9],
        "12": [6, 14],
        "13": [7, 15],
        "14": [4, 12],
        "15": [5, 13]
      }
    },
    "detuned_qubits":
    {
        "description": "A two-qubit flux gate lowers the frequency of its source qubit to get near the frequency of its target qubit.  Any two qubits which have near frequencies execute a two-qubit flux gate.  To prevent any neighbor qubit of the source qubit that has the same frequency as the target qubit to interact as well, those neighbors must have their frequency detuned (lowered out of the way).  A detuned qubit cannot execute a single-qubit rotation (an instruction of 'mw' type).  An edge is a pair of qubits which can execute a two-qubit flux gate.  There are 'count' qubits. For each edge it is described, when executing a two-qubit gate for it, which set of qubits it detunes.",
        "count": 7,
        "connection_map":
        {
        "0": [3],
        "1": [2],
        "2": [4],
        "3": [3],
        "4": [],
        "5": [6],
        "6": [5],
        "7": [],
        "8": [3],
        "9": [2],
        "10": [4],
        "11": [3],
        "12": [],
        "13": [6],
        "14": [5],
        "15": []
        }
    }
  },

  "topology" : 
  {
    "x_size": 5,
    "y_size": 3,
    "qubits": 
    [ 
      { "id": 0,  "x": 1, "y": 2 },
      { "id": 1,  "x": 3, "y": 2 },
      { "id": 2,  "x": 0, "y": 1 },
      { "id": 3,  "x": 2, "y": 1 },
      { "id": 4,  "x": 4, "y": 1 },
      { "id": 5,  "x": 1, "y": 0 },
      { "id": 6,  "x": 3, "y": 0 }
    ],
    "edges": 
    [
      { "id": 0,  "src": 2, "dst": 0 },
      { "id": 1,  "src": 0, "dst": 3 },
      { "id": 2,  "src": 3, "dst": 1 },
      { "id": 3,  "src": 1, "dst": 4 },
      { "id": 4,  "src": 2, "dst": 5 },
      { "id": 5,  "src": 5, "dst": 3 },
      { "id": 6,  "src": 3, "dst": 6 },
      { "id": 7,  "src": 6, "dst": 4 },
      { "id": 8,  "src": 0, "dst": 2 },
      { "id": 9,  "src": 3, "dst": 0 },
      { "id": 10,  "src": 1, "dst": 3 },
      { "id": 11,  "src": 4, "dst": 1 },
      { "id": 12,  "src": 5, "dst": 2 },
      { "id": 13,  "src": 3, "dst": 5 },
      { "id": 14,  "src": 6, "dst": 3 },
      { "id": 15,  "src": 4, "dst": 6 }

    ]
  },

   "instructions": {

      "prepx" : {
         "duration": 40,
         "matrix" : [ [0.0,0.0], [1.0,0.0],
                      [1.0,0.0], [0.0,0.0] ],
         "disable_optimization": true
      },

      "prepy" : {
         "duration": 40,
         "matrix" : [ [0.0,0.0], [1.0,0.0],
                      [1.0,0.0], [0.0,0.0] ],
         "disable_optimization": true
      },

      "prepz" : {
         "duration": 40,
         "matrix" : [ [0.0,0.0], [1.0,0.0],
                      [1.0,0.0], [0.0,0.0] ],
         "disable_optimization": true
      },

      "i" : {
         "duration": 40,
         "matrix" : [ [1.0,0.0], [0.0,0.0],
                [0.0,0.0], [1.0,0.0] ],
         "disable_optimization": true
      },

      "h" : {
         "duration": 40,
         "matrix" : [ [0.7071068,0.0], [-0.7071068,0.0],
                [0.7071068,0.0], [ 0.7071068,0.0] ],
         "disable_optimization": true
      },

      "x" : {
         "duration": 40,
         "matrix" : [ [0.0,0.0], [1.0,0.0],
                [1.0,0.0], [0.0,0.0] ],
         "disable_optimization": false
      },

      "y" : {
         "duration": 40,
         "matrix" : [ [0.0,0.0], [1.0,0.0],
                [1.0,0.0], [0.0,0.0] ],
         "disable_optimization": false
      },

      "z" : {
         "duration": 40,
         "matrix" : [ [0.0,0.0], [1.0,0.0],
                [1.0,0.0], [0.0,0.0] ],
         "disable_optimization": false
      },

      "x90" : {
         "duration": 40,
         "matrix" : [ [0.0,0.0], [1.0,0.0],
                [1.0,0.0], [0.0,0.0] ],
         "disable_optimization": false
      },

      "y90" : {
         "duration": 20,
         "matrix" : [ [0.7071068,0.0], [-0.7071068,0.0],
                 [0.7071068,0.0], [ 0.7071068,0.0] ],
         "disable_optimization": true
      },

      "x180" : {
         "duration": 40,
         "matrix" : [ [0.0,0.0], [1.0,0.0],
                [1.0,0.0], [0.0,0.0] ],
         "disable_optimization": false
      },

      "y180" : {
         "duration": 40,
         "matrix" : [ [0.0,0.0], [1.0,0.0],
                [1.0,0.0], [0.0,0.0] ],
         "disable_optimization": false
      },

      "mx90" : {
         "duration": 40,
         "matrix" : [ [0.0,0.0], [1.0,0.0],
                [1.0,0.0], [0.0,0.0] ],
         "disable_optimization": false
      },

      "my90" : {
         "duration": 20,
         "matrix" : [ [0.7071068,0.0], [-0.7071068,0.0],
                 [0.7071068,0.0], [ 0.7071068,0.0] ],
         "disable_optimization": true
      },

      "rx" : {
         "duration": 40,
         "matrix" : [ [0.0,0.0], [1.0,0.0],
                [1.0,0.0], [0.0,0.0] ],
         "disable_optimization": false
      },

      "ry" : {
         "duration": 40,
         "matrix" : [ [0.0,0.0], [1.0,0.0],
                [1.0,0.0], [0.0,0.0] ],
         "disable_optimization": false
      },

      "rz" : {
         "duration": 40,
         "matrix" : [ [0.0,0.0], [1.0,0.0],
                [1.0,0.0], [0.0,0.0] ],
         "disable_optimization": false
      },

      "s" : {
         "duration": 40,
         "matrix" : [ [0.0,0.0], [1.0,0.0],
                [1.0,0.0], [0.0,0.0] ],
         "disable_optimization": false
      },

      "sdag" : {
         "duration": 40,
         "matrix" : [ [0.0,0.0], [1.0,0.0],
                [1.0,0.0], [0.0,0.0] ],
         "disable_optimization": false
      },

      "t" : {
         "duration": 40,
         "matrix" : [ [0.0,0.0], [1.0,0.0],
                [1.0,0.0], [0.0,0.0] ],
         "disable_optimization": false
      },

      "tdag" : {
         "duration": 40,
         "matrix" : [ [0.0,0.0], [1.0,0.0],
                [1.0,0.0], [0.0,0.0] ],
         "disable_optimization": false
      },

      "cnot" : {
         "duration": 80,
         "matrix" : [ [0.0,0.0], [1.0,0.0],
                [1.0,0.0], [0.0,0.0] ],
         "disable_optimization": true
      },

      "toffoli" : {
         "duration": 80,
         "matrix" : [ [0.0,0.0], [1.0,0.0],
                [1.0,0.0], [0.0,0.0] ],
         "disable_optimization": true
      },

      "cz" : {
         "duration": 80,
         "matrix" : [ [0.0,0.0], [1.0,0.0],
                [1.0,0.0], [0.0,0.0] ],
         "disable_optimization": true
      },

      "swap" : {
         "duration": 80,
         "matrix" : [ [0.0,0.0], [1.0,0.0],
                [1.0,0.0], [0.0,0.0] ],
         "disable_optimization": true
      },

      "move" : {
         "duration": 80,
         "matrix" : [ [0.0,0.0], [1.0,0.0],
                [1.0,0.0], [0.0,0.0] ],
         "disable_optimization": true
      },

      "measure" : {
         "duration": 300,
         "matrix" : [ [0.0,0.0], [1.0,0.0],
                [1.0,0.0], [0.0,0.0] ],
         "disable_optimization": true
      },

      "measure_x" : {
         "duration": 300,
         "matrix" : [ [0.0,0.0], [1.0,0.0],
                [1.0,0.0], [0.0,0.0] ],
         "disable_optimization": true
      },

      "measure_y" : {
         "duration": 300,
         "matrix" : [ [0.0,0.0], [1.0,0.0],
                [1.0,0.0], [0.0,0.0] ],
         "disable_optimization": true
      },

      "measure_z" : {
         "duration": 300,
         "matrix" : [ [0.0,0.0], [1.0,0.0],
                [1.0,0.0], [0.0,0.0] ],
         "disable_optimization": true
      }

   },


   "gate_decomposition": {
   }
}
"""

@plugin("Deutsch-Jozsa", "Tutorial", "0.1")
class DeutschJozsa(Frontend):

    def oracle_constant_0(self, qi, qo):
        """x -> 0 oracle function."""
        pass

    def oracle_constant_1(self, qi, qo):
        """x -> 1 oracle function."""
        self.x_gate(qo)

    def oracle_passthrough(self, qi, qo):
        """x -> x oracle function."""
        self.cnot_gate(qi, qo)

    def oracle_invert(self, qi, qo):
        """x -> !x oracle function."""
        self.cnot_gate(qi, qo)
        self.x_gate(qo)

    def deutsch_jozsa(self, qi, qo, oracle, expected):
        """Runs the Deutsch-Jozsa algorithm on the given oracle. The oracle is
        called with the input and output qubits as positional arguments."""

        # Prepare the input qubit.
        self.prepare(qi)
        self.h_gate(qi)

        # Prepare the output qubit.
        self.prepare(qo)
        self.x_gate(qo)
        self.h_gate(qo)

        # Run the oracle function.
        oracle(qi, qo)

        # Measure the input.
        self.h_gate(qi)
        self.measure(qi)
        if self.get_measurement(qi).value:
            self.info('Oracle was balanced!')
            if expected != 'balanced':
                raise ValueError('unexpected oracle result!')
        else:
            self.info('Oracle was constant!')
            if expected != 'constant':
                raise ValueError('unexpected oracle result!')

    def handle_run(self):
        qi, qo = self.allocate(2)

        self.info('Running Deutsch-Jozsa on x -> 0...')
        self.deutsch_jozsa(qi, qo, self.oracle_constant_0, 'constant')

        self.info('Running Deutsch-Jozsa on x -> 1...')
        self.deutsch_jozsa(qi, qo, self.oracle_constant_1, 'constant')

        self.info('Running Deutsch-Jozsa on x -> x...')
        self.deutsch_jozsa(qi, qo, self.oracle_passthrough, 'balanced')

        self.info('Running Deutsch-Jozsa on x -> !x...')
        self.deutsch_jozsa(qi, qo, self.oracle_invert, 'balanced')

        self.free(qi, qo)


class Constructor(unittest.TestCase):

    def test_simple(self):

        with tempfile.TemporaryDirectory() as tmpdir:

            plat_fname = tmpdir + os.sep + 'hardware_config.json'
            gate_fname = tmpdir + os.sep + 'gates.json'

            with open(plat_fname, 'w') as f:
                f.write(TEST_HARDWARE_CFG)

            dqcsim_openql_mapper.platform2gates(plat_fname, gate_fname)

            with Simulator(
                (DeutschJozsa(), {
                    'verbosity': Loglevel.INFO
                }),
                ('openql-mapper', {
                    'init': [
                        ArbCmd(
                            'openql_mapper', 'hardware_config',
                            plat_fname.encode('utf-8')
                        ),
                        ArbCmd(
                            'openql_mapper', 'gatemap',
                            gate_fname.encode('utf-8')
                        ),
                    ]
                }),
                ('qx', {
                    'verbosity': Loglevel.INFO
                }),
                stderr_verbosity=Loglevel.INFO
            ) as sim:
                sim.run()
