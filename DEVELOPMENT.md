# Development Workflow Guide

This guide outlines the recommended development workflow for the NMEA2000 Analyzer, supporting native development with easy Raspberry Pi deployment.

## Quick Setup

### 1. **Initial Setup**
```bash
# Clone and enter project
git clone [your-repo-url]
cd pocotest

# Copy configuration template
cp .dev-config.example .dev-config

# Edit configuration for your Pi
nano .dev-config
```

### 2. **Load Development Shortcuts**
```bash
# Load development aliases and functions
source dev-shortcuts.sh

# Check status
n2k-status
```

### 3. **Development Cycle**
```bash
# Standard development cycle
n2k-native-test    # Develop and test natively
n2k-wasm-deploy    # Deploy to Pi when ready
```

## Development Commands

### **Core Workflow**
| Command | Description |
|---------|-------------|
| `n2k-native` | Build and run native version |
| `n2k-native-test` | Build native with comprehensive tests |
| `n2k-wasm-deploy` | Build WASM and deploy to Pi |
| `n2k-quick-deploy` | Test native + deploy WASM in one command |

### **Testing & Validation**
| Command | Description |
|---------|-------------|
| `n2k-test` | Run all tests (native + Pi connectivity) |
| `n2k-full-test` | Complete test cycle (clean + build + test) |
| `n2k-clean` | Clean all build artifacts |

### **Pi Management**
| Command | Description |
|---------|-------------|
| `n2k-pi-ssh` | SSH to your Raspberry Pi |
| `n2k-pi-logs` | View bridge daemon logs in real-time |
| `n2k-pi-status` | Check bridge daemon status |
| `n2k-pi-restart` | Restart bridge daemon |

### **CAN Interface Tools**
| Command | Description |
|---------|-------------|
| `n2k-setup-vcan` | Setup virtual CAN for development |
| `n2k-can-dump` | Monitor CAN traffic |
| `n2k-can-send <frame>` | Send test CAN frames |

## Recommended Workflow

### **Daily Development**
```bash
# 1. Start development session
source dev-shortcuts.sh
n2k-status

# 2. Develop with immediate feedback
n2k-native-test

# 3. Iterate quickly
# Edit code, then:
n2k-native

# 4. Deploy when feature is ready
n2k-wasm-deploy
```

### **Feature Development**
```bash
# 1. Start with clean build
n2k-clean
n2k-native-test

# 2. Develop feature
# Edit source files...

# 3. Test incrementally
n2k-native

# 4. Full validation before deployment
n2k-full-test

# 5. Deploy to Pi
n2k-deploy
```

### **Testing & Debugging**
```bash
# Native debugging with CAN simulation
n2k-setup-vcan
n2k-native

# Monitor CAN traffic during development
n2k-can-dump

# Send test data
n2k-can-send 18FEF125#0102030405060708

# Check Pi deployment
n2k-pi-status
n2k-pi-logs
```

## Configuration

### **Development Config (`.dev-config`)**
```bash
# Your Pi details
PI_HOST=192.168.1.100
PI_USER=pi

# Development preferences
DEFAULT_BUILD_TYPE=debug
RUN_TESTS_AFTER_BUILD=true
AUTO_DEPLOY_WASM=false

# Enable advanced testing
ENABLE_VALGRIND_TESTS=true
ENABLE_PERFORMANCE_TESTS=true
```

### **Pi Target Management**
The development tools support multiple Pi configurations:

```bash
# Development Pi
PI_HOST=dev-pi.local
PI_USER=pi

# Production Pi  
PI_HOST=192.168.1.100
PI_USER=nmea2000

# Test Pi
PI_HOST=test-analyzer.local
PI_USER=test
```

## IDE Integration

### **VS Code Tasks**
The workflow integrates with VS Code tasks. Add to `.vscode/tasks.json`:

```json
{
    "version": "2.0.0",
    "tasks": [
        {
            "label": "N2K: Native Build & Test",
            "type": "shell",
            "command": "./dev-workflow.sh",
            "args": ["native", "--test"],
            "group": "build",
            "problemMatcher": "$gcc"
        },
        {
            "label": "N2K: Deploy to Pi",
            "type": "shell",
            "command": "./dev-workflow.sh",
            "args": ["wasm", "--deploy"],
            "group": "build"
        }
    ]
}
```

### **Debug Configuration**
For native debugging, the built executable is in `build-native/pocotest`:

```json
{
    "name": "Debug NMEA2000 Analyzer",
    "type": "cppdbg",
    "request": "launch",
    "program": "${workspaceFolder}/build-native/pocotest",
    "cwd": "${workspaceFolder}/build-native",
    "environment": []
}
```

## Advanced Usage

### **Custom Build Configurations**
```bash
# Release build with optimization
./dev-workflow.sh native --release

# WASM build with specific optimization
WASM_OPTIMIZATION_LEVEL="-O3" ./dev-workflow.sh wasm

# Debug build with extra diagnostics
QMAKE_CXXFLAGS_DEBUG+="-fsanitize=address" ./dev-workflow.sh native
```

### **Multiple Pi Targets**
```bash
# Deploy to different Pis
./dev-workflow.sh deploy --pi-host=dev-pi.local
./dev-workflow.sh deploy --pi-host=192.168.1.100
./dev-workflow.sh deploy --pi-host=production-analyzer.local
```

### **Automated Testing**
```bash
# Continuous testing during development
watch -n 30 'n2k-native-test'

# Performance testing
ENABLE_PERFORMANCE_TESTS=true n2k-test

# Memory leak detection
ENABLE_VALGRIND_TESTS=true n2k-native-test
```

## Troubleshooting

### **Common Issues**

**Native build fails:**
```bash
# Check dependencies
./dev-workflow.sh test

# Clean and rebuild
n2k-clean
n2k-native
```

**Pi deployment fails:**
```bash
# Check connectivity
n2k-pi-ssh

# Check Pi status
n2k-pi-status

# Manual deployment
cd bridge-daemon
./deploy-to-rpi.sh your-pi-host pi
```

**CAN interface issues:**
```bash
# Setup virtual CAN for development
n2k-setup-vcan

# Check available interfaces
ip link show | grep can

# Test CAN communication
n2k-can-dump &
n2k-can-send 123#DEADBEEF
```

### **Performance Optimization**

**Faster builds:**
```bash
# Parallel compilation
export MAKEFLAGS="-j$(nproc)"

# ccache for faster rebuilds
sudo apt install ccache
export PATH="/usr/lib/ccache:$PATH"
```

**Efficient development:**
```bash
# Skip tests during rapid iteration
n2k-native

# Auto-deploy after successful native build
./dev-workflow.sh native --deploy
```

This workflow provides a smooth development experience with quick iteration and reliable deployment to your Raspberry Pi target.
