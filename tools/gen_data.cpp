#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <exception>
#include <string>

#include "lob/gen.hpp"
#include "lob/replay.hpp"

namespace {

void usage() {
    std::fprintf(stderr,
        "usage: lob_gen --out <file.csv> [--orders N] [--seed S]\n"
        "               [--cancel-ratio R] [--modify-ratio R]\n");
}

} // namespace

int main(int argc, char** argv) {
    std::string  out;
    std::size_t  orders       = 100000;
    std::uint64_t seed        = 42;
    double       cancel_ratio = 0.25;
    double       modify_ratio = 0.05;

    for (int i = 1; i < argc; ++i) {
        auto next = [&](const char* name) -> const char* {
            if (i + 1 >= argc) { usage(); std::exit(1); }
            (void)name;
            return argv[++i];
        };
        if      (std::strcmp(argv[i], "--out") == 0)          out = next("--out");
        else if (std::strcmp(argv[i], "--orders") == 0)       orders = std::stoull(next("--orders"));
        else if (std::strcmp(argv[i], "--seed") == 0)         seed = std::stoull(next("--seed"));
        else if (std::strcmp(argv[i], "--cancel-ratio") == 0) cancel_ratio = std::stod(next("--cancel-ratio"));
        else if (std::strcmp(argv[i], "--modify-ratio") == 0) modify_ratio = std::stod(next("--modify-ratio"));
        else { usage(); return 1; }
    }
    if (out.empty()) { usage(); return 1; }

    try {
        auto events = lob::generate_events(orders, seed, cancel_ratio, modify_ratio);
        lob::write_events(out, events);
        std::printf("wrote %zu events to %s\n", events.size(), out.c_str());
    } catch (const std::exception& e) {
        std::fprintf(stderr, "error: %s\n", e.what());
        return 1;
    }
    return 0;
}
