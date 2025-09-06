#!/bin/bash
#
# Deploy WASM build to Raspberry Pi
# This script copies the WASM build output to the Pi and configures it

set -e

# Configuration
RPI_HOST="${1:-raspberrypi.local}"
RPI_USER="${2:-pi}"
LOCAL_WASM_DIR="../wasm-deploy"
REMOTE_WEB_DIR="/var/www/nmea2000-analyzer"

echo "=== Deploying NMEA2000 Analyzer WASM to Raspberry Pi ==="
echo "Target: $RPI_USER@$RPI_HOST"
echo "Local WASM dir: $LOCAL_WASM_DIR"
echo "Remote web dir: $REMOTE_WEB_DIR"

# Check if WASM files exist
if [ ! -d "$LOCAL_WASM_DIR" ]; then
    echo "Error: WASM deploy directory not found: $LOCAL_WASM_DIR"
    echo "Please build the WASM version first:"
    echo "  cd ../wasm-dev && ./build-wasm.sh"
    exit 1
fi

# Check if we can reach the Pi
if ! ping -c 1 "$RPI_HOST" &>/dev/null; then
    echo "Error: Cannot reach $RPI_HOST"
    echo "Please check the hostname/IP and network connection"
    exit 1
fi

echo "Copying WASM files to Raspberry Pi..."

# Copy WASM files
rsync -avz --progress "$LOCAL_WASM_DIR/" "$RPI_USER@$RPI_HOST:~/nmea2000-wasm-temp/"

# Install files on the Pi
ssh "$RPI_USER@$RPI_HOST" << 'EOF'
    echo "Installing WASM files on Raspberry Pi..."
    
    # Create web directory if it doesn't exist
    sudo mkdir -p /var/www/nmea2000-analyzer
    
    # Copy files
    sudo cp -r ~/nmea2000-wasm-temp/* /var/www/nmea2000-analyzer/
    
    # Set permissions
    sudo chown -R www-data:www-data /var/www/nmea2000-analyzer
    sudo chmod -R 755 /var/www/nmea2000-analyzer
    
    # Clean up temp directory
    rm -rf ~/nmea2000-wasm-temp
    
    # Restart nginx to ensure configuration is active
    sudo systemctl restart nginx
    
    echo "WASM deployment complete!"
    echo "Web files installed to /var/www/nmea2000-analyzer"
EOF

echo ""
echo "=== Deployment Complete ==="
echo ""
echo "Next steps:"
echo "1. Ensure the bridge daemon is running:"
echo "   ssh $RPI_USER@$RPI_HOST 'sudo systemctl status nmea2000-bridge.service'"
echo ""
echo "2. Check CAN interface:"
echo "   ssh $RPI_USER@$RPI_HOST 'ip link show can0'"
echo ""
echo "3. Test WebSocket bridge:"
echo "   ssh $RPI_USER@$RPI_HOST 'curl -i -N -H \"Connection: Upgrade\" -H \"Upgrade: websocket\" -H \"Sec-WebSocket-Version: 13\" -H \"Sec-WebSocket-Key: test\" http://localhost:8080'"
echo ""
echo "4. Access the web interface:"
echo "   http://$RPI_HOST/"
echo ""
echo "5. Monitor bridge logs:"
echo "   ssh $RPI_USER@$RPI_HOST 'sudo journalctl -u nmea2000-bridge.service -f'"
