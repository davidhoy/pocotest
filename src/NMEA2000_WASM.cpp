/*
NMEA2000_WASM.cpp

WebAssembly stub implementation for NMEA2000 SocketCAN interface.
This provides a non-functional but compilable interface for WASM builds.

For actual NMEA2000 communication in WASM, you would need to implement
WebSocket communication to a bridge daemon running on the target system.
*/

#include "NMEA2000_WASM.h"
#include <iostream>
#include <cstdio>
#include <string>
#include <iomanip>
#include <chrono>
#include <thread>

//*****************************************************************************
tNMEA2000_WASM::tNMEA2000_WASM(char* CANport) : tNMEA2000(), m_isOpen(false)
{
    static char defaultCANport[] = "wasm0";
    
    if (CANport != nullptr)
        m_CANport = CANport;
    else
        m_CANport = defaultCANport;
    
    std::cout << "WASM NMEA2000 interface created for: " << m_CANport << std::endl;
}

//*****************************************************************************
tNMEA2000_WASM::~tNMEA2000_WASM() {
    if (m_isOpen) {
        std::cout << "WASM NMEA2000 interface closed" << std::endl;
        m_isOpen = false;
    }
}

//*****************************************************************************
bool tNMEA2000_WASM::CANOpen() {
    std::cout << "WASM: Opening CAN interface " << m_CANport << std::endl;
    
    // Simulate successful connection
    m_isOpen = true;
    
    std::cout << "WASM: CAN interface opened successfully (stub implementation)" << std::endl;
    return true;
}

//*****************************************************************************
bool tNMEA2000_WASM::CANSendFrame(unsigned long id, unsigned char len, const unsigned char *buf, bool wait_sent) {
    if (!m_isOpen) {
        return false;
    }
    
    // Log the frame that would be sent (for debugging)
    std::cout << "WASM: Would send CAN frame - ID: 0x" << std::hex << id 
             << " Len: " << std::dec << (int)len << " Data: ";
    for(int i = 0; i < len; i++) {
        std::cout << std::hex << (int)buf[i];
    }
    std::cout << std::endl;
    
    // Simulate successful send
    return true;
}

//*****************************************************************************
bool tNMEA2000_WASM::CANGetFrame(unsigned long &id, unsigned char &len, unsigned char *buf) {
    if (!m_isOpen) {
        return false;
    }
    
    // No frames available in stub implementation
    // In a real WASM implementation, this would check for WebSocket messages
    return false;
}

//*****************************************************************************
size_t tWASMStream::write(const uint8_t* data, size_t size) {
    // For WASM builds, just output to console/debug
    if (data && size > 0) {
        std::string output((const char*)data, size);
        std::cout << "WASM Stream: " << output << std::endl;
    }
    return size;
}

//*****************************************************************************
void delay(const uint32_t ms) {
    // Use standard C++ thread sleep for WASM compatibility
    std::this_thread::sleep_for(std::chrono::milliseconds(ms));
}

//*****************************************************************************
uint32_t millis(void) {
    // Use standard C++ chrono for WASM compatibility
    auto now = std::chrono::steady_clock::now();
    auto duration = now.time_since_epoch();
    auto millis = std::chrono::duration_cast<std::chrono::milliseconds>(duration).count();
    return static_cast<uint32_t>(millis & 0xFFFFFFFF);
}
