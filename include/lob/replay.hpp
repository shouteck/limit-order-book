#pragma once

#include <string>
#include <vector>

#include "lob/types.hpp"

namespace lob {

// Replay format is CSV, one event per line ('#' comments and blanks ignored):
//   A,<id>,<B|S>,<L|M>,<G|I|F>,<price>,<qty>   add
//   C,<id>                                     cancel
//   M,<id>,<price>,<qty>                       modify (loses priority)
std::vector<Event> read_events(const std::string& path);
void               write_events(const std::string& path, const std::vector<Event>& events);

// Applies every event to `book` in order; `on_trade(Trade)` fires per fill.
// Deterministic: same events + same book type => same trade sequence.
template <typename BookT, typename OnTrade>
void run(BookT& book, const std::vector<Event>& events, OnTrade&& on_trade) {
    for (const Event& e : events) {
        switch (e.kind) {
        case EventKind::Add:    book.add(e.order, on_trade);                            break;
        case EventKind::Cancel: book.cancel(e.order.id);                                break;
        case EventKind::Modify: book.modify(e.order.id, e.order.price, e.order.qty);    break;
        }
    }
}

} // namespace lob
