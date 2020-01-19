#include <string>
#include <vector>
#include <unordered_set>
#include <fstream>
#include <json.h>
#include <dqcsim>

/**
 * Used for reporting that a gate is unknown.
 */
class UnknownGateException : public std::exception {
public:
  std::string message;

  UnknownGateException(std::string &&message) : message(message) {
  }

  const char *what() const throw () {
    return message.c_str();
  }
};

/**
 * Wrapper for OpenQL gates, used by the gate map.
 */
class OpenQLGateDescription {
public:
  std::string name;
  std::vector<size_t> qubits;
  double angle;
};

/**
 * Gate map from DQCsim gates (based on matrices) to OpenQL-like gates (based
 * on identifiers) and back, based on a json description of the mapping.
 */
class OpenQLGateMap {
private:

  /**
   * The DQCsim gatemap.
   */
  dqcsim::wrap::GateMap<std::string> map;

  /**
   * Stores which OpenQL gates use the angle argument.
   */
  std::unordered_set<std::string> has_angle;

  /**
   * Constructs the gate map.
   */
  void initialize(const nlohmann::json &json, double epsilon);

  /**
   * Adds a mapping to the DQCsim gate map.
   */
  void add_mapping(
    const std::string &openql,
    const nlohmann::json &desc,
    double epsilon);

public:

  OpenQLGateMap() = delete;

  /**
   * Constructs a gate map with the given JSON file and matrix detection
   * accuracy.
   */
  OpenQLGateMap(const nlohmann::json &json, double epsilon) {
    initialize(json, epsilon);
  }

  /**
   * Constructs a gate map with the given JSON file and matrix detection
   * accuracy.
   */
  OpenQLGateMap(const std::string &json_fname, double epsilon) {
    std::ifstream ifs(json_fname);
    auto json = nlohmann::json::parse(ifs);
    initialize(json, epsilon);
  }

  /**
   * Converts a DQCsim gate to a record from which an OpenQL gate can be
   * constructed.
   *
   * \throws UnknownGateException if the DQCsim gate was not recognized.
   */
  OpenQLGateDescription detect(const dqcsim::wrap::Gate &gate);

  /**
   * Converts an OpenQL gate description to a DQCsim gate.
   *
   * \throws UnknownGateException if the OpenQL gate was not recognized.
   */
  dqcsim::wrap::Gate construct(const OpenQLGateDescription &gate);

};
