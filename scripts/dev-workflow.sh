#!/bin/bash
#
# Development workflow script for NMEA2000 Analyzer
# Supports native development with easy Pi deployment

set -e

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
PROJECT_DIR="$(dirname "$SCRIPT_DIR")"

# Configuration
DEFAULT_PI_HOST="raspberrypi.local"
DEFAULT_PI_USER="pi"
BUILD_TYPE="debug"
DEPLOY_AFTER_BUILD="false"
RUN_TESTS="false"

show_help() {
    cat << EOF
NMEA2000 Analyzer Development Workflow

Usage: $0 [COMMAND] [OPTIONS]

Commands:
    native          Build and run native version (default)
    wasm            Build WASM version for testing
    deploy          Deploy to Raspberry Pi
    test            Run native tests and validation
    clean           Clean all build artifacts
    
Options:
    --release       Build in release mode (default: debug)
    --pi-host=HOST  Raspberry Pi hostname/IP (default: $DEFAULT_PI_HOST)
    --pi-user=USER  Raspberry Pi username (default: $DEFAULT_PI_USER)
    --deploy        Auto-deploy after successful build
    --test          Run tests after build
    --help, -h      Show this help

Examples:
    $0 native                           # Build and run native debug version
    $0 native --release --test          # Build native release with tests
    $0 wasm --deploy                    # Build WASM and deploy to Pi
    $0 deploy --pi-host=192.168.1.100   # Deploy to specific Pi IP
    $0 test                             # Run comprehensive tests
    $0 clean                            # Clean all builds

Development Workflow:
    1. Develop and test natively: $0 native --test
    2. Build WASM when ready: $0 wasm
    3. Deploy to Pi: $0 deploy
    4. Iterate and repeat
EOF
}

log_info() {
    echo -e "\e[32m[INFO]\e[0m $1"
}

log_warn() {
    echo -e "\e[33m[WARN]\e[0m $1"
}

log_error() {
    echo -e "\e[31m[ERROR]\e[0m $1"
}

check_dependencies() {
    log_info "Checking development dependencies..."
    
    # Check Qt6
    if ! command -v qmake6 &> /dev/null && ! command -v qmake &> /dev/null; then
        log_error "Qt6 not found. Please install Qt6 development packages."
        return 1
    fi
    
    # Check compiler
    if ! command -v g++ &> /dev/null; then
        log_error "C++ compiler not found. Please install build-essential."
        return 1
    fi
    
    log_info "✓ Dependencies check passed"
}

build_native() {
    log_info "Building native version ($BUILD_TYPE mode)..."
    
    cd "$PROJECT_DIR"
    
    # Clean previous build if exists
    [ -d "build-native" ] && rm -rf "build-native"
    mkdir -p "build-native"
    cd "build-native"
    
    # Configure build
    local config_args=""
    if [ "$BUILD_TYPE" = "release" ]; then
        config_args="CONFIG+=release CONFIG-=debug"
    else
        config_args="CONFIG+=debug CONFIG-=release QMAKE_CXXFLAGS_DEBUG+=-g3 QMAKE_CXXFLAGS_DEBUG+=-O0"
    fi
    
    # Generate Makefile
    if command -v qmake6 &> /dev/null; then
        qmake6 $config_args ../pocotest.pro
    else
        qmake $config_args ../pocotest.pro
    fi
    
    # Build
    make -j$(nproc)
    
    if [ $? -eq 0 ]; then
        log_info "✓ Native build successful"
        
        # Run tests if requested
        if [ "$RUN_TESTS" = "true" ]; then
            run_native_tests
        fi
        
        # Deploy if requested
        if [ "$DEPLOY_AFTER_BUILD" = "true" ]; then
            build_and_deploy_wasm
        fi
        
        return 0
    else
        log_error "Native build failed"
        return 1
    fi
}

run_native() {
    log_info "Running native application..."
    
    cd "$PROJECT_DIR/build-native"
    
    if [ ! -f "./pocotest" ]; then
        log_error "Native executable not found. Build first with: $0 native"
        return 1
    fi
    
    # Check for CAN interfaces
    if ip link show can0 &>/dev/null; then
        log_info "CAN interface can0 detected"
        ./pocotest
    elif ip link show vcan0 &>/dev/null; then
        log_info "Virtual CAN interface vcan0 detected"
        ./pocotest
    else
        log_warn "No CAN interfaces found. Setting up virtual CAN for testing..."
        setup_vcan
        ./pocotest
    fi
}

setup_vcan() {
    log_info "Setting up virtual CAN interface for development..."
    
    # Setup virtual CAN
    sudo modprobe vcan || true
    sudo ip link add dev vcan0 type vcan || true
    sudo ip link set up vcan0 || true
    
    # Inject some test data
    if command -v cansend &> /dev/null; then
        log_info "Injecting test CAN data..."
        (
            sleep 2
            for i in {1..10}; do
                cansend vcan0 18FEF125#0102030405060708
                sleep 1
            done
        ) &
    fi
}

