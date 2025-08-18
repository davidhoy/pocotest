#!/usr/bin/env bash
# start_cannelloni_client.sh
# Bring up vcan0 (loading vcan if needed) and start a cannelloni client.
# Usage: ./start_cannelloni_client.sh <server-hostname-or-ip> [port]
# Notes:
#  - vcan interfaces don't use a bitrate, but your physical bus is assumed 250000.
#  - Requires: cannelloni, iproute2, (optionally) can-utils.

set -euo pipefail

PORT="${2:-20000}"
IFACE="vcan0"
ASSUMED_BITRATE="250000"  # informational; vcan ignores bitrate

# --- helpers ---------------------------------------------------------------

err() { echo "ERROR: $*" >&2; exit 1; }
need_cmd() { command -v "$1" >/dev/null 2>&1 || err "Missing '$1'. Install it and retry."; }

# --- args ------------------------------------------------------------------

SERVER="${1:-}"
[ -n "$SERVER" ] || err "No server hostname/IP provided.
Usage: $0 <server-hostname-or-ip> [port]"

# --- prereqs ---------------------------------------------------------------

need_cmd ip
need_cmd sudo
need_cmd cannelloni

# Optionally useful for testing: candump/cansend
if ! command -v candump >/dev/null 2>&1; then
  echo "Tip: 'can-utils' not found; install with: sudo apt-get install -y can-utils" >&2
fi

# --- ensure vcan module ----------------------------------------------------

if ! lsmod | grep -q '^vcan'; then
  echo "[*] Loading vcan kernel module..."
  sudo modprobe vcan
else
  echo "[*] vcan module already loaded."
fi

# --- create & up vcan0 -----------------------------------------------------

if ! ip link show "$IFACE" >/dev/null 2>&1; then
  echo "[*] Creating $IFACE..."
  sudo ip link add dev "$IFACE" type vcan
else
  echo "[*] $IFACE already exists."
fi

# Bring it up (idempotent)
echo "[*] Bringing $IFACE up..."
sudo ip link set "$IFACE" up

# --- info ------------------------------------------------------------------

echo "[*] Note: vcan ignores bitrate, but physical CAN is assumed ${ASSUMED_BITRATE}."
echo "[*] Starting cannelloni client -> ${SERVER}:${PORT} using interface ${IFACE} ..."

# If you want to avoid sudo here, you can set capabilities on cannelloni once:
#   sudo setcap cap_net_raw,cap_net_admin+ep "$(command -v cannelloni)"

# --- run cannelloni client -------------------------------------------------

# Foreground (Ctrl+C to stop). Remove sudo if you've granted capabilities.
sudo cannelloni -C c -p -I "$IFACE" -l "$PORT" -R "$SERVER" &
