# Raspberry Pi Headless Deployment Guide

This guide covers deploying the NMEA2000 Analyzer as a headless WebAssembly application on a Raspberry Pi with a CAN HAT.

## Architecture Overview

```
NMEA2000 Network ↔ CAN HAT ↔ Raspberry Pi ↔ WiFi/Ethernet ↔ Browser (WASM App)
                                    ↑
                            Bridge Daemon
                        (SocketCAN ↔ WebSocket)
```

## Hardware Requirements

### Raspberry Pi Setup
- **Raspberry Pi 4 Model B** (recommended, 2GB+ RAM)
- **MicroSD Card** (32GB+ Class 10)
- **CAN HAT** - one of:
  - PiCAN2 (most tested)
  - PiCAN2 Duo (dual CAN ports)
  - Waveshare RS485/CAN HAT
  - Any MCP2515-based CAN HAT

### Network Requirements
- **WiFi or Ethernet** connection for web access
- **Static IP** recommended for consistent access
- **Firewall** configured to allow HTTP (port 80) and optionally WebSocket (port 8080)

## Software Components

### Bridge Daemon (`nmea2000_bridge`)
- C++ application that bridges SocketCAN ↔ WebSocket
- Runs as systemd service
- Handles CAN frame ↔ JSON conversion
- Supports multiple WebSocket clients

### Web Server (Nginx)
- Serves WASM application files
- Proxies WebSocket connections to bridge daemon
- Handles WASM MIME types and CORS headers

### WASM Application
- Qt6-based NMEA2000 analyzer compiled to WebAssembly
- Connects to bridge via WebSocket
- Full-featured marine network analysis in browser

## Installation Process

### 1. Prepare Raspberry Pi

```bash
# Flash Raspberry Pi OS Lite to SD card
# Enable SSH in boot partition: touch /boot/ssh
# Boot Pi and connect via SSH

# Update system
sudo apt update && sudo apt upgrade -y

# Install git to get the project
sudo apt install -y git
git clone [your-repo-url]
cd pocotest/bridge-daemon
```

### 2. Run Automated Setup

```bash
# Execute the installation script
./install-rpi.sh

# This script will:
# - Install required packages (build tools, CAN utils, nginx)
# - Configure SPI and CAN HAT overlay
# - Create systemd services
# - Set up users and permissions
# - Configure nginx for WASM serving
# - Set up firewall rules
```

### 3. Reboot and Verify

```bash
# Reboot to load CAN drivers
sudo reboot

# After reboot, verify CAN interface
sudo systemctl status can-setup.service
ip link show can0

# Should show: can0: <NOARP,UP,LOWER_UP> mtu 16 qdisc pfifo_fast state UP
```

### 4. Build and Install Bridge Daemon

```bash
cd ~/pocotest/bridge-daemon
mkdir build && cd build
cmake ..
make -j$(nproc)
sudo make install
```

### 5. Deploy WASM Application

```bash
# From your development machine, build WASM version
cd /path/to/pocotest/wasm-dev
./build-wasm.sh

# Deploy to Pi (replace raspberrypi.local with your Pi's IP)
cd ../bridge-daemon
./deploy-to-rpi.sh raspberrypi.local pi
```

### 6. Start Services

```bash
# On the Raspberry Pi
sudo systemctl start nmea2000-bridge.service
sudo systemctl enable nmea2000-bridge.service
sudo systemctl status nmea2000-bridge.service
```

## Testing and Verification

### CAN Interface Test
```bash
# Connect NMEA2000 network and test
candump can0

# Should show CAN frames if network is active
# Example: can0  19F50425   [8]  00 FC 05 FF 00 00 FF FF
```

### WebSocket Bridge Test
```bash
# Test WebSocket connection
curl -i -N \
  -H "Connection: Upgrade" \
  -H "Upgrade: websocket" \
  -H "Sec-WebSocket-Version: 13" \
  -H "Sec-WebSocket-Key: test" \
  http://localhost:8080

# Should return WebSocket handshake response
```

### Web Interface Test
```bash
# Access from browser
http://[raspberry-pi-ip]/

# Should load WASM application
# Check browser console for WebSocket connection
```

## Configuration

### Bridge Daemon Config
Edit `/etc/default/nmea2000-bridge.conf`:

```bash
# Change CAN interface if using dual-port HAT
CAN_INTERFACE=can1

# Change WebSocket port if needed
WEBSOCKET_PORT=8080

# Enable message filtering for performance
ENABLE_FILTERING=true
PGN_FILTER=127250,129025,129026,127505
```

### Nginx Config
Edit `/etc/nginx/sites-available/nmea2000-analyzer` for:
- Custom domain names
- SSL/TLS certificates
- Authentication
- Rate limiting

### CAN Interface Config
Edit `/etc/systemd/system/can-setup.service` for:
- Different CAN interfaces (can1, etc.)
- Custom bit rates
- Error handling options

## Monitoring and Maintenance

### Service Status
```bash
# Check all services
sudo systemctl status can-setup.service
sudo systemctl status nmea2000-bridge.service
sudo systemctl status nginx.service

# View logs
sudo journalctl -u nmea2000-bridge.service -f
sudo journalctl -u nginx.service -f
```

### Performance Monitoring
```bash
# Monitor CAN traffic
candump can0 | wc -l  # Messages per second

# Monitor WebSocket connections
sudo netstat -tulpn | grep :8080

# Monitor system resources
htop
iotop
```

### Network Analysis
```bash
# Check CAN statistics
cat /proc/net/can/stats

# Monitor network interfaces
ip -s link show can0
ip -s link show wlan0
```

## Troubleshooting

### CAN Issues
- **No CAN interface**: Check HAT connection, SPI enabled, correct overlay
- **No CAN traffic**: Verify NMEA2000 network connection, termination, power
- **CAN errors**: Check bit rate (usually 250kbps), wiring, termination

### WebSocket Issues
- **Connection refused**: Check bridge daemon status, firewall
- **Connection drops**: Monitor system resources, check logs
- **No data**: Verify CAN traffic first, check bridge daemon logs

### Web Interface Issues
- **404 errors**: Check nginx config, WASM file deployment
- **WASM load errors**: Verify MIME types, check browser console
- **CORS errors**: Check nginx headers configuration

### Performance Issues
- **High CPU**: Enable message filtering, reduce WebSocket clients
- **Memory usage**: Monitor with `free -h`, consider swap if needed
- **Network congestion**: Use message filtering, monitor bandwidth

## Security Considerations

### Network Security
- Change default passwords
- Use strong WiFi security (WPA3)
- Consider VPN for remote access
- Firewall rules for specific IP ranges

### System Security
- Regular security updates
- Non-root service execution
- Read-only filesystem partitions
- Disable unnecessary services

### Application Security
- Authentication for web interface
- Rate limiting for WebSocket connections
- Input validation in bridge daemon
- Secure WebSocket (WSS) for public networks

## Advanced Configuration

### Multiple CAN Interfaces
For dual-port CAN HATs:

```bash
# /etc/systemd/system/can-setup.service
ExecStart=/bin/bash -c '/sbin/ip link set can0 up type can bitrate 250000; /sbin/ip link set can1 up type can bitrate 250000'
```

### Load Balancing
For high-traffic networks:

```bash
# Multiple bridge instances
systemctl start nmea2000-bridge@can0.service
systemctl start nmea2000-bridge@can1.service
```

### Remote Monitoring
Set up monitoring dashboard:
- Grafana for metrics visualization
- Prometheus for data collection
- Custom alerts for system issues

This headless deployment provides a robust, scalable solution for marine network analysis accessible from any modern web browser.
