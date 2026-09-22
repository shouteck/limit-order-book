@echo off
REM Build the WASM demo: compiles Book + generator + embind bindings
REM into docs/lob.js + docs/lob.wasm (served by GitHub Pages).
REM Requires emsdk at %USERPROFILE%\emsdk (emsdk install latest && emsdk activate latest).
setlocal
set "EMSDK=%USERPROFILE%\emsdk"
set "EMSDK_NODE=%EMSDK%\node\24.19.0_64bit\node.exe"
set "EMSDK_PYTHON=%EMSDK%\python\3.13.3_64bit\python.exe"
set "EMCC=%EMSDK%\upstream\emscripten\em++.exe"
if not exist "%EMCC%" (
    echo emsdk not found at %EMSDK%
    exit /b 1
)
"%EMCC%" -O2 -std=c++20 -fexceptions ^
    -Iinclude ^
    src\book.cpp src\gen.cpp wasm\lob_wasm.cpp ^
    -lembind ^
    -s MODULARIZE=1 -s EXPORT_NAME=createLob ^
    -s ALLOW_MEMORY_GROWTH=1 ^
    -o docs\lob.js
if errorlevel 1 exit /b 1
echo Built docs\lob.js + docs\lob.wasm
endlocal
