@echo off
REM ============================================================================
REM build_wasm.bat — Compile the C++ scheduler to WebAssembly (Windows)
REM ============================================================================
REM
REM Prerequisites: Emscripten SDK (emsdk) must be installed and activated.
REM   Install: https://emscripten.org/docs/getting_started/downloads.html
REM   Quick:
REM     git clone https://github.com/emscripten-core/emsdk.git
REM     cd emsdk
REM     emsdk install latest
REM     emsdk activate latest
REM     emsdk_env.bat
REM
REM Run this script from the project root.
REM ============================================================================

echo Compiling C++ scheduler to WebAssembly...

call em++ -std=c++17 -O2 --bind wasm_bridge.cpp -o docs/scheduler.js -s WASM=1 -s MODULARIZE=1 -s EXPORT_NAME="createSchedulerModule" -s ALLOW_MEMORY_GROWTH=1

if %ERRORLEVEL% EQU 0 (
    echo Done! Output files:
    echo   docs\scheduler.js
    echo   docs\scheduler.wasm
) else (
    echo Build failed. Make sure emsdk is installed and activated.
    exit /b 1
)
