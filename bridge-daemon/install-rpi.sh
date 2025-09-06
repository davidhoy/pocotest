#!/bin/bash
# 
# NMEA2000 Bridge Installation Script for Raspberry Pi
# Sets up CAN interface, installs bridge daemon, and configures services

set -e

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
INSTALL_PREFIX="/usr/local"
CAN_INTERFACE="can0"
CAN_BITRATE="250000"

echo "=== NMEA2000 Bridge Setup for Raspberry Pi ==="

# Check if running as root
if [[ $EUID -eq 0 ]]; then
    echo "Please run this script as a regular user (not root)"
    echo "It will prompt for sudo when needed"
    exit 1
fi

# Update system
echo "Updating system packages..."
sudo apt-get update
sudo apt-get upgrade -y

# Install required packages
echo "Installing required packages..."
sudo apt-get install -y \
    build-essential \
    cmake \
    git \
    can-utils \
    python3-can \
    nginx \
    ufw

# Enable SPI for MCP2515-based CAN HATs
echo "Configuring SPI for CAN HAT..."
if ! grep -q "dtparam=spi=on" /boot/config.txt; then
    echo "dtparam=spi=on" | sudo tee -a /boot/config.txt
fi

# Configure MCP2515 overlay for PiCAN2
if ! grep -q "dtoverlay=mcp2515" /boot/config.txt; then
    echo "dtoverlay=mcp2515-can0,oscillator=16000000,interrupt=25" | sudo tee -a /boot/config.txt
fi

# Setup CAN interface configuration
echo "Setting up CAN interface..."
sudo tee /etc/systemd/system/can-setup.service > /dev/null <<EOF
[Unit]
Description=Setup CAN interface
After=multi-user.target

[Service]
Type=oneshot
ExecStart=/sbin/ip link set ${CAN_INTERFACE} up type can bitrate ${CAN_BITRATE}
ExecStop=/sbin/ip link set ${CAN_INTERFACE} down
RemainAfterExit=yes

[Install]
WantedBy=multi-user.target
EOF

sudo systemctl enable can-setup.service

# Create nmea2000 user
echo "Creating nmea2000 user..."
if ! id "nmea2000" &>/dev/null; then
    sudo useradd -r -s /bin/false -d /var/lib/nmea2000 nmea2000
    sudo mkdir -p /var/lib/nmea2000
    sudo chown nmea2000:nmea2000 /var/lib/nmea2000
fi

# Add nmea2000 user to dialout group for CAN access
sudo usermod -a -G dialout nmea2000

# Build and install bridge daemon
echo "Building NMEA2000 bridge daemon..."
cd "$SCRIPT_DIR"
mkdir -p build
cd build
cmake .. -DCMAKE_INSTALL_PREFIX="$INSTALL_PREFIX"
make -j$(nproc)
sudo make install

# Configure systemd service
echo "Configuring systemd service..."
sudo systemctl daemon-reload
sudo systemctl enable nmea2000-bridge.service

# Setup nginx for serving WASM files
echo "Configuring nginx for WASM serving..."
sudo tee /etc/nginx/sites-available/nmea2000-analyzer > /dev/null <<EOF
server {
    listen 80 default_server;
    listen [::]:80 default_server;
    
    root /var/www/nmea2000-analyzer;
    index index.html;
    
    server_name _;
    
    # WASM MIME type
    location ~* \.wasm$ {
        add_header Content-Type application/wasm;
        add_header Cross-Origin-Embedder-Policy require-corp;
        add_header Cross-Origin-Opener-Policy same-origin;
    }
    
    # JavaScript files
    location ~* \.js$ {
        add_header Cross-Origin-Embedder-Policy require-corp;
        add_header Cross-Origin-Opener-Policy same-origin;
    }
    
    # WebSocket proxy to bridge daemon
    location /nmea2000-ws {
        proxy_pass http://localhost:8080;
        proxy_http_version 1.1;
        proxy_set_header Upgrade \$http_upgrade;
        proxy_set_header Connection "upgrade";
        proxy_set_header Host \$host;
        proxy_set_header X-Real-IP \$remote_addr;
        proxy_set_header X-Forwarded-For \$proxy_add_x_forwarded_for;
        proxy_set_header X-Forwarded-Proto \$scheme;
    }
    
    location / {
        try_files \$uri \$uri/ =404;
    }
}
EOF

# Remove default nginx site
sudo rm -f /etc/nginx/sites-enabled/default
sudo ln -sf /etc/nginx/sites-available/nmea2000-analyzer /etc/nginx/sites-enabled/

# Create web directory
sudo mkdir -p /var/www/nmea2000-analyzer
sudo chown www-data:www-data /var/www/nmea2000-analyzer

# Configure firewall
echo "Configuring firewall..."
sudo ufw --force enable
sudo ufw allow ssh
sudo ufw allow 80/tcp    # HTTP for web interface
sudo ufw allow 8080/tcp  # WebSocket bridge (if direct access needed)

# Test CAN interface (if hardware is connected)
echo "Testing CAN interface setup..."
if sudo ip link show "$CAN_INTERFACE" &>/dev/null; then
    echo "✓ CAN interface $CAN_INTERFACE is available"
else
    echo "⚠ CAN interface $CAN_INTERFACE not found (hardware may not be connected)"
fi

echo ""
echo "=== Setup Complete ==="
echo ""
echo "Next steps:"
echo "1. Reboot the Raspberry Pi to load CAN drivers:"
echo "   sudo reboot"
echo ""
echo "2. After reboot, test CAN interface:"
echo "   sudo systemctl status can-setup.service"
echo "   ip link show $CAN_INTERFACE"
echo "   candump $CAN_INTERFACE"
echo ""
echo "3. Deploy WASM files to /var/www/nmea2000-analyzer/"
echo "   (Copy files from your WASM build output)"
echo ""
echo "4. Start the bridge daemon:"
echo "   sudo systemctl start nmea2000-bridge.service"
echo "   sudo systemctl status nmea2000-bridge.service"
echo ""
echo "5. Access the web interface:"
echo "   http://[raspberry-pi-ip]/"
echo ""
echo "Configuration files:"
echo "- Bridge config: /etc/default/nmea2000-bridge.conf"
echo "- Nginx config: /etc/nginx/sites-available/nmea2000-analyzer"
echo "- CAN setup: /etc/systemd/system/can-setup.service"
