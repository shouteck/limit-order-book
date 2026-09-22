#include "lob/gen.hpp"

#include <random>

namespace lob {

std::vector<Event> generate_events(std::size_t n, std::uint64_t seed,
                                   double cancel_ratio, double modify_ratio,
                                   Price spread) {
    std::mt19937_64 rng(seed);
    std::uniform_real_distribution<double> u(0.0, 1.0);

    std::vector<Event>   out;
    std::vector<OrderId> live;   // ids that *might* still be resting
    out.reserve(n);
    live.reserve(n / 2);

    Price   mid  = 10000;
    OrderId next = 1;

    for (std::size_t i = 0; i < n; ++i) {
        const double r = u(rng);

        if (!live.empty() && r < cancel_ratio) {
            const std::size_t k = static_cast<std::size_t>(rng() % live.size());
            Event e{EventKind::Cancel, {}};
            e.order.id = live[k];
            live[k] = live.back();
            live.pop_back();
            out.push_back(e);
        } else if (!live.empty() && r < cancel_ratio + modify_ratio) {
            const std::size_t k = static_cast<std::size_t>(rng() % live.size());
            Event e{EventKind::Modify, {}};
            e.order.id    = live[k];
            e.order.price = mid + static_cast<Price>(rng() % (2 * spread + 1)) - spread;
            if (e.order.price < 1)     e.order.price = 1;
            if (e.order.price > 60000) e.order.price = 60000;
            e.order.qty   = static_cast<Quantity>(1 + rng() % 200);
            out.push_back(e);
        } else {
            Event e{EventKind::Add, {}};
            Order& o  = e.order;
            o.id      = next++;
            o.ts      = i;
            o.type    = OrderType::Limit;
            o.tif     = TimeInForce::GTC;
            o.side    = (rng() & 1) ? Side::Buy : Side::Sell;
            const bool aggressive = u(rng) < 0.20;
            const Price  cross      = spread / 4 + 1;   // aggressive band
            if (o.side == Side::Buy) {
                o.price = aggressive ? mid + static_cast<Price>(rng() % cross)
                                     : mid - static_cast<Price>(1 + rng() % spread);
            } else {
                o.price = aggressive ? mid - static_cast<Price>(rng() % cross)
                                     : mid + static_cast<Price>(1 + rng() % spread);
            }
            // keep generated prices inside Book's domain [LO, LO+NPRICES)
            if (o.price < 1)     o.price = 1;
            if (o.price > 60000) o.price = 60000;
            o.qty = static_cast<Quantity>(1 + rng() % 200);
            out.push_back(e);
            live.push_back(o.id);
        }

        mid += static_cast<Price>(rng() % 11) - 5;
        if (mid < 5000)  mid = 5000;
        if (mid > 15000) mid = 15000;
    }
    return out;
}

} // namespace lob
