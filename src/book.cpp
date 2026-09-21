#include "lob/book.hpp"

namespace lob {

namespace {
[[noreturn]] void not_implemented(const char* what) {
    throw std::logic_error(std::string("Book::") + what +
                           " not implemented -- this is your part. See book.hpp");
}
} // namespace

Book::Book(std::size_t capacity) {
    // create n Node objects
    // pool_[0..N-1] all exist
    pool_.resize(capacity);
    // raw space for N elements, size still 0
    // no objects created, just dont reallocate later
    free_.reserve(capacity);
    for (std::uint32_t i = 0; i < capacity; ++i) {
        free_.push_back(i);
    }
    bids_lv_.resize(NPRICES);
    asks_lv_.resize(NPRICES);
}

Book::Level& Book::lvl_at(std::vector<Level>& lv, Price p) {

    if (p < LO || p >= LO + static_cast<Price>(NPRICES))
        throw std::out_of_range("Book: price out of range");
    return lv[static_cast<std::size_t>(p - LO)];

}

const Book::Level& Book::lvl_at(const std::vector<Level>& lv, Price p) const {

    if (p < LO || p >= LO + static_cast<Price>(NPRICES))
        throw std::out_of_range("Book: price out of range");
    return lv[static_cast<std::size_t>(p - LO)];

}

int Book::next_nonempty_bid(int i) const {
    for (int j = i; j >= 0; --j) {
        if (bids_lv_[j].head != nullptr) return j;
    }
    return -1;
}

int Book::next_nonempty_ask(int i) const {
    for (int j = i; j < (int)NPRICES; ++j) {
        if (asks_lv_[j].head != nullptr) return j;
    }
    return -1;
}

Book::Node* Book::alloc() {
    if (free_.empty())
        throw std::runtime_error("Book: order pool exhausted");
    Node* n = &pool_[free_.back()];
    free_.pop_back();
    return n;
}

void Book::release(Node * n) {
    /*
    slot 0  ->  0x1000
    slot 1  ->  0x1020     (0x1000 + 1*32)
    slot 2  ->  0x1040     (0x1000 + 2*32)
    slot 3  ->  0x1060
    Now someone hands you n = 0x1040 and asks "which slot is that?":
    n - pool_.data()   =   (0x1040 - 0x1000) / 32   =   0x40 / 32   =   2
    */
    free_.push_back(static_cast<uint32_t>(n - pool_.data()));
}

void Book::unlink(Level& lvl, Node* n) {

    if (n->prev) n->prev->next = n->next;
    else lvl.head = n->next;
    if (n->next) n->next->prev = n->prev;
    else lvl.tail = n->prev;

}

void Book::push_back(Level& lvl, Node* n) {

    n->prev = lvl.tail;
    n->next = nullptr;
    if (lvl.tail) lvl.tail->next = n;
    else lvl.head = n;
    lvl.tail = n;

}

void Book::rest(const Order& o) {
    // 1. find/create the queue's doorway (map auto-creates an empty Level)
    Level& lvl = lvl_at(o.side == Side::Buy ? bids_lv_ : asks_lv_, o.price);

    // 2. claim a parking space
    Node* n = alloc();
    n->o    = o;        // write the order into the slot
    n->lvl  = &lvl;     // <-- the backpointer: remember which queue owns me

    // 3. join the back of the line
    push_back(lvl, n);
 
    // 4. record in the notebook — the order ITSELF now, not directions to it
    index_[o.id] = n;
}

bool Book::fillable(const Order& in) const {
    Quantity have = 0;
    auto accumulate = [&](const auto& opp, bool in_is_buy) {
        int i = in_is_buy ? 0 : static_cast<int>(NPRICES) - 1;
        for (; i >= 0 && i < (int)NPRICES; in_is_buy ? ++i : --i) {
            const Price px = LO + i;
            if (in.type == OrderType::Limit &&
                (in_is_buy ? px > in.price : px < in.price))
                break;
            const Level& lvl = opp[i];
            Node* n = lvl.head;
            while (n != nullptr) {
                have += n->o.qty;
                if (have >= in.qty) return;
                n = n->next;
            }
        }
    };
    if (in.side == Side::Buy) accumulate(asks_lv_, true);
    else                      accumulate(bids_lv_, false);
    return have >= in.qty;
}

bool Book::cancel(OrderId id) {
    auto it = index_.find(id);
    if (it == index_.end()) return false;
    Node* n = it->second;
    unlink(*n->lvl, n);
    index_.erase(it);
    release(n);
    return true;
}

bool Book::modify(OrderId id, Price new_price, Quantity new_qty) {
    auto it = index_.find(id);
    if (it == index_.end()) return false;

    Node* n = it->second;
    Order o = n->o;

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

    if (in.side == Side::Buy) match_into(in, asks_lv_, true, on_trade);
    else match_into(in, bids_lv_, false, on_trade);

    if (in.qty > 0 && in.type == OrderType::Limit && in.tif == TimeInForce::GTC) {
        rest(in);
    }

}

std::optional<Price> Book::best_bid() const { 
    int i = next_nonempty_bid(static_cast<int>(NPRICES) - 1);
    if (i < 0) return std::nullopt;
    return LO + i;
}
std::optional<Price> Book::best_ask() const { 
    int i = next_nonempty_ask(0);
    if (i < 0) return std::nullopt;
    return LO + i;
}
std::size_t Book::order_count() const { 
    return index_.size();
}
std::uint64_t Book::resting_qty(Side side) const { 
    std::uint64_t total = 0;
    const std::vector<Level>& lv = 
        (side == Side::Buy) ? bids_lv_ : asks_lv_;
    for (const Level& lvl : lv) {
        for (Node* n = lvl.head; n; n = n->next)
            total += n->o.qty;
    }
    return total;
}

} // namespace lob
