#pragma once

#include <functional>
#include <optional>
#include <stdexcept>
#include <utility>

#include "lob/types.hpp"

namespace lob {

// Book -- the fast implementation. You own everything in here.
//
// Contract (must match NaiveBook's observable behavior):
//   - Price-time priority: better price first, FIFO within a level.
//   - Limit orders match while crossing; remainder rests iff GTC.
//   - Market orders match at whatever is available; never rest.
//   - IOC fills what it can, drops the rest. FOK fills fully or not at all.
//   - cancel() returns false for unknown ids. modify() = cancel + re-add
//     (loses queue priority), returns false for unknown ids.
//
// Perf targets to design for: O(1) add/cancel, no per-order heap alloc,
// no std::function/virtual in the hot path (the template below is a
// placeholder -- replace with a real template once internals exist).
//
// Until implemented, every method throws std::logic_error.
class Book {
public:
    template <typename OnTrade>
    void add(const Order& o, OnTrade&& on_trade) {
        add_impl(o, std::function<void(const Trade&)>(
                        std::forward<OnTrade>(on_trade)));
    }

    bool cancel(OrderId id);
    bool modify(OrderId id, Price new_price, Quantity new_qty);

    std::optional<Price> best_bid() const;
    std::optional<Price> best_ask() const;
    std::size_t          order_count() const;
    std::uint64_t        resting_qty(Side side) const;

private:
    void add_impl(const Order& o, const std::function<void(const Trade&)>& on_trade);

    // TODO(you): internal representation -- price levels, order index,
    // arena/pool allocator, intrusive FIFO, flat-array levels...
};

} // namespace lob
