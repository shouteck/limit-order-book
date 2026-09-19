#include <cstdio>
#include <cstring>
#include <exception>
#include <string>
#include <vector>

#include "lob/book.hpp"
#include "lob/naive_book.hpp"
#include "lob/replay.hpp"

namespace {

void usage() {
    std::fprintf(stderr,
        "usage: lob_replay <events.csv> [--book naive|fast] [--trades]\n");
}

double to_dollars(lob::Price p) { return static_cast<double>(p) / 100.0; }

template <typename BookT>
int replay_with(const std::vector<lob::Event>& events, bool print_trades) {
    BookT book;
    std::size_t   trades = 0;
    std::uint64_t filled = 0;
    auto sink = [&](const lob::Trade& t) {
        ++trades;
        filled += t.qty;
        if (print_trades)
            std::printf("T %llu %llu %.2f %u\n",
                        (unsigned long long)t.aggressor_id,
                        (unsigned long long)t.resting_id,
                        to_dollars(t.price), t.qty);
    };

    lob::run(book, events, sink);

    std::printf("events:      %zu\n", events.size());
    std::printf("trades:      %zu\n", trades);
    std::printf("filled qty:  %llu\n", (unsigned long long)filled);
    if (auto b = book.best_bid()) std::printf("best bid:    %.2f\n", to_dollars(*b));
    else                          std::printf("best bid:    -\n");
    if (auto a = book.best_ask()) std::printf("best ask:    %.2f\n", to_dollars(*a));
    else                          std::printf("best ask:    -\n");
    std::printf("open orders: %zu\n", book.order_count());
    return 0;
}

} // namespace

int main(int argc, char** argv) {
    if (argc < 2) { usage(); return 1; }

    std::string path;
    std::string book_name = "naive";
    bool print_trades = false;

    for (int i = 1; i < argc; ++i) {
        if (std::strcmp(argv[i], "--book") == 0 && i + 1 < argc)
            book_name = argv[++i];
        else if (std::strcmp(argv[i], "--trades") == 0)
            print_trades = true;
        else if (argv[i][0] != '-')
            path = argv[i];
        else { usage(); return 1; }
    }
    if (path.empty()) { usage(); return 1; }

    try {
        auto events = lob::read_events(path);
        if (book_name == "naive") return replay_with<lob::NaiveBook>(events, print_trades);
        if (book_name == "fast")  return replay_with<lob::Book>(events, print_trades);
        std::fprintf(stderr, "unknown book: %s\n", book_name.c_str());
        return 1;
    } catch (const std::exception& e) {
        std::fprintf(stderr, "error: %s\n", e.what());
        return 1;
    }
}
