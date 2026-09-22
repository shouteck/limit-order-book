@echo off
REM Build the WASM demo: compiles Book + generator + embind bindings
REM into docs/lob.js + docs/lob.wasm (served by GitHub Pages).
REM Requires emsdk at %USERPROFILE%\emsdk (emsdk install latest && emsdk activate latest).
setlocal
set "PATH=%USERPROFILE%\AppData\Local\Programs\Python\Python312;%PATH%"
call "%USERPROFILE%\emsdk\emsdk_env.bat" >nul
if errorlevel 1 (
    echo emsdk not found at %USERPROFILE%\emsdk
    exit /b 1
)
emcc -O2 -std=c++20 -fexceptions ^
    -Iinclude ^
    src\book.cpp src\gen.cpp wasm\lob_wasm.cpp ^
    -lembind ^
    -s MODULARIZE=1 -s EXPORT_NAME=createLob ^
    -s ALLOW_MEMORY_GROWTH=1 ^
    -o docs\lob.js
if errorlevel 1 exit /b 1
echo Built docs\lob.js + docs\lob.wasm
endlocal
