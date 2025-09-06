#!/bin/bash
#
# Quick development shortcuts for NMEA2000 Analyzer
# Source this file to get convenient aliases

# Color codes for output
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
BLUE='\033[0;34m'
NC='\033[0m' # No Color

# Helper function for colored output
dev_echo() {
    echo -e "${GREEN}[DEV]${NC} $1"
}

# Get project root directory
if [ -f "./dev-workflow.sh" ]; then
    PROJECT_ROOT="$(pwd)"
elif [ -f "../dev-workflow.sh" ]; then
    PROJECT_ROOT="$(cd .. && pwd)"
else
    echo -e "${RED}Error: Could not find project root${NC}"
    return 1
fi

# Load configuration if it exists
if [ -f "$PROJECT_ROOT/.dev-config" ]; then
    source "$PROJECT_ROOT/.dev-config"
    dev_echo "Loaded configuration from .dev-config"
fi

# Quick development aliases
alias n2k-native="$PROJECT_ROOT/dev-workflow.sh native"
alias n2k-native-test="$PROJECT_ROOT/dev-workflow.sh native --test"
alias n2k-native-release="$PROJECT_ROOT/dev-workflow.sh native --release --test"

alias n2k-wasm="$PROJECT_ROOT/dev-workflow.sh wasm"
alias n2k-wasm-deploy="$PROJECT_ROOT/dev-workflow.sh wasm --deploy"

alias n2k-deploy="$PROJECT_ROOT/dev-workflow.sh deploy"
alias n2k-test="$PROJECT_ROOT/dev-workflow.sh test"
alias n2k-clean="$PROJECT_ROOT/dev-workflow.sh clean"

# Quick CAN interface management
alias n2k-setup-vcan="sudo modprobe vcan; sudo ip link add dev vcan0 type vcan; sudo ip link set up vcan0"
alias n2k-can-dump="candump vcan0"
alias n2k-can-send="cansend vcan0"

# Quick Pi management
alias n2k-pi-ssh="ssh ${PI_USER:-pi}@${PI_HOST:-raspberrypi.local}"
alias n2k-pi-logs="ssh ${PI_USER:-pi}@${PI_HOST:-raspberrypi.local} 'sudo journalctl -u nmea2000-bridge.service -f'"
alias n2k-pi-status="ssh ${PI_USER:-pi}@${PI_HOST:-raspberrypi.local} 'sudo systemctl status nmea2000-bridge.service'"
alias n2k-pi-restart="ssh ${PI_USER:-pi}@${PI_HOST:-raspberrypi.local} 'sudo systemctl restart nmea2000-bridge.service'"

# Development workflow shortcuts
n2k-dev-cycle() {
    dev_echo "Starting development cycle..."
    n2k-native-test && n2k-wasm-deploy
}

n2k-quick-deploy() {
    dev_echo "Quick deploy (native test + WASM deploy)..."
    $PROJECT_ROOT/dev-workflow.sh native --test && $PROJECT_ROOT/dev-workflow.sh wasm --deploy
}

n2k-full-test() {
    dev_echo "Full test cycle..."
    $PROJECT_ROOT/dev-workflow.sh clean && \
    $PROJECT_ROOT/dev-workflow.sh native --test && \
    $PROJECT_ROOT/dev-workflow.sh wasm && \
    $PROJECT_ROOT/dev-workflow.sh test
}

# Project navigation
n2k-src() {
    cd "$PROJECT_ROOT/src"
}

n2k-wasm-dir() {
    cd "$PROJECT_ROOT/wasm-dev"
}

n2k-bridge() {
    cd "$PROJECT_ROOT/bridge-daemon"
}

n2k-root() {
    cd "$PROJECT_ROOT"
}

# Quick file editing
n2k-edit-config() {
    ${EDITOR:-nano} "$PROJECT_ROOT/.dev-config"
}

n2k-edit-pro() {
    ${EDITOR:-nano} "$PROJECT_ROOT/pocotest.pro"
}

