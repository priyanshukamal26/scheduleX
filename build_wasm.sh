#!/bin/bash
# ============================================================================
# build_wasm.sh — Compile the C++ scheduler to WebAssembly
# ============================================================================
#
# Prerequisites: Emscripten SDK (emsdk) must be installed and activated.
#   Install: https://emscripten.org/docs/getting_started/downloads.html
#   Quick:
#     git clone https://github.com/emscripten-core/emsdk.git
#     cd emsdk
#     ./emsdk install latest
#     ./emsdk activate latest
#     source ./emsdk_env.sh
#
# Run this script from the project root (the folder containing this file).
# It produces two files in docs/:
#   scheduler.js    — JavaScript glue code
#   scheduler.wasm  — the compiled WebAssembly binary
# ============================================================================

echo "Compiling C++ scheduler to WebAssembly..."

em++ -std=c++17 -O2 --bind wasm_bridge.cpp -o docs/scheduler.js \
    -s WASM=1 \
    -s MODULARIZE=1 \
    -s EXPORT_NAME="createSchedulerModule" \
    -s ALLOW_MEMORY_GROWTH=1

if [ $? -eq 0 ]; then
    echo "Done! Output files:"
    echo "  docs/scheduler.js"
    echo "  docs/scheduler.wasm"
else
    echo "Build failed. Make sure emsdk is installed and activated."
    exit 1
fi
