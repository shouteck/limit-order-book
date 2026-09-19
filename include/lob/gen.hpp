#pragma once

#include <cstddef>
#include <cstdint>
#include <vector>

#include "lob/types.hpp"

namespace lob {

// Deterministic synthetic event stream: random-walk mid price, mix of
// resting + aggressive GTC limit orders, plus cancels/modifies of live ids.
// Emits only Limit+GTC orders so quantity is conserved (test-friendly).
// Ratios are probabilities per step; remainder is adds.
std::vector<Event> generate_events(std::size_t  n,
                                   std::uint64_t seed,
                                   double        cancel_ratio = 0.25,
                                   double        modify_ratio = 0.05);

} // namespace lob