# Development status
n2k-status() {
    echo -e "${BLUE}=== NMEA2000 Analyzer Development Status ===${NC}"
    echo
    
    # Project info
    echo -e "${GREEN}Project:${NC} $(basename "$PROJECT_ROOT")"
    echo -e "${GREEN}Location:${NC} $PROJECT_ROOT"
    echo
    
    # Git status
    if [ -d "$PROJECT_ROOT/.git" ]; then
        cd "$PROJECT_ROOT"
        echo -e "${GREEN}Git Branch:${NC} $(git branch --show-current)"
        echo -e "${GREEN}Git Status:${NC}"
        git status --porcelain | head -5
        [ $(git status --porcelain | wc -l) -gt 5 ] && echo "  ... and $(( $(git status --porcelain | wc -l) - 5 )) more files"
        echo
    fi
    
    # Build status
    echo -e "${GREEN}Build Status:${NC}"
    [ -d "$PROJECT_ROOT/build-native" ] && echo "  ✓ Native build exists" || echo "  ✗ Native build missing"
    [ -d "$PROJECT_ROOT/wasm-deploy" ] && echo "  ✓ WASM build exists" || echo "  ✗ WASM build missing"
    echo
    
    # Pi connectivity
    if [ -n "${PI_HOST}" ]; then
        echo -e "${GREEN}Pi Target:${NC} ${PI_USER:-pi}@${PI_HOST}"
        if ping -c 1 "${PI_HOST}" &>/dev/null; then
            echo "  ✓ Pi is reachable"
            if ssh -o ConnectTimeout=3 "${PI_USER:-pi}@${PI_HOST}" "echo 'SSH OK'" &>/dev/null; then
                echo "  ✓ SSH connection works"
            else
                echo "  ✗ SSH connection failed"
            fi
        else
            echo "  ✗ Pi is not reachable"
        fi
    fi
    echo
    
    # CAN interfaces
    echo -e "${GREEN}CAN Interfaces:${NC}"
    if ip link show can0 &>/dev/null; then
        echo "  ✓ can0 available"
    elif ip link show vcan0 &>/dev/null; then
        echo "  ✓ vcan0 available (virtual)"
    else
        echo "  ✗ No CAN interfaces found"
    fi
}

# Show help
n2k-help() {
    echo -e "${BLUE}=== NMEA2000 Analyzer Development Shortcuts ===${NC}"
    echo
    echo -e "${YELLOW}Build & Run:${NC}"
    echo "  n2k-native              - Build and run native version"
    echo "  n2k-native-test         - Build native with tests"
    echo "  n2k-native-release      - Build native release with tests"
    echo "  n2k-wasm                - Build WASM version"
    echo "  n2k-wasm-deploy         - Build WASM and deploy to Pi"
    echo
    echo -e "${YELLOW}Deployment:${NC}"
    echo "  n2k-deploy              - Deploy to Raspberry Pi"
    echo "  n2k-quick-deploy        - Test native + deploy WASM"
    echo "  n2k-dev-cycle           - Full development cycle"
    echo "  n2k-full-test           - Complete test cycle"
    echo
    echo -e "${YELLOW}Pi Management:${NC}"
    echo "  n2k-pi-ssh              - SSH to Raspberry Pi"
    echo "  n2k-pi-logs             - View bridge daemon logs"
    echo "  n2k-pi-status           - Check bridge daemon status"
    echo "  n2k-pi-restart          - Restart bridge daemon"
    echo
    echo -e "${YELLOW}CAN Tools:${NC}"
    echo "  n2k-setup-vcan          - Setup virtual CAN interface"
    echo "  n2k-can-dump            - Monitor CAN traffic"
    echo "  n2k-can-send <frame>    - Send CAN frame"
    echo
    echo -e "${YELLOW}Navigation:${NC}"
    echo "  n2k-root                - Go to project root"
    echo "  n2k-src                 - Go to source directory"
    echo "  n2k-wasm-dir            - Go to WASM directory"
    echo "  n2k-bridge              - Go to bridge daemon directory"
    echo
    echo -e "${YELLOW}Utilities:${NC}"
    echo "  n2k-status              - Show development status"
    echo "  n2k-clean               - Clean all builds"
    echo "  n2k-test                - Run all tests"
    echo "  n2k-edit-config         - Edit development config"
    echo "  n2k-help                - Show this help"
}

# Initialize
dev_echo "NMEA2000 Analyzer development shortcuts loaded"
dev_echo "Pi target: ${PI_USER:-pi}@${PI_HOST:-raspberrypi.local}"
dev_echo "Type 'n2k-help' for available commands"

# Quick status check
if [ "${SHOW_STATUS_ON_LOAD:-true}" = "true" ]; then
    echo
    n2k-status
fi
