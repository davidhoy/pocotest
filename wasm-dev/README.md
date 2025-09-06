# WASM Development Files

This directory contains all WebAssembly development files for the NMEA2000 Analyzer.

## Contents

- **`NMEA2000_WASM.h/.cpp`** - WASM stub implementation for browser compatibility
- **`wasm_shell.html`** - Custom HTML shell template for the WASM application
- **`build-*wasm*.sh`** - Build scripts for various WASM configurations
- **`setup-wasm.sh`** - Initial WASM environment setup script
- **`README_WASM.md`** - Detailed WASM build instructions
- **`WASM_BUILD_SUCCESS.md`** - Build completion documentation

## Usage

To build the WASM version, run from the project root:

```bash
source ~/emsdk/emsdk_env.sh
~/Qt/6.9.2/wasm_singlethread/bin/qmake -spec wasm-emscripten CONFIG+=wasm pocotest.pro
make -j$(nproc)
```

The build output will be placed in the `wasm-deploy/` directory.
