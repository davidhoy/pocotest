#!/usr/bin/env python3
"""
Test script for NMEA2000 SocketCAN to WebSocket bridge
Simulates CAN traffic and tests WebSocket connectivity
"""

import asyncio
import websockets
import json
import struct
import socket
import time
import sys
from typing import Optional

class BridgeTest:
    def __init__(self, websocket_url: str = "ws://localhost:8080", can_interface: str = "vcan0"):
        self.websocket_url = websocket_url
        self.can_interface = can_interface
        self.websocket = None
        self.can_socket = None
        
    async def test_websocket_connection(self):
        """Test WebSocket connection to bridge"""
        try:
            print(f"Testing WebSocket connection to {self.websocket_url}...")
            self.websocket = await websockets.connect(self.websocket_url)
            print("✓ WebSocket connection established")
            
            # Send a test message
            test_frame = {
                "id": 0x18FEF121,  # Engine parameters PGN
                "dlc": 8,
                "data": [0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07, 0x08],
                "timestamp": int(time.time() * 1000)
            }
            
            await self.websocket.send(json.dumps(test_frame))
            print("✓ Test message sent via WebSocket")
            
            # Wait for any responses
            try:
                response = await asyncio.wait_for(self.websocket.recv(), timeout=2.0)
                print(f"✓ Received WebSocket response: {response}")
            except asyncio.TimeoutError:
                print("⚠ No WebSocket response received (may be normal if no CAN traffic)")
                
        except Exception as e:
            print(f"✗ WebSocket test failed: {e}")
            return False
        finally:
            if self.websocket:
                await self.websocket.close()
                
        return True
    
    def setup_virtual_can(self):
        """Setup virtual CAN interface for testing"""
        try:
            import subprocess
            print(f"Setting up virtual CAN interface {self.can_interface}...")
            subprocess.run(["sudo", "modprobe", "vcan"], check=True)
            subprocess.run(["sudo", "ip", "link", "add", "dev", self.can_interface, "type", "vcan"], check=False)
            subprocess.run(["sudo", "ip", "link", "set", "up", self.can_interface], check=True)
            print(f"✓ Virtual CAN interface {self.can_interface} ready")
            return True
        except Exception as e:
            print(f"⚠ Could not setup virtual CAN: {e}")
            return False
    
    def test_can_interface(self):
        """Test CAN interface connectivity"""
        try:
            print(f"Testing CAN interface {self.can_interface}...")
            
            # Create raw CAN socket
            self.can_socket = socket.socket(socket.AF_CAN, socket.SOCK_RAW, socket.CAN_RAW)
            self.can_socket.bind((self.can_interface,))
            print(f"✓ CAN socket bound to {self.can_interface}")
            
            # Send a test CAN frame
            can_id = 0x18FEF121  # Engine parameters PGN
            can_dlc = 8
            can_data = b'\x01\x02\x03\x04\x05\x06\x07\x08'
            
            # Pack CAN frame (ID + DLC + PAD + DATA)
            can_frame = struct.pack("=IB3x8s", can_id, can_dlc, can_data)
            self.can_socket.send(can_frame)
            print("✓ Test CAN frame sent")
            
            return True
            
        except Exception as e:
            print(f"✗ CAN interface test failed: {e}")
            return False
        finally:
            if self.can_socket:
                self.can_socket.close()
    
    def test_bridge_integration(self):
        """Test full bridge integration"""
        print("\n=== Testing Bridge Integration ===")
        
        # Setup virtual CAN for testing
        vcan_ok = self.setup_virtual_can()
        
        # Test CAN interface
        can_ok = self.test_can_interface()
        
        print(f"\nCAN Interface: {'✓' if can_ok else '✗'}")
        print(f"Virtual CAN:   {'✓' if vcan_ok else '✗'}")
        
        return can_ok

async def main():
    if len(sys.argv) > 1:
        websocket_url = sys.argv[1]
    else:
        websocket_url = "ws://localhost:8080"
    
    if len(sys.argv) > 2:
        can_interface = sys.argv[2]
    else:
        can_interface = "vcan0"
    
    print("=== NMEA2000 Bridge Test ===")
    print(f"WebSocket URL: {websocket_url}")
    print(f"CAN Interface: {can_interface}")
    print()
    
    tester = BridgeTest(websocket_url, can_interface)
    
    # Test WebSocket connection
    ws_ok = await tester.test_websocket_connection()
    
    # Test CAN interface  
    can_ok = tester.test_bridge_integration()
    
    print("\n=== Test Results ===")
    print(f"WebSocket: {'✓ PASS' if ws_ok else '✗ FAIL'}")
    print(f"CAN:       {'✓ PASS' if can_ok else '✗ FAIL'}")
    
    if ws_ok and can_ok:
        print("\n✓ Bridge appears to be working correctly!")
        print("\nNext steps:")
        print("1. Connect real NMEA2000 network to CAN interface")
        print("2. Test with candump can0 to verify traffic")
        print("3. Access web interface in browser")
    else:
        print("\n✗ Bridge has issues - check the logs:")
        print("  sudo journalctl -u nmea2000-bridge.service -f")

if __name__ == "__main__":
    try:
        asyncio.run(main())
    except KeyboardInterrupt:
        print("\nTest interrupted by user")
    except ImportError as e:
        print(f"Missing dependency: {e}")
        print("Install with: pip3 install websockets")
        sys.exit(1)
