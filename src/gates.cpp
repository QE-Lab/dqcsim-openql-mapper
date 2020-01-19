#include <algorithm>
#include <cctype>
#include <cmath>
#include <complex>
#include <gates.hpp>

// Alias the dqcsim::wrap namespace to something shorter.
namespace dqcs = dqcsim::wrap;

static inline std::string &&lowercase(std::string &&s) {
  std::transform(
    s.begin(), s.end(), s.begin(),
    [](unsigned char c) { return std::tolower(c); }
  );
  return std::move(s);
}

/**
 * Constructs the gate map.
 */
void OpenQLGateMap::initialize(const nlohmann::json &json, double epsilon) {

  // We need to add the parameterized gates to the DQCsim gatemap after adding
  // all non-parameterized gates, otherwise a parameterized gate may be
  // detected for some specialization of the gate. So we gather the records
  // from the JSON file into two vectors, one for the fixed gates and one for
  // the parameterized.
  std::vector<std::pair<std::string, nlohmann::json>> fixed;
  std::vector<std::pair<std::string, nlohmann::json>> parameterized;

  // While gathering the objects, desugar from the string notation to the
  // object notation and return an error for non-object/string records.
  for (auto it = json.begin(); it != json.end(); it++) {
    std::string openql = it.key();
    try {
      nlohmann::json desc = it.value();

      // Desugar to object.
      std::string typ;
      if (desc.is_string()) {
        typ = lowercase(desc);
        size_t controlled = 0;
        while (typ.rfind("c-", 0) == 0) {
          typ = typ.substr(2);
          controlled++;
        }
        desc = {
          {"type", typ},
          {"controlled", controlled}
        };
      } else {
        typ = lowercase(desc["type"]);
      }

      // Construct the record for later.
      auto record = std::make_pair(std::move(openql), std::move(desc));

      // Determine whether the gate is parameterized.
      if (typ == "rx" || typ == "ry" || typ == "rz") {
        parameterized.emplace_back(std::move(record));
      } else {
        fixed.emplace_back(std::move(record));
      }

    } catch (const std::exception& e) {
      throw std::runtime_error("while parsing gatemap entry for " + openql + ": " + e.what());
    }
  }

  // Add the non-parameterized gates to the DQCsim gatemap.
  for (auto const &record : fixed) {
    add_mapping(record.first, record.second, epsilon);
  }

  // Add the parameterized gates to the DQCsim gatemap.
  for (auto const &record : parameterized) {
    has_angle.insert(record.first);
    add_mapping(record.first, record.second, epsilon);
  }

}

/**
 * Adds a mapping to the DQCsim gate map.
 */
