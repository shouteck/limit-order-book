#include "test_util.hpp"

#include <vector>

#include "lob/book.hpp"
#include "lob/gen.hpp"
#include "lob/naive_book.hpp"
#include "lob/replay.hpp"

// Property tests over generated event streams, run against every book type.

namespace {

template <typename B>
void apply(B& book, const lob::Event& e) {
    switch (e.kind) {
    case lob::EventKind::Add:    book.add(e.order, [](const lob::Trade&) {});         break;
    case lob::EventKind::Cancel: book.cancel(e.order.id);                             break;
    case lob::EventKind::Modify: book.modify(e.order.id, e.order.price, e.order.qty); break;
    }
}

// The book must never be crossed: best_bid < best_ask after every event.
template <typename B> void t_never_crossed() {
    auto events = lob::generate_events(20000, /*seed*/42, 0.30, 0.10);
    B b;
    for (const auto& e : events) {
        apply(b, e);
        auto bb = b.best_bid();
        auto ba = b.best_ask();
        if (bb && ba) CHECK(*bb < *ba);
    }
}

// Same seed + same book type => identical trade sequence.
template <typename B> void t_deterministic() {
    auto events = lob::generate_events(20000, /*seed*/7, 0.30, 0.10);
    auto collect = [&] {
        B b;
        std::vector<lob::Trade> ts;
        lob::run(b, events, [&](const lob::Trade& t) { ts.push_back(t); });
        return ts;
    };
    auto a = collect();
    auto b = collect();
    CHECK_EQ(a.size(), b.size());
    for (std::size_t i = 0; i < a.size(); ++i) {
        CHECK(a[i].aggressor_id == b[i].aggressor_id);
        CHECK(a[i].resting_id   == b[i].resting_id);
        CHECK(a[i].price        == b[i].price);
        CHECK(a[i].qty          == b[i].qty);
    }
}

// Quantity conservation: with no cancels/modifies and only GTC limits,
// added == 2*traded + still resting (each fill consumes both sides).
template <typename B> void t_qty_conserved() {
    auto events = lob::generate_events(20000, /*seed*/3,
                                     /*cancel*/0.0, /*modify*/0.0);
    B b;
    std::uint64_t added = 0, traded = 0;
    for (const auto& e : events) {
        if (e.kind != lob::EventKind::Add) continue;
        added += e.order.qty;
        b.add(e.order, [&](const lob::Trade& t) { traded += t.qty; });
    }
    std::uint64_t resting = b.resting_qty(lob::Side::Buy) +
                            b.resting_qty(lob::Side::Sell);
    CHECK_EQ(added, 2 * traded + resting);
}

// Nonzero resting qty iff a top-of-book exists on that side.
template <typename B> void t_no_zero_qty() {
    auto events = lob::generate_events(20000, /*seed*/11, 0.30, 0.10);
    B b;
    for (const auto& e : events) apply(b, e);
    CHECK(b.resting_qty(lob::Side::Buy) > 0 || !b.best_bid().has_value());
    CHECK(b.resting_qty(lob::Side::Sell) > 0 || !b.best_ask().has_value());
}

// Cross-book differential: same tape through both implementations must
// produce identical trades. The oracle is NaiveBook's known-good behavior.
TEST(props_differential_naive_vs_fast) {
    auto events = lob::generate_events(50000, /*seed*/99, 0.30, 0.10);
    auto collect = [&](auto& book) {
        std::vector<lob::Trade> ts;
        lob::run(book, events, [&](const lob::Trade& t) { ts.push_back(t); });
        return ts;
    };
    lob::NaiveBook nb;
    lob::Book      fb;
    auto a = collect(nb);
    auto b = collect(fb);
    CHECK_EQ(a.size(), b.size());
    for (std::size_t i = 0; i < a.size(); ++i) {
        CHECK(a[i].aggressor_id == b[i].aggressor_id);
        CHECK(a[i].resting_id   == b[i].resting_id);
        CHECK(a[i].price        == b[i].price);
        CHECK(a[i].qty          == b[i].qty);
    }
}

} // namespace

#define PROPS_SUITE(P, T)                                   \
    TEST(P##_never_crossed) { t_never_crossed<T>(); }       \
    TEST(P##_deterministic) { t_deterministic<T>(); }       \
    TEST(P##_qty_conserved) { t_qty_conserved<T>(); }       \
    TEST(P##_no_zero_qty)   { t_no_zero_qty<T>(); }

PROPS_SUITE(props_naive_, lob::NaiveBook)
PROPS_SUITE(props_fast_,  lob::Book)
