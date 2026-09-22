// Emscripten bindings: exposes the real Book + event generator to JS.
// JS creates Events, steps them through JsBook::apply, reads back trades
// and depth snapshots for rendering. Same code that runs the benchmark.
#include <emscripten/bind.h>

#include "lob/book.hpp"
#include "lob/gen.hpp"

namespace {

class JsBook {
public:
    explicit JsBook(std::size_t capacity) : book_(capacity) {}

    // Apply one event; returns the trades it produced (empty for
    // cancels/modifies — same observable contract as the C++ callers).
    std::vector<lob::Trade> apply(const lob::Event& e) {
        trades_.clear();
        switch (e.kind) {
        case lob::EventKind::Add:
            book_.add(e.order, [this](const lob::Trade& t) {
                trades_.push_back(t);
            });
            break;
        case lob::EventKind::Cancel:
            book_.cancel(e.order.id);
            break;
        case lob::EventKind::Modify:
            book_.modify(e.order.id, e.order.price, e.order.qty);
            break;
        }
        return trades_;
    }

    // -1 = side empty (optional isn't registered; sentinel keeps JS simple)
    double bestBid() const {
        auto p = book_.best_bid();
        return p ? static_cast<double>(*p) : -1.0;
    }
    double bestAsk() const {
        auto p = book_.best_ask();
        return p ? static_cast<double>(*p) : -1.0;
    }

    std::vector<lob::LevelDepth> depthBid(int max) const {
        return book_.depth(lob::Side::Buy, static_cast<std::size_t>(max));
    }
    std::vector<lob::LevelDepth> depthAsk(int max) const {
        return book_.depth(lob::Side::Sell, static_cast<std::size_t>(max));
    }

    std::size_t orderCount() const { return book_.order_count(); }

private:
    lob::Book                  book_;
    std::vector<lob::Trade>    trades_;
};

std::vector<lob::Event> generateEvents(std::size_t n, std::uint64_t seed,
                                       double cancel_ratio,
                                       double modify_ratio,
                                       lob::Price spread) {
    return lob::generate_events(n, seed, cancel_ratio, modify_ratio, spread);
}

} // namespace

EMSCRIPTEN_BINDINGS(lob) {
    using namespace emscripten;

    enum_<lob::Side>("Side")
        .value("Buy", lob::Side::Buy)
        .value("Sell", lob::Side::Sell);
    enum_<lob::OrderType>("OrderType")
        .value("Limit", lob::OrderType::Limit)
        .value("Market", lob::OrderType::Market);
    enum_<lob::TimeInForce>("TimeInForce")
        .value("GTC", lob::TimeInForce::GTC)
        .value("IOC", lob::TimeInForce::IOC)
        .value("FOK", lob::TimeInForce::FOK);
    enum_<lob::EventKind>("EventKind")
        .value("Add", lob::EventKind::Add)
        .value("Cancel", lob::EventKind::Cancel)
        .value("Modify", lob::EventKind::Modify);

    value_object<lob::Order>("Order")
        .field("id",    &lob::Order::id)
        .field("side",  &lob::Order::side)
        .field("type",  &lob::Order::type)
        .field("tif",   &lob::Order::tif)
        .field("price", &lob::Order::price)
        .field("qty",   &lob::Order::qty)
        .field("ts",    &lob::Order::ts);

    value_object<lob::Trade>("Trade")
        .field("aggressorId", &lob::Trade::aggressor_id)
        .field("restingId",   &lob::Trade::resting_id)
        .field("price",       &lob::Trade::price)
        .field("qty",         &lob::Trade::qty);

    value_object<lob::LevelDepth>("LevelDepth")
        .field("price", &lob::LevelDepth::price)
        .field("qty",   &lob::LevelDepth::qty);

    value_object<lob::Event>("Event")
        .field("kind",  &lob::Event::kind)
        .field("order", &lob::Event::order);

    register_vector<lob::Event>("EventVector");
    register_vector<lob::Trade>("TradeVector");
    register_vector<lob::LevelDepth>("LevelDepthVector");

    class_<JsBook>("JsBook")
        .constructor<std::size_t>()
        .function("apply",      &JsBook::apply)
        .function("bestBid",    &JsBook::bestBid)
        .function("bestAsk",    &JsBook::bestAsk)
        .function("depthBid",   &JsBook::depthBid)
        .function("depthAsk",   &JsBook::depthAsk)
        .function("orderCount", &JsBook::orderCount);

    function("generateEvents", &generateEvents);
}
