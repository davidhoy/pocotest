#!/bin/bash
#
# start_mcba.sh - Bring up Microchip CAN Bus Analyzer as SocketCAN
#

# ---- CONFIGURATION ----
IFACE="can0"          # Name of the SocketCAN interface
BITRATE="250000"      # Bitrate in bits/sec (125000, 250000, 500000, 1000000, etc.)
TERMINATION="120"     # Set to "120" to enable 120Ω termination, or "0" to disable
# -----------------------

echo "[*] Starting Microchip CAN Bus Analyzer on $IFACE at ${BITRATE}bps..."

# Load kernel modules (if not already loaded)
sudo modprobe can
sudo modprobe can_raw
sudo modprobe mcba_usb

# Wait for the USB device to enumerate
sleep 1

# Bring interface down if it already exists
if ip link show "$IFACE" >/dev/null 2>&1; then
    sudo ip link set "$IFACE" down
fi

# Configure bitrate and termination
sudo ip link set "$IFACE" type can bitrate "$BITRATE" termination "$TERMINATION"

# Bring interface up
sudo ip link set "$IFACE" up

# Verify status
ip -details link show "$IFACE"

echo "[+] $IFACE is up and running."
echo "    Use 'candump $IFACE' to monitor traffic."