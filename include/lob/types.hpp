#pragma once

#include <cstdint>

namespace lob {

using OrderId   = std::uint64_t;
using Price     = std::int64_t;   // fixed-point ticks (e.g. 10000 = $100.00)
using Quantity  = std::uint32_t;
using Timestamp = std::uint64_t;

enum class Side        : std::uint8_t { Buy, Sell };
enum class OrderType   : std::uint8_t { Limit, Market };
enum class TimeInForce : std::uint8_t { GTC, IOC, FOK };

struct Order {
    OrderId     id    = 0;
    Side        side  = Side::Buy;
    OrderType   type  = OrderType::Limit;
    TimeInForce tif   = TimeInForce::GTC;
    Price       price = 0;
    Quantity    qty   = 0;
    Timestamp   ts    = 0;
};

struct Trade {
    OrderId  aggressor_id = 0;
    OrderId  resting_id   = 0;
    Price    price        = 0;
    Quantity qty          = 0;
};

// Read-model snapshot of one occupied price level (for depth queries).
struct LevelDepth {
    Price    price = 0;
    Quantity qty   = 0;
};

enum class EventKind : std::uint8_t { Add, Cancel, Modify };

// A single instruction to the book. For Add, `order` is fully populated.
// For Cancel, only order.id matters. For Modify, order.id + new price/qty.
struct Event {
    EventKind kind = EventKind::Add;
    Order     order{};
};

} // namespace lob
