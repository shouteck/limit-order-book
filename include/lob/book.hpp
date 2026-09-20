#pragma once

#include <functional>
#include <optional>
#include <stdexcept>
#include <utility>
#include <algorithm>
#include <deque>
#include <map>
#include <unordered_map>

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

    using Level = std::deque<Order>;
    struct Locator { Side side; Price price; };

    std::map<Price, Level, std::greater<Price>> bids_;  // best bid first
    std::map<Price, Level, std::less<Price>>    asks_;  // best ask first
    std::unordered_map<OrderId, Locator>        index_;    

    template<typename OppLevels, typename OnTrade>
    void match_into(Order& in, OppLevels& opp, bool in_is_buy, OnTrade&& on_trade) {
        Price aggressorPrice = in.price;
        auto it = opp.begin();
        while (it != opp.end() && in.qty > 0) {
            Price restingPrice = it->first;
            if (in.type == OrderType::Limit && 
                // if you're buying, you can't be buying at a price lower than the lowest ask
                // if you're selling, you can't be selling at a price higher than the highest bid
                (in_is_buy ? aggressorPrice < restingPrice : aggressorPrice > restingPrice))
                break;
            Level& lvl = it->second;
            while (!lvl.empty() && in.qty > 0) {
                Order& r = lvl.front();
                Quantity n = std::min(in.qty, r.qty);
                // defining contract for on_trade to accept Trade
                on_trade(Trade{in.id, r.id, restingPrice, n});
                in.qty -= n;
                r.qty -= n;
                if (r.qty == 0) {
                    index_.erase(r.id);
                    lvl.pop_front();
                }
            }
            if (lvl.empty()) it = opp.erase(it);
            // aggressor is done
            else break;
        }
    }

    void rest(const Order& o);
    bool fillable(const Order& in) const;  // FOK pre-check    

    // TODO(you): internal representation -- price levels, order index,
    // arena/pool allocator, intrusive FIFO, flat-array levels...
};

} // namespace lob
