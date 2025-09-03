# VS Code Debugging Setup for Pocotest

This project is now configured with VS Code debugging support for the Qt-based NMEA2000 application.

## Prerequisites

Make sure you have the following VS Code extensions installed:
- C/C++ Extension Pack (ms-vscode.cpptools-extension-pack)
- Makefile Tools (ms-vscode.makefile-tools)

## Available Debug Configurations

### 1. Debug pocotest
- **Description**: Builds the project in debug mode and launches with debugger
- **Build**: Automatically builds with debug symbols (`-g3 -O0 -DDEBUG`)
- **Usage**: Press `F5` or use "Run and Debug" → "Debug pocotest"

### 2. Debug pocotest (no build)
- **Description**: Launches debugger without rebuilding (assumes executable is up-to-date)
- **Usage**: Use when you've already built manually and want to debug quickly

### 3. Debug pocotest with IPG100
- **Description**: Builds and debugs with IPG100 support enabled
- **Build**: Uses `CONFIG+=ipg100` flag for IPG100 functionality
- **Usage**: For debugging IPG100-specific features

## Available Build Tasks

### Debug Tasks
- **build-debug**: Configure and build debug version
- **build-debug-ipg100**: Configure and build debug version with IPG100 support

### Release Tasks  
- **build-release**: Configure and build release version

### Utility Tasks
- **clean**: Clean build artifacts
- **qmake**: Run qmake to regenerate Makefile

## Quick Start

1. **Set a breakpoint**: Click in the gutter next to any line number in a `.cpp` file
2. **Start debugging**: Press `F5` or go to "Run and Debug" panel and select "Debug pocotest"
3. **Debug controls**: Use the debug toolbar or keyboard shortcuts:
   - `F5`: Continue
   - `F10`: Step Over  
   - `F11`: Step Into
   - `Shift+F11`: Step Out
   - `Shift+F5`: Stop

## Environment Setup

The debug configuration sets up the Qt environment automatically:
- `QT_QPA_PLATFORM_PLUGIN_PATH`: Points to Qt plugins
- `LD_LIBRARY_PATH`: Includes Qt libraries

## Debugging Features

- **Pretty printing**: GDB pretty-printing enabled for better variable display
- **Source mapping**: Proper source file mapping for stepping through code
- **Exception handling**: Breaks on exceptions for easier debugging
- **Program output**: Console output visible in VS Code debug console

## Build Variants

### Debug Build
- Compiler flags: `-g3 -O0 -DDEBUG`
- Optimizations: Disabled for easier debugging
- Debug symbols: Full debug information included

### Release Build  
- Compiler flags: Standard release optimization
- Optimizations: Enabled for performance
- Debug symbols: Minimal

## Troubleshooting

### Can't Start Debugging
1. Ensure project builds successfully: `Ctrl+Shift+P` → "Tasks: Run Task" → "build-debug"
2. Check that executable exists: `ls -la pocotest`
3. Verify GDB is installed: `gdb --version`

### Breakpoints Not Hit
1. Ensure you're using the debug build (not release)
2. Check that source files match the executable
3. Try rebuilding: `Ctrl+Shift+P` → "Tasks: Run Task" → "clean" then "build-debug"

### Qt Application Issues
1. Ensure X11 forwarding is enabled if using SSH
2. Check that Qt plugins are available
3. Verify DISPLAY environment variable is set

## File Structure

```
.vscode/
├── launch.json          # Debug configurations
├── tasks.json           # Build tasks  
├── c_cpp_properties.json # IntelliSense configuration
├── settings.json        # Project-specific settings
└── extensions.json      # Recommended extensions
```

## Manual Debug Build

If you prefer to build manually:

```bash
# Debug build
qmake CONFIG+=debug CONFIG-=release QMAKE_CXXFLAGS_DEBUG+=-g3 QMAKE_CXXFLAGS_DEBUG+=-O0
make clean && make

# Debug build with IPG100
qmake CONFIG+=debug CONFIG+=ipg100 CONFIG-=release QMAKE_CXXFLAGS_DEBUG+=-g3 QMAKE_CXXFLAGS_DEBUG+=-O0  
make clean && make

# Release build
qmake CONFIG+=release CONFIG-=debug
make clean && make
```
