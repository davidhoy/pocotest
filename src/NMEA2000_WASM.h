/*
NMEA2000_WASM.h

WebAssembly stub implementation for NMEA2000 SocketCAN interface.
This provides a non-functional but compilable interface for WASM builds.

For actual NMEA2000 communication in WASM, you would need to implement
WebSocket communication to a bridge daemon running on the target system.
*/

#ifndef NMEA2000_WASM_H_
#define NMEA2000_WASM_H_

#include <stdio.h>
#include <NMEA2000.h>
#include <N2kMsg.h>
// Avoid QDebug for WASM due to QScopeGuard template issues

using namespace std;

//-----------------------------------------------------------------------------
class tNMEA2000_WASM : public tNMEA2000
{
protected:
    bool CANSendFrame(unsigned long id, unsigned char len, const unsigned char *buf, bool wait_sent);
    bool CANOpen();
    bool CANGetFrame(unsigned long &id, unsigned char &len, unsigned char *buf);

    bool m_isOpen;
    char* m_CANport;

public:
    tNMEA2000_WASM(char* CANport = nullptr);
    virtual ~tNMEA2000_WASM();
};

//-----------------------------------------------------------------------------
class tWASMStream : public N2kStream {
public:
    tWASMStream() = default;
    virtual ~tWASMStream() = default;
    
    int read() override { return -1; }  // No data available
    int peek() override { return -1; }  // No data available
    size_t write(const uint8_t* data, size_t size) override;
};

// Timing functions for WASM compatibility
void delay(uint32_t ms);
uint32_t millis(void);

#endif /* NMEA2000_WASM_H_ */
