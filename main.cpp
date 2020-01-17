#include <string>
#include <unordered_map>
#include <dqcsim>
#include <openql.h>

using namespace dqcsim::wrap;
using namespace ql;

class QubitBiMap {
private:
  std::unordered_map<size_t, size_t> forward;
  std::unordered_map<size_t, size_t> reverse;

public:

  /**
   * Given an upstream qubit, returns the downstream qubit, if any. If there is
   * no mapping, returns -1.
   */
  ssize_t forward_lookup(size_t upstream) {
    auto iter = forward.find(upstream);
    if (iter != forward.end()) {
      return iter->second;
    }
    return -1;
  }

  /**
   * Given a downstream qubit, returns the upstream qubit, if any. If there is
   * no mapping, returns -1.
   */
  ssize_t reverse_lookup(size_t downstream) {
    auto iter = reverse.find(downstream);
    if (iter != reverse.end()) {
      return iter->second;
    }
    return -1;
  }

  /**
   * Unmaps an upstream qubit, looking up the respective downstream qubit.
   * No-op if already unmapped.
   */
  void unmap_upstream(size_t upstream) {
    auto iter = forward.find(upstream);
    if (iter != forward.end()) {
      reverse.erase(iter->second);
      forward.erase(iter);
    }
  }

  /**
   * Unmaps a downstream qubit, looking up the respective upstream qubit.
   * No-op if already unmapped.
   */
  void unmap_downstream(size_t downstream) {
    auto iter = reverse.find(downstream);
    if (iter != reverse.end()) {
      forward.erase(iter->second);
      reverse.erase(iter);
    }
  }

  /**
   * Maps the given qubits to each other. If there was already a mapping, the
   * old mapping is removed.
   */
  void map(size_t upstream, size_t downstream) {
    unmap_upstream(upstream);
    unmap_downstream(downstream);
    forward.emplace(std::make_pair(upstream, downstream));
    reverse.emplace(std::make_pair(downstream, upstream));
  }

};

class MapperPlugin {
public:

  // OpenQL platform.
  ql::quantum_platform platform;

  // Map from DQCsim qubits to OpenQL virtual qubits.
  QubitBiMap dqcs2virt;

  void initialize(PluginState &state, ArbCmdQueue &&cmds) {
    std::string platform_json;

    // Interpret the initialization commands.
    for (; cmds.size(); cmds.next()) {
      if (cmds.is_iface("openql_mapper")) {
        if (cmds.is_oper("hardware_config")) {
          if (cmds.get_arb_arg_count() != 1) {
            throw std::invalid_argument("Expected one argument for openql_mapper." + cmds.get_oper());
          } else {
            platform_json = cmds.get_arb_arg_string(0);
          }
        } else {
          throw std::invalid_argument("Unknown command openql_mapper." + cmds.get_oper());
        }
      }
    }
    if (platform_json.empty()) {
      throw std::invalid_argument("Missing openql_mapper.hardware_config cmd");
    }

    // Construct the platform.
    platform = ql::quantum_platform("dqcsim_platform", platform_json);
    state.allocate(platform.qubit_number);
    DQCSIM_INFO("OpenQL platform with %d qubits loaded", platform.qubit_number);

  }

  void drop(PluginState &state) {
  }

  void allocate(PluginState &state, QubitSet &&qubits, ArbCmdQueue &&cmds) {
    while (qubits.size()) {
      size_t dqcsim_qubit = qubits.pop().get_index();
      bool ok = false;
      for (size_t virt_qubit = 0; virt_qubit < platform.qubit_number; virt_qubit++) {
        if (dqcs2virt.reverse_lookup(virt_qubit) < 0) {
          DQCSIM_DEBUG("Placed upstream qubit %d at virtual index %d", dqcsim_qubit, virt_qubit);
          dqcs2virt.map(dqcsim_qubit, virt_qubit);
          ok = true;
          break;
        }
      }
      if (!ok) {
        throw std::runtime_error("Upstream plugin requires too many live qubits!");
      }
    }
  }

  void free(PluginState &state, QubitSet &&qubits) {
    while (qubits.size()) {
      size_t dqcsim_qubit = qubits.pop().get_index();
      dqcs2virt.unmap_upstream(dqcsim_qubit);
    }
    DQCSIM_DEBUG("banana");
  }

  MeasurementSet gate(PluginState &state, Gate &&gate) {
    return MeasurementSet();
  }

};

int main(int argc, char *argv[]) {
  MapperPlugin mapperPlugin;
  return Plugin::Operator("openql_mapper", "JvS", "v0.0")
    .with_initialize(&mapperPlugin, &MapperPlugin::initialize)
    .with_drop(&mapperPlugin, &MapperPlugin::drop)
    .with_allocate(&mapperPlugin, &MapperPlugin::allocate)
    .with_free(&mapperPlugin, &MapperPlugin::free)
    .with_gate(&mapperPlugin, &MapperPlugin::gate)
    .run(argc, argv);
}
