#include "lob/book.hpp"

namespace lob {

namespace {
[[noreturn]] void not_implemented(const char* what) {
    throw std::logic_error(std::string("Book::") + what +
                           " not implemented -- this is your part. See book.hpp");
}
} // namespace

void Book::rest(const Order& o) {
    if (o.side == Side::Buy) bids_[o.price].push_back(o);
    else                     asks_[o.price].push_back(o);
    index_[o.id] = Locator{o.side, o.price};
    // this is for cancel
    // the id tells us which map
    // the price tells us which level
}

bool Book::fillable(const Order& in) const {
    Quantity have = 0;
    auto accumulate = [&](const auto& opp, bool in_is_buy) {
        for (const auto& [px, lvl] : opp) {
            if (in.type == OrderType::Limit &&
                (in_is_buy ? px > in.price : px < in.price))
                break;
            for (const Order& r : lvl) {
                have += r.qty;
                if (have >= in.qty) return;
            }
        }
    };
    if (in.side == Side::Buy) accumulate(asks_, true);
    else                      accumulate(bids_, false);
    return have >= in.qty;
}

bool Book::cancel(OrderId id) {
    auto it = index_.find(id);
    if (it == index_.end()) return false;
    const Locator loc = it->second;

    auto erase_in = [&](auto& levels) {
        auto lv = levels.find(loc.price);
        if (lv == levels.end()) return;
        Level& q = lv->second;
        for (auto oit = q.begin(); oit != q.end(); ++oit) {
            if (oit->id == id) { q.erase(oit); break; }
        }
        if (q.empty()) levels.erase(lv);
    };
    if (loc.side == Side::Buy) erase_in(bids_);
    else                       erase_in(asks_);
    index_.erase(it);
    return true;
}

bool Book::modify(OrderId id, Price new_price, Quantity new_qty) {
    auto it = index_.find(id);
    if (it == index_.end()) return false;
    const Locator loc = it->second;

    Order o{};
    bool found = false;
    auto grab = [&](auto& levels) {
        auto lv = levels.find(loc.price);
        if (lv == levels.end()) return;
        for (const Order& r : lv->second) {
            if (r.id == id) {
                o = r;
                found = true;
                return;
            }
        }
    };

    if (loc.side == Side::Buy) grab(bids_);
    else                       grab(asks_);
    if (!found) return false;

    cancel(id);
    o.price = new_price;
    o.qty = new_qty;
    add(o, [](const Trade&) {});
    return true;

}

void Book::add_impl(const Order& o, const std::function<void(const Trade&)>& on_trade) {
    
    Order in = o;

    // clerk checks if the order is fillable for FOK orders
    if (in.tif == TimeInForce::FOK && !fillable(in)) return;

    if (in.side == Side::Buy) match_into(in, asks_, true, on_trade);
    else match_into(in, bids_, false, on_trade);

    if (in.qty > 0 && in.type == OrderType::Limit && in.tif == TimeInForce::GTC) {
        rest(in);
    }

}

std::optional<Price> Book::best_bid() const { 
    if (bids_.empty()) return std::nullopt;
    return bids_.begin()->first;
}
std::optional<Price> Book::best_ask() const { 
    if (asks_.empty()) return std::nullopt;
    return asks_.begin()->first;
}
std::size_t Book::order_count() const { 
    return index_.size();
}
std::uint64_t Book::resting_qty(Side side) const { 
    std::uint64_t total = 0;
    auto sum = [&](const auto& levels) {
        for (const auto& [px, lvl] : levels)
            for (const Order& o : lvl) total += o.qty;
    };
    if (side == Side::Buy) sum(bids_);
    else                   sum(asks_);
    return total;
}

} // namespace lob
