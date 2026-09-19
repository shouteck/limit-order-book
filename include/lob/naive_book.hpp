#pragma once

#include <algorithm>
#include <deque>
#include <map>
#include <optional>
#include <unordered_map>

#include "lob/types.hpp"

namespace lob {

// Correctness-first reference implementation.
// std::map price levels + std::deque FIFO per level + hash map for cancel lookup.
// Deliberately simple and a bit slow: deque pop_front, std::function-free but
// map/tree overhead everywhere. This is the benchmark BASELINE that Book beats.
class NaiveBook {
public:
    // Adds `o`; matches against the opposite side first, then rests any
    // remaining quantity (GTC limits only). `on_trade(Trade)` fires per fill.
    template <typename OnTrade>
    void add(const Order& o, OnTrade&& on_trade) {
        Order in = o;
        if (in.tif == TimeInForce::FOK && !fillable(in)) return;
        if (in.side == Side::Buy) match_into(in, asks_, true,  on_trade);
        else                      match_into(in, bids_, false, on_trade);
        if (in.qty > 0 && in.type == OrderType::Limit && in.tif == TimeInForce::GTC)
            rest(in);
    }

    bool cancel(OrderId id);
    bool modify(OrderId id, Price new_price, Quantity new_qty);

    std::optional<Price> best_bid() const;
    std::optional<Price> best_ask() const;
    std::size_t          order_count() const;
    std::uint64_t        resting_qty(Side side) const;  // debug/test aid, O(levels)

private:
    using Level = std::deque<Order>;
    struct Locator { Side side; Price price; };

    std::map<Price, Level, std::greater<Price>> bids_;  // best bid first
    std::map<Price, Level, std::less<Price>>    asks_;  // best ask first
    std::unordered_map<OrderId, Locator>        index_;

    template <typename OppLevels, typename OnTrade>
    void match_into(Order& in, OppLevels& opp, bool in_is_buy, OnTrade&& on_trade) {
        auto it = opp.begin();
        while (it != opp.end() && in.qty > 0) {
            if (in.type == OrderType::Limit &&
                (in_is_buy ? it->first > in.price : it->first < in.price))
                break;
            Level& lvl = it->second;
            while (!lvl.empty() && in.qty > 0) {
                Order& r = lvl.front();
                Quantity n = std::min(in.qty, r.qty);
                on_trade(Trade{in.id, r.id, it->first, n});
                in.qty -= n;
                r.qty  -= n;
                if (r.qty == 0) { index_.erase(r.id); lvl.pop_front(); }
            }
            if (lvl.empty()) it = opp.erase(it);
            else             break;  // incoming order exhausted
        }
    }

    void rest(const Order& o);
    bool fillable(const Order& in) const;  // FOK pre-check
};

} // namespace lob
