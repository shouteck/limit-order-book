# Agent notes

## Toolchain (not on PATH — bundled inside Visual Studio)

- vcvars: `C:\Program Files\Microsoft Visual Studio\18\Community\VC\Auxiliary\Build\vcvars64.bat`
- cmake 4.1.1: `C:\Program Files\Microsoft Visual Studio\18\Community\Common7\IDE\CommonExtensions\Microsoft\CMake\CMake\bin\cmake.exe`
- ninja: same tree, `...\CMake\Ninja\ninja.exe`
- MinGW g++ 8.1 also exists but MSVC is the primary toolchain.

## Commands

- Build: `build.bat` (calls vcvars, configures Ninja once, then builds)
- Test: `build.bat test` (ctest)
- Bench: `build.bat bench --gen 1000000` or `build\lob_bench.exe --gen N --book fast --spread S --cancel C --modify M`
- Generate data: `build\lob_gen.exe --out data.csv --orders N --seed S`
- WASM demo: `build_wasm.bat` (needs emsdk at `%USERPROFILE%\emsdk`;
  emcc is `emsdk\upstream\emscripten\em++.exe` — emsdk_env.bat emits
  unix-style PATH under git-bash, so the bat sets env vars directly).
  emsdk was installed via `py emsdk.py install latest` (system `python`
  is the broken Store alias; needs Python >= 3.10).

## Conventions

- `Book` (include/lob/book.hpp, src/book.cpp) is implemented — pool +
  intrusive FIFO + `id->Node*` index + flat level arrays + occupancy
  bitmap. The user owns its internals; AI assists but doesn't redesign.
- `NaiveBook` is the correctness reference + benchmark baseline. Keep it simple.
- `NaiveBook` is the correctness reference + benchmark baseline. Keep it simple.
- Tests run the same suite over any book type via `BOOK_SUITE(prefix, Type)`.
- Generated `.csv` data files are gitignored.