void OpenQLGateMap::add_mapping(
  const std::string &openql,
  const nlohmann::json &desc,
  double epsilon
) {
  try {

    // Load the gate type.
    std::string typ = lowercase(desc["type"]);

    // Parse the matrix/basis description.
    dqcs::Matrix matrix = dqcs::Matrix(dqcs::PauliBasis::Z);
    auto it = desc.find("matrix");
    if (it != desc.end()) {
      auto ob = it.value();

      // Parse the array into complex entries.
      if (!ob.is_array()) {
        DQCSIM_NOTE("%s", ob.dump().c_str());
        throw std::runtime_error("\"matrix\" must be an array");
      }
      std::vector<dqcs::complex> entries;
      for (auto &el : ob) {
        if (!el.is_array() || el.size() != 2) {
          throw std::runtime_error("\"matrix\" elements must be arrays of size two");
        }
        double re = el[0];
        double im = el[1];
        entries.push_back(dqcs::complex(re, im));
      }

      // Find the size of the matrix.
      size_t nq = 0;
      size_t dim = 1;
      size_t len = entries.size();
      while (len > 1) {
        if (len & 3) {
          throw std::runtime_error("\"matrix\" has invalid size");
        }
        len >>= 2;
        dim <<= 1;
        nq += 1;
      }

      // Normalize the columns of the matrix.
      for (size_t col = 0; col < dim; col++) {
        double norm = 0.0;
        for (size_t row = 0; row < dim; row++) {
          norm += std::norm(entries[row*dim + col]);
        }
        double scale = 1.0 / std::sqrt(norm);
        for (size_t row = 0; row < dim; row++) {
          entries[row*dim + col] *= scale;
        }
      }

      // Construct a DQCsim matrix for the entries.
      matrix = dqcs::Matrix(nq, entries.data());

      // Check whether the provided matrix is unitary.
      if (!matrix.approx_unitary()) {
        throw std::runtime_error("\"matrix\" is not unitary");
      }

    } else if (auto ob = desc.find("basis") != desc.end()) {
      std::string basis = lowercase(desc["basis"]);
      if (basis == "x") {
        matrix = dqcs::Matrix(dqcs::PauliBasis::X);
      } else if (basis == "y") {
        matrix = dqcs::Matrix(dqcs::PauliBasis::Y);
      } else if (basis != "z") {
        throw std::runtime_error("unknown basis " + basis);
      }
    }

    // Handle measurement and prep.
    if (typ == "measure") {
      map.with_measure(openql, matrix, epsilon);
      DQCSIM_DEBUG("Registered measurement for %s into gatemap", openql.c_str());
      return;
    }
    if (typ == "prep") {
      map.with_prep(openql, matrix, epsilon);
      DQCSIM_DEBUG("Registered prep for %s into gatemap", openql.c_str());
      return;
    }

    // Everything else is a normal unitary gate, and can thus be turned into a
    // controlled gate.
    size_t controlled = 0;
    it = desc.find("controlled");
    if (it != desc.end()) {
      controlled = it.value();
    }

    // Handle custom unitary gates.
    if (typ == "unitary") {
      map.with_unitary(openql, matrix, controlled, epsilon);
      DQCSIM_DEBUG(
        "Registered custom unitary with %d control qubit(s) for %s into gatemap",
        (int)controlled, openql.c_str());
      return;
    }

    // Handle predefined gates.
    dqcs::PredefinedGate gate;
    if (typ == "i") {
      gate = dqcs::PredefinedGate::I;
    } else if (typ == "x") {
      gate = dqcs::PredefinedGate::X;
    } else if (typ == "y") {
      gate = dqcs::PredefinedGate::Y;
    } else if (typ == "z") {
      gate = dqcs::PredefinedGate::Z;
    } else if (typ == "h") {
      gate = dqcs::PredefinedGate::H;
    } else if (typ == "s") {
      gate = dqcs::PredefinedGate::S;
    } else if (typ == "s_dag") {
      gate = dqcs::PredefinedGate::S_DAG;
    } else if (typ == "t") {
      gate = dqcs::PredefinedGate::T;
    } else if (typ == "t_dag") {
      gate = dqcs::PredefinedGate::T_DAG;
    } else if (typ == "rx_90") {
      gate = dqcs::PredefinedGate::RX_90;
    } else if (typ == "rx_m90") {
      gate = dqcs::PredefinedGate::RX_M90;
    } else if (typ == "rx_180") {
      gate = dqcs::PredefinedGate::RX_180;
    } else if (typ == "rx") {
      gate = dqcs::PredefinedGate::RX;
    } else if (typ == "ry_90") {
      gate = dqcs::PredefinedGate::RY_90;
    } else if (typ == "ry_m90") {
      gate = dqcs::PredefinedGate::RY_M90;
    } else if (typ == "ry_180") {
      gate = dqcs::PredefinedGate::RY_180;
    } else if (typ == "ry") {
      gate = dqcs::PredefinedGate::RY;
    } else if (typ == "rz_90") {
      gate = dqcs::PredefinedGate::RZ_90;
    } else if (typ == "rz_m90") {
      gate = dqcs::PredefinedGate::RZ_M90;
    } else if (typ == "rz_180") {
      gate = dqcs::PredefinedGate::RZ_180;
    } else if (typ == "rz") {
      gate = dqcs::PredefinedGate::RZ;
    } else if (typ == "phase") {
      gate = dqcs::PredefinedGate::Phase;
    } else if (typ == "swap") {
      gate = dqcs::PredefinedGate::Swap;
    } else if (typ == "sqswap") {
      gate = dqcs::PredefinedGate::SqSwap;
    } else {
      throw std::runtime_error("unknown gate type " + typ);
    }
    map.with_unitary(openql, gate, controlled, epsilon);
    DQCSIM_DEBUG(
      "Registered predefined unitary with %d control qubit(s) for %s into gatemap",
      (int)controlled, openql.c_str());

  } catch (const std::exception& e) {
    throw std::runtime_error("while parsing gatemap entry for " + openql + ": " + e.what());
  }
}

/**
 * Converts a DQCsim gate to a record from which an OpenQL gate can be
 * constructed.
 *
 * \throws UnknownGateException if the DQCsim gate was not recognized.
 */
OpenQLGateDescription OpenQLGateMap::detect(const dqcs::Gate &gate) {

  // Detect using the gate map.
  const std::string *openql;
  dqcs::QubitSet qubits = dqcs::QubitSet(0);
  dqcs::ArbData params = dqcs::ArbData(0);
  bool detected = map.detect(gate, &openql, &qubits, &params);
  if (!detected) {
    DQCSIM_DEBUG("Gate detection failed! Dump: %s", gate.dump().c_str());
    throw UnknownGateException("failed to convert an incoming gate to its OpenQL representation");
  }

  // Construct the gate description object.
  OpenQLGateDescription desc;
  desc.name = *openql;

  // Handle gates parameterized with an angle.
  if (has_angle.count(desc.name)) {
    desc.angle = params.pop_arb_arg_as<double>();
  } else {
    desc.angle = 0.0;
  }

  // Convert the qubit references.
  while (qubits.size()) {
    desc.qubits.push_back(qubits.pop().get_index());
  }

  return desc;
}

/**
 * Converts an OpenQL gate description to a DQCsim gate.
 *
 * \throws UnknownGateException if the OpenQL gate was not recognized.
 */
dqcs::Gate OpenQLGateMap::construct(const OpenQLGateDescription &desc) {

  // Construct the parameterization object.
  dqcs::ArbData params;
  if (has_angle.count(desc.name)) {
    params.push_arb_arg(desc.angle);
  }

  // Construct the qubit set.
  dqcs::QubitSet qubits;
  for (auto index : desc.qubits) {
    qubits.push(dqcs::QubitRef(index));
  }

  // Construct the gate.
  try {
    return map.construct(desc.name, std::move(qubits), std::move(params));
  } catch (const std::exception &e) {
    throw UnknownGateException("failed to convert OpenQL gate " + desc.name + ": " + e.what());
  }
}
