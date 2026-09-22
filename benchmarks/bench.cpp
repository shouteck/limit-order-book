#include <algorithm>
#include <chrono>
#include <cstdio>
#include <cstring>
#include <exception>
#include <string>
#include <vector>

#include "lob/book.hpp"
#include "lob/gen.hpp"
#include "lob/naive_book.hpp"
#include "lob/replay.hpp"

namespace {

using Clock = std::chrono::steady_clock;

struct KindStats {
    const char*          name;
    std::vector<double>  ns;
};

void report(const KindStats& s) {
    if (s.ns.empty()) return;
    auto v = s.ns;
    std::sort(v.begin(), v.end());
    auto pct = [&](double p) {
        return v[static_cast<std::size_t>(p * (v.size() - 1))];
    };
    double sum = 0;
    for (double x : v) sum += x;
    std::printf("%-8s n=%-9zu mean=%-8.0f p50=%-7.0f p90=%-7.0f p99=%-7.0f p99.9=%-8.0f max=%.0f ns\n",
                s.name, v.size(), sum / v.size(),
                pct(0.50), pct(0.90), pct(0.99), pct(0.999), v.back());
}

template <typename BookT>
int bench_with(const std::vector<lob::Event>& events, int runs) {
    KindStats adds{"add"}, cancels{"cancel"}, modifies{"modify"}, all{"all"};
    double best_wall_s = 1e30;

    for (int r = 0; r < runs; ++r) {
        BookT book;
        auto sink = [](const lob::Trade&) {};
        const auto wall0 = Clock::now();
        for (const lob::Event& e : events) {
            const auto t0 = Clock::now();
            switch (e.kind) {
            case lob::EventKind::Add:    book.add(e.order, sink);                       break;
            case lob::EventKind::Cancel: book.cancel(e.order.id);                       break;
            case lob::EventKind::Modify: book.modify(e.order.id, e.order.price, e.order.qty); break;
            }
            const auto t1 = Clock::now();
            const double ns = std::chrono::duration<double, std::nano>(t1 - t0).count();
            all.ns.push_back(ns);
            if      (e.kind == lob::EventKind::Add)    adds.ns.push_back(ns);
            else if (e.kind == lob::EventKind::Cancel) cancels.ns.push_back(ns);
            else                                       modifies.ns.push_back(ns);
        }
        const double wall = std::chrono::duration<double>(Clock::now() - wall0).count();
        if (wall < best_wall_s) best_wall_s = wall;
    }

    std::printf("per-event latency (ns, %d run%s):\n", runs, runs == 1 ? "" : "s");
    report(adds);
    report(cancels);
    report(modifies);
    report(all);
    std::printf("throughput: %.2f M events/sec (best run, %.3fs for %zu events)\n",
                events.size() / best_wall_s / 1e6, best_wall_s, events.size());
    return 0;
}

void usage() {
    std::fprintf(stderr,
        "usage: lob_bench [<events.csv> | --gen N] [--seed S] [--book naive|fast] [--runs R]\n"
        "       [--spread TICKS] [--cancel RATIO] [--modify RATIO]\n"
        "  spread: half-width of resting price band around mid (default 100;\n"
        "          larger = sparser, deeper book)\n");
}

} // namespace

int main(int argc, char** argv) {
    std::string   path;
    std::string   book_name = "naive";
    std::size_t   gen_n  = 0;
    std::uint64_t seed   = 42;
    int           runs   = 3;
    lob::Price    spread = 100;
    double        cancel_ratio = 0.25;
    double        modify_ratio = 0.05;

    for (int i = 1; i < argc; ++i) {
        if      (std::strcmp(argv[i], "--gen") == 0 && i + 1 < argc)    gen_n = std::stoull(argv[++i]);
        else if (std::strcmp(argv[i], "--seed") == 0 && i + 1 < argc)   seed = std::stoull(argv[++i]);
        else if (std::strcmp(argv[i], "--book") == 0 && i + 1 < argc)   book_name = argv[++i];
        else if (std::strcmp(argv[i], "--runs") == 0 && i + 1 < argc)   runs = std::atoi(argv[++i]);
        else if (std::strcmp(argv[i], "--spread") == 0 && i + 1 < argc) spread = std::stoll(argv[++i]);
        else if (std::strcmp(argv[i], "--cancel") == 0 && i + 1 < argc) cancel_ratio = std::stod(argv[++i]);
        else if (std::strcmp(argv[i], "--modify") == 0 && i + 1 < argc) modify_ratio = std::stod(argv[++i]);
        else if (argv[i][0] != '-')                                   path = argv[i];
        else { usage(); return 1; }
    }
    if (path.empty() && gen_n == 0) { usage(); return 1; }

    try {
        std::vector<lob::Event> events =
            gen_n ? lob::generate_events(gen_n, seed, cancel_ratio, modify_ratio, spread)
                  : lob::read_events(path);
        std::printf("benchmarking %zu events on book=%s\n", events.size(), book_name.c_str());
        if (book_name == "naive") return bench_with<lob::NaiveBook>(events, runs);
        if (book_name == "fast")  return bench_with<lob::Book>(events, runs);
        std::fprintf(stderr, "unknown book: %s\n", book_name.c_str());
        return 1;
    } catch (const std::exception& e) {
        std::fprintf(stderr, "error: %s\n", e.what());
        return 1;
    }
}
