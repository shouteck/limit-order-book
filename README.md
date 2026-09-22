# lob — limit order book / matching engine

A price-time-priority matching engine in modern C++, built as a portfolio
project for quant-dev / backend roles. Exchange infrastructure, not a trading
bot: deterministic event replay, property tests, and honest latency numbers.

Two implementations share one observable contract:

- **`NaiveBook`** — the correctness oracle: `std::map` price levels +
  `std::deque` FIFO per level. Simple, obviously right, deliberately slow.
- **`Book`** — the engineered version: order pool, intrusive FIFO lists,
  direct `id → Node*` index, flat price-indexed levels, occupancy bitmap.

Every test runs against both; a differential test asserts identical trades
on a 50k-event generated tape.

## Fast-book internals (`Book`)

```
pool_    vector<Node>          one allocation at ctor; alloc/release are
free_    vector<uint32_t>      index-stack pops — zero hot-path malloc,
                               bounded and constant-cost

Level    {head, tail}          intrusive FIFO — orders carry their own
Node     {o, prev, next, lvl}  prev/next links; unlink = 4 pointer writes.
                               lvl backpointer -> cancel needs no lookup
                               for the level

index_   id -> Node*           O(1) cancel: hash -> unlink -> release.
                               No deque scan, no locator indirection

bids_lv_/asks_lv_  vector<Level>   flat arrays indexed by (price - LO);
                                   price->level = one subtraction

occ_bid_/occ_ask_  uint64_t[]      occupancy bitmap, 1 bit per price;
                                   maintained at level birth/death.
                                   best/next occupied level = word scan
                                   + countr_zero/countl_zero
```

Hot path has no `malloc`, no tree descent, no `std::function` (trade
callbacks are templated end-to-end; an empty sink optimizes to nothing).

## Benchmarks (300k events, MSVC Release, best of 3 runs)

Workload is tunable: `--spread` sets the resting-band half-width around mid,
`--cancel`/`--modify` set churn ratios. Latency in nanoseconds.

| workload                | naive      | fast       | notes |
|-------------------------|-----------:|-----------:|-------|
| dense (spread 100)      | 4.13M ev/s | 3.38M ev/s | tiny book, map tree stays cache-hot — naive legitimately wins |
| sparse (spread 4000)    | 1.50M ev/s | 2.17M ev/s | fast +45% |
| very sparse (15000)     | 0.53M ev/s | 2.23M ev/s | fast 4.2x; cancel p99.9: 216us -> 1.7us (~127x) |
| churn (cancel .45)      | 3.51M ev/s | ~same      | book stays shallow; nothing to optimize |

Honest read: the fast design scales with **book depth and domain width** —
the regime that matters on real venues. On a tiny dense book the cached tree
is competitive. `p50`/`p99`/`p99.9` are reported per event kind
(add/cancel/modify); averages alone would hide the tail, which is where the
naive structure's deque scans and allocator calls actually live.

## Design decisions (and their costs)

- **Fixed pool capacity** (default 1M orders). `alloc` throws on exhaustion —
  the pool can never grow: `vector` reallocation would invalidate every
  `Node*` in `index_` and every link. Bounded capacity is the price of
  stable addresses; venues make the same trade (size to max open interest).
- **Fixed price domain** `[LO, LO + NPRICES)` (65,536 ticks). `lvl_at`
  throws out-of-range. Resting orders need a slot; aggressive prices are
  only matching bounds, so an out-of-range limit can still *trade* but
  its leftover can't *rest*.
- **Bitmap desync risk** is the new invariant to maintain:
  `bit[i] == (level[i].head != nullptr)`. Set on empty->live (`rest`),
  cleared on live->empty (`cancel`, `match_into` drain). Transitions only —
  busy-level ops never touch it.
- **`modify` = cancel + re-add** — loses queue priority (exchange-standard),
  and fills triggered by the re-add execute but are not reported (same in
  both books; a real callback could be threaded through if desired).
- **Single-threaded.** No locking anywhere — by design.

## Build & run (Windows, MSVC)

```bat
build.bat            :: configure + build with VS-bundled CMake/Ninja
build.bat test       :: + run the test suite (27 tests)
build\lob_gen --out data.csv --orders 1000000 --seed 42
build\lob_replay data.csv --trades
build\lob_bench --gen 300000 --book fast --runs 3 --spread 15000
```

## Event format (CSV)

```
A,<id>,<B|S>,<L|M>,<G|I|F>,<price>,<qty>   add (limit/market, GTC/IOC/FOK)
C,<id>                                     cancel
M,<id>,<price>,<qty>                       modify (loses queue priority)
```

Prices are integer ticks (10000 = $100.00). Streams are deterministic for a
given seed — replaying the same file must produce the same trades.

## Testing

- **Behavioral suite** — price-time priority, FIFO, IOC/FOK/market
  semantics, cancel/modify, edge cases. Runs over both books.
- **Property tests** — never crossed book, FIFO preserved, deterministic
  replay, quantity conservation (`added == 2*traded + resting`),
  no zero-qty resting orders.
- **Differential test** — identical trade stream from both books on a
  50k-event tape; `NaiveBook` is the oracle.

## Layout

```
include/lob/    types.hpp      core typedefs + Order/Trade/Event
                book.hpp       Book — the fast implementation
                naive_book.hpp NaiveBook — correct baseline
                replay.hpp     event stream read/write + apply loop
                gen.hpp        deterministic synthetic market data
src/            book.cpp, naive_book.cpp, replay.cpp, gen.cpp
src/main.cpp    lob_replay — replay a CSV event file, print trades + top of book
tools/          lob_gen — generate synthetic event files
benchmarks/     lob_bench — per-event latency (p50/p90/p99/p99.9) + throughput
tests/          behavioral + property + differential suites
```

## Docs

- `GUIDE.md` / `GUIDE.pdf` — roadmap, syllabus, commands (regenerate: `python tools/make_guide.py`)
- `THEORY.tex` — per-milestone theory notes; compile with `pdflatex THEORY.tex` or Overleaf
- `human_notes.txt` — personal concept notes

## Roadmap

- [x] Project scaffold, build plumbing, replay pipeline
- [x] NaiveBook baseline (correct, deliberately unoptimized)
- [x] Benchmark harness: p50/p90/p99/p99.9 + M events/sec
- [x] Property tests + differential testing
- [x] `Book`: pool + intrusive FIFO + direct index (M2a)
- [x] `Book`: flat price-indexed levels (M2c) + occupancy bitmap (M2d)
- [x] Benchmark report with honest regime analysis
- [ ] Stretch: two-level bitmap (TLSF-style — restores O(1) best-level
      query asymptotically), single shared level array (no-crossed-book
      means a price hosts at most one side), NASDAQ ITCH replay,
      depth-chart visualization

## Attribution

Project direction, design decisions, and the `Book` internals (pool,
intrusive lists, flat levels, bitmap) were implemented by me with AI
(Devin) mentoring — the harness (NaiveBook, replay, generator, benchmark,
tests) and documentation were AI-assisted. The point of the exercise was
to own the theory: every data structure in `Book` was implemented by hand
and defended in code review-style Q&A.
