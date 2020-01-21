#include <cstdlib>
#include <string>
#include <dqcsim>
#include <openql.h>
#include "bimap.hpp"
#include "gates.hpp"

// Alias the dqcsim::wrap namespace to something shorter.
namespace dqcs = dqcsim::wrap;

/**
 * Operator plugin for the mapper.
 */
class MapperPlugin {
public:

  // OpenQL platform.
  std::shared_ptr<ql::quantum_platform> platform;

  // OpenQL mapper.
  Mapper mapper;

  // Current OpenQL kernel.
  std::shared_ptr<ql::quantum_kernel> kernel;

  // Number of physical qubits in the platform.
  size_t num_qubits;

  // Kernel counter, for generating unique names.
  size_t kernel_counter = 0;

  // Map from DQCsim gates to OpenQL gate descriptions and back.
  std::shared_ptr<OpenQLGateMap> gatemap;

  // Map from DQCsim qubits to OpenQL qubits.
  QubitBiMap dqcs2virt;

  // Number of upstream qubits allocated so far.
  size_t dqcs_nq = 0;

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
    kernel = std::make_shared<ql::quantum_kernel>(
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
   * TODO: it'd be nice to be able to omit the JSON filenames and instead pass
   * the contents of the files through the JSON object in the arb directly.
   */
  void initialize(
    dqcs::PluginState &state,
    dqcs::ArbCmdQueue &&cmds
  ) {
    std::string platform_json_fname;
    std::string gatemap_json_fname;

    // Get the default values for the gate and platform JSON filenames from the
    // environment.
    const char *s;
    s = std::getenv("DQCSIM_OPENQL_HARDWARE_CONFIG");
    if (s != nullptr) platform_json_fname = std::string(s);
    s = std::getenv("DQCSIM_OPENQL_GATEMAP");
    if (s != nullptr) gatemap_json_fname = std::string(s);

    // Interpret the initialization commands.
    for (; cmds.size(); cmds.next()) {
      if (cmds.is_iface("openql_mapper")) {
        if (cmds.is_oper("hardware_config")) {
          if (cmds.get_arb_arg_count() != 1) {
            throw std::invalid_argument("Expected one argument for openql_mapper.hardware_config");
          } else {
            platform_json_fname = cmds.get_arb_arg_string(0);
          }
        } else if (cmds.is_oper("gatemap")) {
          if (cmds.get_arb_arg_count() != 1) {
            throw std::invalid_argument("Expected one argument for openql_mapper.gatemap");
          } else {
            gatemap_json_fname = cmds.get_arb_arg_string(0);
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

    // Check that we have a platform and gatemap description.
    if (platform_json_fname.empty()) {
      throw std::invalid_argument(
        "Missing openql_mapper.hardware_config cmd/DQCSIM_OPENQL_HARDWARE_CONFIG env");
    }
    if (gatemap_json_fname.empty()) {
      throw std::invalid_argument(
        "Missing openql_mapper.gatemap cmd/DQCSIM_OPENQL_GATEMAP env");
    }

    // Construct the OpenQL platform.
    platform = std::make_shared<ql::quantum_platform>("dqcsim_platform", platform_json_fname);
    platform->print_info();
    ql::set_platform(*platform);
    num_qubits = platform->qubit_number;

    // Construct the mapper.
    // FIXME: this initializes its own private random generator with the
    // current timestamp, but DQCsim plugins should be pure to be
    // reproducible! It should be seeded with DQCsim's random number
    // generator (`state.random()`).
    mapper.Init(*platform);

    // Construct the initial kernel.
    new_kernel();

    // Construct the DQCsim/OpenQL gatemap.
    // TODO: the epsilon value should probably be configurable.
    gatemap = std::make_shared<OpenQLGateMap>(gatemap_json_fname, 1.0e-6);

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
  void allocate(
    dqcs::PluginState &state,
    dqcs::QubitSet &&qubits,
    dqcs::ArbCmdQueue &&cmds
  ) {

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

      // Update the qubit counter.
      dqcs_nq++;

    }

  }

  /**
   * Qubit deallocation callback.
   *
   * Inverse of `allocate()`.
   */
  void free(
    dqcs::PluginState &state,
    dqcs::QubitSet &&qubits
  ) {

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
   * Dumps the current qubit map with debug verbosity.
   */
  void dump_qubit_map() {
    std::string dump;
    char lbuf[64];

    // Print table header.
    dump += "| upstream | virtual  | physical |downstream|\n";
    dump += "|----------|----------|----------|----------|\n";

    // Print mappings for all upstream qubits.
    std::unordered_set<size_t> phys_printed;
    for (size_t dqcs = 1; dqcs <= dqcs_nq; dqcs++) {
      std::string dqcs_str = std::to_string(dqcs);
      std::string virt_str = "-";
      std::string phys_str = "-";
      std::string down_str = "-";

      ssize_t virt = dqcs2virt.forward_lookup(dqcs);
      if (virt >= 0) {
        virt_str = std::to_string(virt);
        ssize_t phys = virt2phys.forward_lookup(virt);
        if (phys >= 0) {
          phys_printed.insert(phys);
          phys_str = std::to_string(phys);
          down_str = std::to_string(phys + 1);
        }
      }

      snprintf(
        lbuf, sizeof(lbuf), "| %8s | %8s | %8s | %8s |\n",
        dqcs_str.c_str(), virt_str.c_str(), phys_str.c_str(), down_str.c_str());
      dump += lbuf;
    }

    // Print mappings for any remaining physical qubits.
    for (size_t phys = 0; phys < num_qubits; phys++) {
      if (phys_printed.count(phys)) {
        continue;
      }
      std::string dqcs_str = "-";
      std::string virt_str = "-";
      std::string phys_str = std::to_string(phys);
      std::string down_str = std::to_string(phys + 1);

      ssize_t virt = virt2phys.reverse_lookup(phys);
      if (virt >= 0) {
        virt_str = std::to_string(virt);
      }

      snprintf(
        lbuf, sizeof(lbuf), "| %8s | %8s | %8s | %8s |\n",
        dqcs_str.c_str(), virt_str.c_str(), phys_str.c_str(), down_str.c_str());
      dump += lbuf;
    }

    DQCSIM_DEBUG("Current qubit mapping:\n%s", dump.c_str());
  }

  /**
   * Dumps a gate with debug verbosity.
   */
  static void dump_gate(
    const std::string &prefix,
    const std::string &qubit_type,
    const OpenQLGateDescription &desc
  ) {
    std::string qubits_string;
    for (size_t qubit : desc.qubits) {
      if (!qubits_string.empty()) {
        qubits_string += ", ";
      }
      qubits_string += std::to_string(qubit);
    }
    DQCSIM_DEBUG(
      "%s gate %s with %s qubit(s) %s and angle %f",
      prefix.c_str(), desc.name.c_str(), qubit_type.c_str(),
      qubits_string.c_str(), desc.angle);
  }

  /**
   * This function runs the mapper for the gates queued up thus far, sends the
   * mapped gates downstream, and returns the measurement result of the last
   * gate if it's a measurement gate.
   */
  void run_mapper(
    dqcs::PluginState &state
  ) {

    // If the current kernel is empty, we don't have to do anything.
    if (kernel->c.empty()) {
      return;
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

    // Dump the current qubit map.
    dump_qubit_map();

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

    // Dump the new qubit map.
    dump_qubit_map();

    // Send the gates downstream, remembering the last gate.
    OpenQLGateDescription desc;
    for (ql::gate *ql_gate : kernel->c) {

      // Convert to DQCsim gates.
      desc.name = ql_gate->name;
      desc.angle = ql_gate->angle;
      desc.qubits.clear();
      for (size_t phys : ql_gate->operands) {
        desc.qubits.push_back(phys + 1);
      }
      dump_gate("Sending", "downstream", desc);
      state.gate(gatemap->construct(desc));

    }

    // Construct a new kernel for the next batch.
    new_kernel();

  }

  /**
   * Gate callback.
   *
   * Measurement gates must be forwarded immediately, but we can queue
   * everything else up in the circuit.
   */
  dqcs::MeasurementSet gate(
    dqcs::PluginState &state,
    dqcs::Gate &&gate
  ) {

    // Convert the DQCsim gate to its OpenQL representation.
    OpenQLGateDescription desc = gatemap->detect(gate);
    dump_gate("Receiving", "upstream", desc);

    // The qubit indices in the vector currently use DQCsim indices. We need to
    // convert them to the current *physical* qubit index, because the mapper
    // maps the circuits without maintaining state (this isn't implemented yet
    // apparently). Instead, we have it assume that the initial state is
    // one-to-one, making physical indices the right ones here.
    for (size_t i = 0; i < desc.qubits.size(); i++) {
      size_t dqcs = desc.qubits[i];
      ssize_t virt = dqcs2virt.forward_lookup(dqcs);
      if (virt < 0) {
        throw std::runtime_error(
          "Missing mapping from DQCsim qubit index " + std::to_string(dqcs) + " to virtual");
      }
      ssize_t phys = virt2phys.forward_lookup(virt);
      if (phys < 0) {
        throw std::runtime_error(
          "Missing mapping from virtual qubit index " + std::to_string(virt) + " to physical");
      }
      desc.qubits[i] = phys;
    }

    // Add the gate to the current kernel.
    if (desc.multi_qubit_parallel) {
      std::vector<size_t> qubits;
      for (size_t qubit : desc.qubits) {
        qubits.push_back(qubit);
        kernel->gate(desc.name, qubits, {}, 0, desc.angle);
        qubits.clear();
      }
    } else {
      kernel->gate(desc.name, desc.qubits, {}, 0, desc.angle);
    }

    // If the gate was a measurement gate, run the mapper now. If we try to
    // queue up the measurement, we might get a deadlock, because the frontend
    // may end up needing the measurement result to determine what the next
    // gate will be.
    if (gate.has_measures()) {
      run_mapper(state);
    }

    // Return the measurements requested by this gates.
    dqcs::MeasurementSet measurements = dqcs::MeasurementSet();
    if (gate.has_measures()) {
      dqcs::QubitSet measures = gate.get_measures();
      while (measures.size()) {

        // Get the upstream qubit reference.
        dqcs::QubitRef up_ref = measures.pop();

        // Convert from upstream qubit index to downstream.
        size_t dqcs = up_ref.get_index();
        ssize_t virt = dqcs2virt.forward_lookup(dqcs);
        if (virt < 0) {
          throw std::runtime_error(
            "Missing mapping from DQCsim qubit index " + std::to_string(dqcs) + " to virtual");
        }
        ssize_t phys = virt2phys.forward_lookup(virt);
        if (phys < 0) {
          throw std::runtime_error(
            "Missing mapping from virtual qubit index " + std::to_string(virt) + " to physical");
        }
        size_t down = phys + 1;

        // Get the downstream qubit reference.
        dqcs::QubitRef down_ref = dqcs::QubitRef(down);

        // Get, convert, and save the measurement result.
        dqcs::Measurement meas = state.get_measurement(down_ref);
        meas.set_qubit(up_ref);
        measurements.set(std::move(meas));

      }
    }

    return measurements;
  }

  /**
   * Modify-measurement callback.
   *
   * This is called when measurement data is received from the downstream
   * plugin and is to be sent upstream implicitly. We do everything explicitly
   * in gate() though, so we never have to return anything here. We have to
   * override it though, because the default behavior for the
   * modify-measurement callback is to pass the results through unchanged.
   */
  dqcs::MeasurementSet modify_measurement(
    dqcs::UpstreamPluginState &state,
    dqcs::Measurement &&measurement
  ) {
    return dqcs::MeasurementSet();
  }

  /**
   * Callback used for advancing simulation time.
   *
   * We currently ignore this. Scheduling logically happens after mapping, so
   * there isn't much we can do with this information at this stage.
   */
  void advance(
    dqcs::PluginState &state,
    dqcs::Cycle cycles
  ) {
    static bool warned = false;
    if (!warned) {
      DQCSIM_WARN(
        "Received request to advance time. This information is discarded, "
        "as scheduling normally happens after mapping!");
      warned = true;
    }
  }

  /**
   * Drop callback.
   *
   * We use this to flush out any pending operations occurring after the last
   * measurement.
   */
  void drop(
    dqcs::PluginState &state
  ) {
    run_mapper(state);
  }

};

int main(int argc, char *argv[]) {
  MapperPlugin mapperPlugin;
  return dqcs::Plugin::Operator("openql_mapper", "JvS", "v0.0")
    .with_initialize(&mapperPlugin, &MapperPlugin::initialize)
    .with_allocate(&mapperPlugin, &MapperPlugin::allocate)
    .with_free(&mapperPlugin, &MapperPlugin::free)
    .with_gate(&mapperPlugin, &MapperPlugin::gate)
    .with_modify_measurement(&mapperPlugin, &MapperPlugin::modify_measurement)
    .with_advance(&mapperPlugin, &MapperPlugin::advance)
    .with_drop(&mapperPlugin, &MapperPlugin::drop)
    .run(argc, argv);
}