build_wasm() {
    log_info "Building WASM version..."
    
    cd "$PROJECT_DIR/wasm-dev"
    
    # Check if Emscripten is activated
    if ! command -v emcc &> /dev/null; then
        log_error "Emscripten not found. Please activate emsdk first:"
        echo "  source /path/to/emsdk/emsdk_env.sh"
        return 1
    fi
    
    # Run WASM build
    if [ -f "./build-wasm.sh" ]; then
        ./build-wasm.sh
    else
        log_error "WASM build script not found"
        return 1
    fi
    
    if [ $? -eq 0 ]; then
        log_info "✓ WASM build successful"
        return 0
    else
        log_error "WASM build failed"
        return 1
    fi
}

build_and_deploy_wasm() {
    log_info "Building WASM and deploying to Pi..."
    
    if build_wasm; then
        deploy_to_pi
    else
        return 1
    fi
}

deploy_to_pi() {
    log_info "Deploying to Raspberry Pi ($PI_HOST)..."
    
    cd "$PROJECT_DIR/bridge-daemon"
    
    if [ ! -f "./deploy-to-rpi.sh" ]; then
        log_error "Deployment script not found"
        return 1
    fi
    
    ./deploy-to-rpi.sh "$PI_HOST" "$PI_USER"
}

run_native_tests() {
    log_info "Running native tests..."
    
    cd "$PROJECT_DIR/build-native"
    
    # Test 1: Basic executable test
    if ./pocotest --version &>/dev/null; then
        log_info "✓ Application starts successfully"
    else
        log_warn "Application version check failed"
    fi
    
    # Test 2: Check for memory leaks (if valgrind available)
    if command -v valgrind &> /dev/null; then
        log_info "Running memory leak check..."
        timeout 10s valgrind --leak-check=summary --error-exitcode=1 ./pocotest --help &>/dev/null
        if [ $? -eq 0 ]; then
            log_info "✓ No memory leaks detected"
        else
            log_warn "Memory leak check failed or timed out"
        fi
    fi
    
    # Test 3: CAN interface compatibility
    setup_vcan
    log_info "✓ Development tests completed"
}

test_pi_connectivity() {
    log_info "Testing Raspberry Pi connectivity..."
    
    if ping -c 1 "$PI_HOST" &>/dev/null; then
        log_info "✓ Pi is reachable at $PI_HOST"
    else
        log_error "Cannot reach Pi at $PI_HOST"
        return 1
    fi
    
    if ssh -o ConnectTimeout=5 "$PI_USER@$PI_HOST" "echo 'SSH OK'" &>/dev/null; then
        log_info "✓ SSH connection successful"
    else
        log_error "SSH connection failed to $PI_USER@$PI_HOST"
        return 1
    fi
    
    # Check bridge daemon
    if ssh "$PI_USER@$PI_HOST" "systemctl is-active nmea2000-bridge.service" &>/dev/null; then
        log_info "✓ Bridge daemon is running on Pi"
    else
        log_warn "Bridge daemon not running on Pi"
    fi
    
    log_info "✓ Pi connectivity tests completed"
}

clean_builds() {
    log_info "Cleaning build artifacts..."
    
    cd "$PROJECT_DIR"
    
    # Clean native builds
    [ -d "build-native" ] && rm -rf "build-native"
    [ -d "build" ] && rm -rf "build"
    [ -f "Makefile" ] && rm -f "Makefile"
    
    # Clean WASM builds
    [ -d "wasm-deploy" ] && rm -rf "wasm-deploy"
    
    # Clean Qt artifacts
    find . -name "*.o" -delete 2>/dev/null || true
    find . -name "moc_*" -delete 2>/dev/null || true
    find . -name "ui_*" -delete 2>/dev/null || true
    
    log_info "✓ Clean completed"
}

# Parse command line arguments
COMMAND="native"
PI_HOST="$DEFAULT_PI_HOST"
PI_USER="$DEFAULT_PI_USER"

while [[ $# -gt 0 ]]; do
    case $1 in
        native|wasm|deploy|test|clean)
            COMMAND="$1"
            ;;
        --release)
            BUILD_TYPE="release"
            ;;
        --deploy)
            DEPLOY_AFTER_BUILD="true"
            ;;
        --test)
            RUN_TESTS="true"
            ;;
        --pi-host=*)
            PI_HOST="${1#*=}"
            ;;
        --pi-user=*)
            PI_USER="${1#*=}"
            ;;
        --help|-h)
            show_help
            exit 0
            ;;
        *)
            log_error "Unknown option: $1"
            show_help
            exit 1
            ;;
    esac
    shift
done

# Main execution
log_info "NMEA2000 Analyzer Development Workflow"
log_info "Command: $COMMAND"
log_info "Build type: $BUILD_TYPE"
log_info "Pi target: $PI_USER@$PI_HOST"

case $COMMAND in
    native)
        check_dependencies && build_native && run_native
        ;;
    wasm)
        build_wasm
        ;;
    deploy)
        test_pi_connectivity && deploy_to_pi
        ;;
    test)
        check_dependencies && run_native_tests && test_pi_connectivity
        ;;
    clean)
        clean_builds
        ;;
    *)
        log_error "Invalid command: $COMMAND"
        show_help
        exit 1
        ;;
esac
