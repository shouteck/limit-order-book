# lob — limit order book / matching engine

A price-time-priority matching engine in modern C++, built as a portfolio
project for quant-dev / backend roles. Exchange infrastructure, not a trading
bot: deterministic event replay, property tests, and honest latency numbers.

## Layout

```
include/lob/    types.hpp      core typedefs + Order/Trade/Event
                book.hpp       Book — the fast implementation (your code)
                naive_book.hpp NaiveBook — correct baseline (std::map + deque)
                replay.hpp     event stream read/write + apply loop
                gen.hpp        deterministic synthetic market data
src/            book.cpp (stubs), naive_book.cpp, replay.cpp, gen.cpp
src/main.cpp    lob_replay — replay a CSV event file, print trades + top of book
tools/          lob_gen — generate synthetic event files
benchmarks/     lob_bench — per-event latency (p50/p90/p99/p99.9) + throughput
tests/          behavioral suite + property tests (never crossed, FIFO,
                deterministic replay, qty conservation)
```

## Build & run (Windows, MSVC)

```bat
build.bat            :: configure + build with VS-bundled CMake/Ninja
build.bat test       :: + run the test suite
build\lob_gen --out data.csv --orders 1000000 --seed 42
build\lob_replay data.csv --trades
build\lob_bench data.csv --book naive
build\lob_bench --gen 1000000 --book naive --runs 5
```

## Roadmap

- [x] Project scaffold, build plumbing, replay pipeline
- [x] NaiveBook baseline (correct, deliberately unoptimized)
- [x] Benchmark harness: p50/p90/p99/p99.9 + M events/sec
- [x] Property tests: never crossed, FIFO, deterministic, qty conserved
- [ ] `Book` — the fast implementation (arena alloc, O(1) cancel, flat levels)
- [ ] Benchmark report: fast vs naive, honest writeup
- [ ] Stretch: NASDAQ ITCH replay, depth-chart visualization

## Event format (CSV)

```
A,<id>,<B|S>,<L|M>,<G|I|F>,<price>,<qty>   add (limit/market, GTC/IOC/FOK)
C,<id>                                     cancel
M,<id>,<price>,<qty>                       modify (loses queue priority)
```

Prices are integer ticks (10000 = $100.00). Streams are deterministic for a
given seed — replaying the same file must produce the same trades.

## The division of labor

`NaiveBook`, replay, generator, benchmark, and tests are the harness. `Book`
(`include/lob/book.hpp`, `src/book.cpp`) is the portfolio piece: same
observable contract, engineered for latency. Enable `BOOK_SUITE(fast_, ...)`
in `tests/test_book.cpp` and `--book fast` in the CLIs once it exists.
