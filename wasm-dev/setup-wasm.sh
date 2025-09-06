#!/bin/bash

# Quick setup script for WASM development
# This script helps you install Emscripten and test Qt WASM setup

echo "=== Qt WASM Setup Helper ==="
echo ""

# Check current Qt installation
echo "Current Qt installation:"
echo "  Version: $(qmake -query QT_VERSION)"
echo "  Location: $(qmake -query QT_INSTALL_BINS)"
echo ""

# Check if WASM mkspecs exist
if [ -d "/usr/lib/x86_64-linux-gnu/qt5/mkspecs/wasm-emscripten" ]; then
    echo "✅ Qt WASM support detected in system Qt"
else
    echo "❌ No WASM support in system Qt"
fi

# Check Emscripten
if command -v emcc &> /dev/null; then
    echo "✅ Emscripten already installed: $(emcc --version | head -1)"
    echo ""
    echo "You can try building with: ./build-wasm.sh"
else
    echo "❌ Emscripten not found"
    echo ""
    echo "To install Emscripten:"
    echo "1. Clone emsdk:"
    echo "   git clone https://github.com/emscripten-core/emsdk.git ~/emsdk"
    echo ""
    echo "2. Install and activate:"
    echo "   cd ~/emsdk"
    echo "   ./emsdk install latest"
    echo "   ./emsdk activate latest"
    echo ""
    echo "3. Set up environment (add to ~/.bashrc for permanent):"
    echo "   source ~/emsdk/emsdk_env.sh"
    echo ""
    echo "4. Then try building:"
    echo "   ./build-wasm.sh"
    echo ""
    
    read -p "Would you like me to install Emscripten now? (y/n): " -n 1 -r
    echo
    if [[ $REPLY =~ ^[Yy]$ ]]; then
        echo "Installing Emscripten..."
        
        # Clone emsdk if not already present
        if [ ! -d "$HOME/emsdk" ]; then
            echo "Cloning emsdk..."
            git clone https://github.com/emscripten-core/emsdk.git ~/emsdk
        else
            echo "emsdk already exists, updating..."
            cd ~/emsdk && git pull
        fi
        
        # Install and activate
        cd ~/emsdk
        echo "Installing latest Emscripten (this may take a while)..."
        ./emsdk install latest
        
        echo "Activating Emscripten..."
        ./emsdk activate latest
        
        echo ""
        echo "✅ Emscripten installed!"
        echo ""
        echo "To use it, run:"
        echo "  source ~/emsdk/emsdk_env.sh"
        echo ""
        echo "Or add this line to your ~/.bashrc for automatic setup:"
        echo "  echo 'source ~/emsdk/emsdk_env.sh' >> ~/.bashrc"
        echo ""
        echo "Then you can build with: ./build-wasm.sh"
    fi
fi

echo ""
echo "=== Alternative: Manual Qt WASM Test ==="
echo "If you want to test manually:"
echo ""
echo "# Set up environment:"
echo "source ~/emsdk/emsdk_env.sh"
echo ""
echo "# Create test build:"
echo "mkdir -p test-wasm && cd test-wasm"
echo "qmake -spec wasm-emscripten CONFIG+=wasm CONFIG+=release ../pocotest.pro"
echo "make"
echo ""
echo "# Serve the result:"
echo "python3 -m http.server 8080"
echo "# Open: http://localhost:8080/pocotest.html"
