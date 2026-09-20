#include "test_util.hpp"

#include <vector>

#include "lob/book.hpp"
#include "lob/naive_book.hpp"

// Behavioral suite, templated on book type so the same checks run against
// NaiveBook today and Book once implemented. Each helper builds fresh state.

namespace {

lob::Order limit(lob::OrderId id, lob::Side side, lob::Price px, lob::Quantity q,
                 lob::TimeInForce tif = lob::TimeInForce::GTC) {
    lob::Order o;
    o.id = id; o.side = side; o.price = px; o.qty = q; o.tif = tif;
    return o;
}

lob::Order market(lob::OrderId id, lob::Side side, lob::Quantity q) {
    lob::Order o;
    o.id = id; o.side = side; o.qty = q; o.type = lob::OrderType::Market;
    return o;
}

template <typename B> void t_rest_no_cross() {
    B b;
    b.add(limit(1, lob::Side::Buy, 9900, 10), [](const lob::Trade&) {});
    CHECK(b.best_bid().has_value());
    CHECK_EQ(*b.best_bid(), 9900);
    CHECK(!b.best_ask().has_value());
    CHECK_EQ(b.order_count(), 1u);
}

template <typename B> void t_simple_cross() {
    B b;
    std::vector<lob::Trade> ts;
    auto sink = [&](const lob::Trade& t) { ts.push_back(t); };
    b.add(limit(1, lob::Side::Buy, 10000, 10), sink);
    b.add(limit(2, lob::Side::Sell, 10000, 4), sink);
    CHECK_EQ(ts.size(), 1u);
    CHECK_EQ(ts[0].aggressor_id, 2ull);
    CHECK_EQ(ts[0].resting_id, 1ull);
    CHECK_EQ(ts[0].qty, 4u);
    CHECK_EQ(ts[0].price, 10000);          // trade at resting price
    CHECK_EQ(*b.best_bid(), 10000);        // 6 remaining
    CHECK_EQ(b.resting_qty(lob::Side::Buy), 6ull);
}

template <typename B> void t_price_priority() {
    B b;
    std::vector<lob::Trade> ts;
    auto sink = [&](const lob::Trade& t) { ts.push_back(t); };
    b.add(limit(1, lob::Side::Sell, 10100, 5), sink);
    b.add(limit(2, lob::Side::Sell, 10000, 5), sink);
    b.add(limit(3, lob::Side::Buy, 10200, 8), sink);
    CHECK_EQ(ts.size(), 2u);
    CHECK_EQ(ts[0].resting_id, 2ull);      // best ask first
    CHECK_EQ(ts[0].price, 10000);
    CHECK_EQ(ts[1].resting_id, 1ull);
    CHECK_EQ(ts[1].price, 10100);
    CHECK_EQ(ts[1].qty, 3u);
}

template <typename B> void t_fifo_within_level() {
    B b;
    std::vector<lob::Trade> ts;
    auto sink = [&](const lob::Trade& t) { ts.push_back(t); };
    b.add(limit(1, lob::Side::Buy, 10000, 5), sink);
    b.add(limit(2, lob::Side::Buy, 10000, 5), sink);
    b.add(limit(3, lob::Side::Sell, 10000, 6), sink);
    CHECK_EQ(ts.size(), 2u);
    CHECK_EQ(ts[0].resting_id, 1ull);      // first in, first out
    CHECK_EQ(ts[0].qty, 5u);
    CHECK_EQ(ts[1].resting_id, 2ull);
    CHECK_EQ(ts[1].qty, 1u);
}

template <typename B> void t_cancel() {
    B b;
    b.add(limit(1, lob::Side::Buy, 9900, 10), [](const lob::Trade&) {});
    CHECK(b.cancel(1));
    CHECK(!b.best_bid().has_value());
    CHECK_EQ(b.order_count(), 0u);
    CHECK(!b.cancel(1));                   // already gone
    CHECK(!b.cancel(999));                 // never existed
}

template <typename B> void t_ioc_drops_remainder() {
    B b;
    b.add(limit(1, lob::Side::Sell, 10000, 5), [](const lob::Trade&) {});
    b.add(limit(2, lob::Side::Buy, 10000, 10, lob::TimeInForce::IOC),
          [](const lob::Trade&) {});
    CHECK_EQ(b.order_count(), 0u);         // 5 unfilled IOC qty did not rest
    CHECK(!b.best_bid().has_value());
}

template <typename B> void t_fok_all_or_nothing() {
    B b;
    std::vector<lob::Trade> ts;
    auto sink = [&](const lob::Trade& t) { ts.push_back(t); };
    b.add(limit(1, lob::Side::Sell, 10000, 5), sink);
    b.add(limit(2, lob::Side::Buy, 10000, 10, lob::TimeInForce::FOK), sink);
    CHECK(ts.empty());                     // not enough liquidity: no trade
    CHECK_EQ(b.order_count(), 1u);
    b.add(limit(3, lob::Side::Buy, 10000, 5, lob::TimeInForce::FOK), sink);
    CHECK_EQ(ts.size(), 1u);               // exactly enough: fills
    CHECK_EQ(b.order_count(), 0u);
}

template <typename B> void t_market_order() {
    B b;
    std::vector<lob::Trade> ts;
    auto sink = [&](const lob::Trade& t) { ts.push_back(t); };
    b.add(limit(1, lob::Side::Sell, 10050, 5), sink);
    b.add(market(2, lob::Side::Buy, 3), sink);
    CHECK_EQ(ts.size(), 1u);
    CHECK_EQ(ts[0].price, 10050);
    CHECK_EQ(b.order_count(), 1u);
}

template <typename B> void t_modify_loses_priority() {
    B b;
    std::vector<lob::Trade> ts;
    auto sink = [&](const lob::Trade& t) { ts.push_back(t); };
    b.add(limit(1, lob::Side::Buy, 10000, 5), sink);
    b.add(limit(2, lob::Side::Buy, 10000, 5), sink);
    CHECK(b.modify(1, 10000, 5));          // same price: still new timestamp
    b.add(limit(3, lob::Side::Sell, 10000, 6), sink);
    CHECK_EQ(ts.size(), 2u);
    CHECK_EQ(ts[0].resting_id, 2ull);      // order 2 now has priority
    CHECK(!b.modify(999, 1, 1));
}

} // namespace

#define BOOK_SUITE(P, T)                  \
    TEST(P##_rest)          { t_rest_no_cross<T>(); }        \
    TEST(P##_cross)         { t_simple_cross<T>(); }         \
    TEST(P##_price_prio)    { t_price_priority<T>(); }       \
    TEST(P##_fifo)          { t_fifo_within_level<T>(); }    \
    TEST(P##_cancel)        { t_cancel<T>(); }               \
    TEST(P##_ioc)           { t_ioc_drops_remainder<T>(); }  \
    TEST(P##_fok)           { t_fok_all_or_nothing<T>(); }   \
    TEST(P##_market)        { t_market_order<T>(); }         \
    TEST(P##_modify_prio)   { t_modify_loses_priority<T>(); }

BOOK_SUITE(naive_, lob::NaiveBook)
BOOK_SUITE(fast_, lob::Book)   // uncomment once Book is implemented
