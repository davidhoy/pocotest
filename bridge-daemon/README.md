# NMEA2000 SocketCAN to WebSocket Bridge

A C++ daemon that bridges NMEA2000 CAN traffic to WebSocket clients, enabling browser-based marine network analysis tools.

## Quick Start

### Raspberry Pi Deployment
```bash
# Clone repository
git clone [your-repo-url]
cd pocotest/bridge-daemon

# Run automated setup
./install-rpi.sh

# Reboot to load CAN drivers
sudo reboot

# Deploy WASM application (from development machine)
./deploy-to-rpi.sh raspberrypi.local pi

# Start bridge service
sudo systemctl start nmea2000-bridge.service
```

### Manual Build
```bash
# Install dependencies
sudo apt install build-essential cmake nlohmann-json3-dev

# Build
mkdir build && cd build
cmake ..
make -j$(nproc)
sudo make install
```

## Architecture

```
NMEA2000 Network → CAN HAT → SocketCAN → Bridge Daemon → WebSocket → Browser App
```

## Features

- **Real-time bidirectional** CAN ↔ WebSocket bridging
- **Multiple client support** with concurrent WebSocket connections
- **JSON message format** for easy JavaScript integration
- **Configurable filtering** to reduce bandwidth for low-power deployments
- **Systemd integration** for reliable service management
- **Security hardening** with non-root execution and sandboxing

## Configuration

### Bridge Settings (`/etc/default/nmea2000-bridge.conf`)
```bash
CAN_INTERFACE=can0          # CAN interface to use
WEBSOCKET_PORT=8080         # WebSocket server port
ENABLE_FILTERING=false      # Enable PGN filtering
PGN_FILTER=127250,129025    # Allowed PGNs (if filtering enabled)
```

### Service Management
```bash
# Start/stop/restart
sudo systemctl {start|stop|restart} nmea2000-bridge.service

# Enable/disable autostart
sudo systemctl {enable|disable} nmea2000-bridge.service

# View logs
sudo journalctl -u nmea2000-bridge.service -f
```

## Testing

### Basic Connectivity
```bash
# Test WebSocket connection
./test-bridge.py ws://localhost:8080

# Test CAN interface
candump can0

# Check service status
sudo systemctl status nmea2000-bridge.service
```

### Performance Testing
```bash
# Monitor CAN traffic rate
candump can0 | pv -l > /dev/null

# Monitor WebSocket connections
sudo netstat -tulpn | grep :8080

# System resource usage
htop
```

## Message Format

### CAN Frame → WebSocket (JSON)
```json
{
  "id": 419363105,
  "dlc": 8,
  "data": [1, 120, 13, 1, 171, 145, 53, 119],
  "timestamp": 1694856789123
}
```

### WebSocket → CAN Frame
Send the same JSON format to inject frames into the CAN bus.

## Troubleshooting

### Common Issues

**Bridge won't start:**
```bash
# Check CAN interface
ip link show can0

# Check permissions
sudo systemctl status nmea2000-bridge.service
sudo journalctl -u nmea2000-bridge.service
```

**No WebSocket connections:**
```bash
# Check firewall
sudo ufw status
sudo ufw allow 8080/tcp

# Check binding
sudo netstat -tulpn | grep 8080
```

**No CAN traffic:**
```bash
# Verify NMEA2000 network
candump can0

# Check termination and wiring
# NMEA2000 requires 120Ω termination at both ends
```

### Debug Mode
```bash
# Run bridge manually with verbose output
sudo -u nmea2000 /usr/local/bin/nmea2000_bridge --can=can0 --port=8080 --verbose
```

## Hardware Compatibility

### Tested CAN HATs
- ✅ **PiCAN2** - Most reliable, dual-port available
- ✅ **PiCAN2 Duo** - Dual CAN interfaces
- ✅ **Waveshare RS485/CAN** - Budget option
- ⚠️ **Generic MCP2515** - May require voltage level adjustment

### Wiring Requirements
- **CAN_H/CAN_L** properly connected to NMEA2000 backbone
- **120Ω termination** at both ends of network
- **12V power** to NMEA2000 network (separate from Pi)
- **Shield/ground** connection for EMI protection

## Security

### Production Deployment
```bash
# Change default ports
sed -i 's/8080/9999/' /etc/default/nmea2000-bridge.conf

# Enable firewall with specific IPs
sudo ufw allow from 192.168.1.0/24 to any port 80
sudo ufw allow from 192.168.1.0/24 to any port 9999

# Use HTTPS with SSL certificates
# Configure nginx with SSL termination
```

### Access Control
- Bridge runs as dedicated `nmea2000` user
- Nginx serves WASM files with proper CORS headers
- WebSocket connections can be filtered by origin
- Consider VPN for remote access

## Development

### Building from Source
```bash
git clone [repo-url]
cd bridge-daemon

# Install build dependencies
sudo apt install build-essential cmake git

# Fetch dependencies automatically
mkdir build && cd build
cmake ..
make -j$(nproc)
```

### Dependencies
- **C++17** compiler (GCC 7+ or Clang 5+)
- **CMake 3.16+** for build system
- **WebSocket++** for WebSocket server
- **nlohmann/json** for JSON serialization
- **ASIO** for async I/O (header-only)

### Contributing
1. Fork the repository
2. Create feature branch
3. Add tests for new functionality
4. Ensure all tests pass
5. Submit pull request

## License

[Specify your license here]

## Support

- **Issues**: [GitHub Issues URL]
- **Documentation**: [Wiki/Docs URL]
- **Community**: [Forum/Discord URL]
