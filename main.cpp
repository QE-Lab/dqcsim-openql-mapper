#include <string>
#include <unordered_map>
#include <dqcsim>
#include <openql.h>

using namespace dqcsim::wrap;
using namespace ql;

/**
 * Represents a bidirectional map from one qubit index space to another.
 *
 * Terminology is as follows:
 *
 *     .----------.  forward  .------------.
 *     | upstream |-----------| downstream |
 *     |  space   |<----------|   space    |
 *     '----------'  reverse  '------------'
 */
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
  std::unique_ptr<ql::quantum_platform> platform;

  // OpenQL mapper.
  Mapper mapper;

  // Current OpenQL kernel.
  std::unique_ptr<ql::quantum_kernel> kernel;

  // Number of physical qubits in the platform.
  size_t num_qubits;

  // Kernel counter, for generating unique names.
  size_t kernel_counter = 0;

  // Map from DQCsim qubits to OpenQL qubits.
  QubitBiMap dqcs2virt;

  // Map from OpenQL virtual qubits to OpenQL physical qubits. We need to keep
  // track of this because the mapper entry point currently isn't stateful...
  // and in fact can't be passed an input mapping other than one-to-one, so we
  // have to use a few tricks to make it work. Basically, all gates are added
  // to the kernels with the current *physical* qubit mapping to make the
  // one-to-one "initial" mapping be correct, and after mapping this map is
  // updated to reflect the new virtual to physical map after mapping.
  QubitBiMap virt2phys;

  /**
   * Constructs a new kernel, representing a new measurement-delimited block.
   */
  void new_kernel() {
    kernel = std::make_unique<ql::quantum_kernel>(
      "kernel_" + std::to_string(kernel_counter),
      *platform, num_qubits);
    kernel_counter++;
  }

  /**
   * Initialization callback.
   *
   * We use the initialization commands to initialize OpenQL's quantum
   * platform. Specifically:
   *
   *  - openql_mapper.hardware_config: expects a single string argument
   *    specifying the location of the JSON file describing the platform.
   *  - openql_mapper.option: expects two string arguments, interpreted as key
   *    and value for `ql::options::set()`.
   *
   * TODO: it'd be nice to be able to omit the JSON filename and instead pass
   * the contents of the description file through the JSON object in the arb
   * directly.
   */
  void initialize(PluginState &state, ArbCmdQueue &&cmds) {
    std::string platform_json;

    // Interpret the initialization commands.
    for (; cmds.size(); cmds.next()) {
      if (cmds.is_iface("openql_mapper")) {
        if (cmds.is_oper("hardware_config")) {
          if (cmds.get_arb_arg_count() != 1) {
            throw std::invalid_argument("Expected one argument for openql_mapper.hardware_config");
          } else {
            platform_json = cmds.get_arb_arg_string(0);
          }
        } else if (cmds.is_oper("option")) {
          if (cmds.get_arb_arg_count() != 2) {
            throw std::invalid_argument("Expected two arguments for openql_mapper.option");
          } else {
            ql::options::set(cmds.get_arb_arg_string(0), cmds.get_arb_arg_string(1));
          }
        } else {
          throw std::invalid_argument("Unknown command openql_mapper." + cmds.get_oper());
        }
      }
    }
    if (platform_json.empty()) {
      throw std::invalid_argument("Missing openql_mapper.hardware_config cmd");
    }

    // Construct the OpenQL platform.
    platform = std::make_unique<ql::quantum_platform>("dqcsim_platform", platform_json);
    platform->print_info();
    ql::set_platform(*platform);
    num_qubits = platform->qubit_number;

    // Construct the mapper.
    mapper.Init(*platform);

    // FIXME: this initializes its own private random generator with the
    // current timestamp, but DQCsim plugins should be pure to be
    // reproducible! It should be seeded with DQCsim's random number
    // generator (`state.random()`).

    // Construct the initial kernel.
    new_kernel();

    // Allocate the physical qubits downstream.
    state.allocate(num_qubits);
    DQCSIM_INFO("OpenQL platform with %d qubits loaded", num_qubits);

    // Initialize the virt2phys map.
    for (size_t qubit = 0; qubit < num_qubits; qubit++) {
      virt2phys.map(qubit, qubit);
    }

  }

  /**
   * Qubit allocation callback.
   *
   * DQCsim supports allocating and freeing qubits at will, but obviously a
   * physical platform doesn't. The trivial solution would be to just error
   * out on the N+1'th qubit allocation, but we can do better than that when
   * there are deallocations as well by reusing qubits that were freed. That's
   * what the dqcs2virt bimap is used for; mapping the upstream DQCsim qubit
   * references to virtual qubits in OpenQL. When qubits aren't freed until
   * the end of the program (or are never freed), the OpenQL virtual qubit
   * index will just be the DQCsim index, minus one because DQCsim starts
   * counting at one.
   */
  void allocate(PluginState &state, QubitSet &&qubits, ArbCmdQueue &&cmds) {

    // We don't use or forward any additional qubit parameters at this time.
    if (cmds.size()) {
      static bool warned = false;
      if (!warned) {
        DQCSIM_WARN(
          "Found data attached to qubit allocation. "
          "This operator discards such data!");
        warned = true;
      }
    }

    // Loop over the qubits that are to be allocated.
    while (qubits.size()) {

      // A new DQCsim upstream qubit index to allocate.
      size_t dqcsim_qubit = qubits.pop().get_index();

      // Look for the first free OpenQL virtual qubit index.
      bool found = false;
      for (size_t virt_qubit = 0; virt_qubit < num_qubits; virt_qubit++) {
        if (dqcs2virt.reverse_lookup(virt_qubit) < 0) {
          DQCSIM_DEBUG("Placed upstream qubit %d at virtual index %d", dqcsim_qubit, virt_qubit);
          dqcs2virt.map(dqcsim_qubit, virt_qubit);
          found = true;
          break;
        }
      }

      // Error out if we can't find a free qubit. This means that too many
      // qubits are currently live.
      if (!found) {
        throw std::runtime_error("Upstream plugin requires too many live qubits!");
      }

    }

  }

  /**
   * Qubit deallocation callback.
   *
   * Inverse of `allocate()`.
   */
  void free(PluginState &state, QubitSet &&qubits) {

    // Loop over the qubits that are to be freed.
    while (qubits.size()) {

      // The DQCsim upstream qubit index to free.
      size_t dqcsim_qubit = qubits.pop().get_index();

      // Unmap it in the bimap to do the free.
      DQCSIM_DEBUG("Freed upstream qubit %d", dqcsim_qubit);
      dqcs2virt.unmap_upstream(dqcsim_qubit);

    }

  }

  /**
   * This function runs the mapper for the gates queued up thus far, sends the
   * mapped gates downstream, and returns the measurement result of the last
   * gate if it's a measurement gate.
   */
  MeasurementSet run_mapper() {

    // If the current kernel is empty, we don't have to do anything.
    if (kernel->c.empty()) {
      return MeasurementSet();
    }

    // If this is the first kernel being mapped, assume that the initial
    // virtual to physical mapping doesn't matter, so we can do an initial map.
    // If this isn't the first, assume the mapping is one-to-one; we've been
    // building the kernel with physical qubit indices to make this valid.
    if (kernel_counter == 0) {
      ql::options::set("mapinitone2one", "no");
      // It's up to the user whether we do initial placement here. The default
      // is currently defined to no in OpenQL.
    } else {
      ql::options::set("mapinitone2one", "yes");
      ql::options::set("initialplace", "no");
    }

    // Don't insert prep gates automatically; let the upstream plugin handle
    // that. DQCsim currently doesn't really support prep gates anyway (they're
    // implemented as a measurement followed by a conditional X).
    ql::options::set("mapassumezeroinitstate", "yes");

    // Run the mapper on the kernel.
    mapper.Map(*kernel);

    // Update our copy of the virtual to physical map based on the mapping
    // result.
    QubitBiMap new_virt2phys;
    for (size_t old_phys = 0; old_phys < num_qubits; old_phys++) {
      size_t new_phys = mapper.v2r_out[old_phys];
      if (new_phys != UNDEFINED_QUBIT) {
        ssize_t virt = virt2phys.reverse_lookup(old_phys);
        if (virt >= 0) {
          new_virt2phys.map(virt, new_phys);
        }
      }
    }
    virt2phys = new_virt2phys;

    // Send the gates downstream.
    // TODO

    // Construct a new kernel for the next batch.
    new_kernel();

    // Return the result of the latest if it was a measurement.
    // TODO
    return MeasurementSet();

  }

  /**
   * Gate callback.
   *
   * Measurement gates must be forwarded immediately, but we can queue
   * everything else up in the circuit.
   */
  MeasurementSet gate(PluginState &state, Gate &&gate) {

    // Add the gate to the current kernel.
    // NOTE: we need to use the current *physical* qubit index to construct the
    // gates, because the mapper maps the circuits without maintaining state
    // (this isn't implemented yet apparently). Instead, we have it assume that
    // the initial state is one-to-one, making physical indices the right ones
    // here.
    // TODO

    // If the gate was a measurement gate, run the mapper now. If we try to
    // queue up the measurement, we might get a deadlock, because the frontend
    // may end up needing the measurement result to determine what the next
    // gate will be.
    if (gate.has_measures()) {
      return run_mapper();
    } else {
      return MeasurementSet();
    }

  }

  /**
   * Drop callback.
   *
   * We use this to flush out any pending operations occurring after the last
   * measurement.
   */
  void drop(PluginState &state) {
    run_mapper();
  }

  /**
   * Callback used for advancing simulation time.
   *
   * We currently ignore this. Scheduling logically happens after mapping, so
   * there isn't much we can do with this information at this stage.
   */
  void advance(PluginState &state, Cycle cycles) {
    static bool warned = false;
    if (!warned) {
      DQCSIM_WARN(
        "Received request to advance time. This information is discarded, "
        "as scheduling normally happens after mapping!");
      warned = true;
    }
  }

};

int main(int argc, char *argv[]) {
  MapperPlugin mapperPlugin;
  return Plugin::Operator("openql_mapper", "JvS", "v0.0")
    .with_initialize(&mapperPlugin, &MapperPlugin::initialize)
    .with_allocate(&mapperPlugin, &MapperPlugin::allocate)
    .with_free(&mapperPlugin, &MapperPlugin::free)
    .with_gate(&mapperPlugin, &MapperPlugin::gate)
    .with_advance(&mapperPlugin, &MapperPlugin::advance)
    .with_drop(&mapperPlugin, &MapperPlugin::drop)
    .run(argc, argv);
}
