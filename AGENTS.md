# Agent notes

## Toolchain (not on PATH — bundled inside Visual Studio)

- vcvars: `C:\Program Files\Microsoft Visual Studio\18\Community\VC\Auxiliary\Build\vcvars64.bat`
- cmake 4.1.1: `C:\Program Files\Microsoft Visual Studio\18\Community\Common7\IDE\CommonExtensions\Microsoft\CMake\CMake\bin\cmake.exe`
- ninja: same tree, `...\CMake\Ninja\ninja.exe`
- MinGW g++ 8.1 also exists but MSVC is the primary toolchain.

## Commands

- Build: `build.bat` (calls vcvars, configures Ninja once, then builds)
- Test: `build.bat test` (ctest)
- Bench: `build.bat bench --gen 1000000` or `build\lob_bench.exe data.csv`
- Generate data: `build\lob_gen.exe --out data.csv --orders N --seed S`

## Conventions

- `Book` (include/lob/book.hpp, src/book.cpp) is intentionally unimplemented —
  it's the user's part of the project. Don't fill it in unless asked.
- `NaiveBook` is the correctness reference + benchmark baseline. Keep it simple.
- Tests run the same suite over any book type via `BOOK_SUITE(prefix, Type)`.
- Generated `.csv` data files are gitignored.
