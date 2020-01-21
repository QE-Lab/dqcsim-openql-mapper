#include <unordered_map>
#include <utility>

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
