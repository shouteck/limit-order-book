#pragma once

#include <optional>
#include <stdexcept>
#include <utility>
#include <algorithm>
#include <unordered_map>
#include <vector>
#include <cstdint>
#include <bit>

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
    explicit Book(std::size_t capacity = 1 << 20);
    template <typename OnTrade>
    void add(const Order& o, OnTrade&& on_trade) {
        add_impl(o, std::forward<OnTrade>(on_trade));
    }

    bool cancel(OrderId id);
    bool modify(OrderId id, Price new_price, Quantity new_qty);

    std::optional<Price> best_bid() const;
    std::optional<Price> best_ask() const;
    std::size_t          order_count() const;
    std::uint64_t        resting_qty(Side side) const;

private:
    template <typename OnTrade>
    void add_impl(const Order& o, OnTrade&& on_trade) {
    
        Order in = o;
    
        // clerk checks if the order is fillable for FOK orders
        if (in.tif == TimeInForce::FOK && !fillable(in)) return;
    
        if (in.side == Side::Buy) match_into(in, asks_lv_, true, on_trade);
        else match_into(in, bids_lv_, false, on_trade);
    
        if (in.qty > 0 && in.type == OrderType::Limit && in.tif == TimeInForce::GTC) {
            rest(in);
        }
    
    }    

    static constexpr Price LO = 0;
    static constexpr std::size_t NPRICES = 1 << 16;

    static constexpr std::size_t NWORDS = NPRICES / 64;

    std::vector<std::uint64_t> occ_bid_;
    std::vector<std::uint64_t> occ_ask_;

    static void set_bit(std::vector<std::uint64_t>& w, int i);
    static void clear_bit(std::vector<std::uint64_t>& w, int i);

    struct Level; // declare the name first

    struct Node {
        Order o;
        Node* prev;
        Node* next;
        Level* lvl;
    };

    struct Level {
        Node* head = nullptr;
        Node* tail = nullptr;
    };

    std::vector<Node> pool_;
    std::vector<uint32_t> free_;

    std::vector<Level> bids_lv_;
    std::vector<Level> asks_lv_;
    std::unordered_map<OrderId, Node*> index_;    

    Node* alloc();
    void release(Node* n);
    void unlink(Level& lvl, Node* n);
    void push_back(Level& lvl, Node* n);    
    Level& lvl_at(std::vector<Level>& lv, Price p);
    const Level& lvl_at(const std::vector<Level>& lv, Price p) const;
    int next_nonempty_bid(int i) const;
    int next_nonempty_ask(int i) const;

    template<typename OppLevels, typename OnTrade>
    void match_into(Order& in, OppLevels& opp, bool in_is_buy, OnTrade&& on_trade) {
        Price aggressorPrice = in.price;
        int i = in_is_buy ? next_nonempty_ask(0) : next_nonempty_bid(NPRICES - 1);
        while (i >= 0 && in.qty > 0) {
            Price restingPrice = LO + i;
            if (in.type == OrderType::Limit && 
                // if you're buying, you can't be buying at a price lower than the lowest ask
                // if you're selling, you can't be selling at a price higher than the highest bid
                (in_is_buy ? aggressorPrice < restingPrice : aggressorPrice > restingPrice))
                break;
            Level& lvl = opp[i];
            while (lvl.head != nullptr && in.qty > 0) {
                Order& r = lvl.head->o;
                Quantity n = std::min(in.qty, r.qty);
                // defining contract for on_trade to accept Trade
                on_trade(Trade{in.id, r.id, restingPrice, n});
                in.qty -= n;
                r.qty -= n;
                if (r.qty == 0) {
                    Node* dead = lvl.head;
                    unlink(lvl, dead);
                    index_.erase(r.id);
                    release(dead);
                }
            }
            if (lvl.head == nullptr) {
                clear_bit(in_is_buy ? occ_ask_ : occ_bid_, i);
                i = in_is_buy ? next_nonempty_ask(i + 1) : next_nonempty_bid(i - 1);
            }
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
