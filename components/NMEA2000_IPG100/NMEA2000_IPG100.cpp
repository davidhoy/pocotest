/*
 * NMEA2000_IPG100.cpp
 *
 * NMEA2000 interface for Maretron IPG100 devices using the reverse-engineered protocol
 * 
 * Copyright (c) 2025 David Hoy, david@thehoys.com
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 * 
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 * 
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
 * SOFTWARE.
 */

#include "NMEA2000_IPG100.h"
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <fcntl.h>
#include <errno.h>
#include <string.h>
#include <ifaddrs.h>
#include <net/if.h>
#include <sys/select.h>
#include <iostream>
#include <sstream>
#include <algorithm>
#include <thread>
#include <chrono>
#include <mutex>
#include <queue>

// Static error storage
static std::string g_lastError;

//===================================================================================
// Constructor and Destructor
//===================================================================================

tNMEA2000_IPG100::tNMEA2000_IPG100(const char* ipAddress)
    : tNMEA2000()
    , m_udp_socket(-1)
    , m_tcp_socket(-1)
    , m_ipAddress(ipAddress ? ipAddress : "")
    , m_connected(false)
    , m_discovery_running(false)
    , m_shouldStop(false)
    , m_connectTimeoutMs(5000)
    , m_readTimeoutMs(100)
{
    std::cout << "Creating IPG100 interface for " << (m_ipAddress.empty() ? "auto-discovery" : m_ipAddress) << std::endl;
}

tNMEA2000_IPG100::~tNMEA2000_IPG100()
{
    stopReceiveThread();
    closeAllSockets();
}

//===================================================================================
// NMEA2000 Interface Implementation
//===================================================================================

bool tNMEA2000_IPG100::CANOpen()
{
    std::cout << "Opening IPG100 connection using reverse-engineered protocol..." << std::endl;
    
    closeAllSockets(); // Ensure any existing connections are closed
    
    // Phase 1: Discover IPG100 device (if IP not specified)
    if (m_ipAddress.empty()) {
        if (!discoverIPG100Device()) {
            g_lastError = "Failed to discover IPG100 device on network";
            return false;
        }
        m_ipAddress = m_discovered_ip;
    } else {
        m_discovered_ip = m_ipAddress;
    }
    
    // Phase 2: Establish TCP data connection
    if (!establishDataConnection()) {
        g_lastError = "Failed to establish TCP data connection to " + m_ipAddress;
        return false;
    }
    
    // Start background receive thread
    m_shouldStop = false;
    m_receiveThread = std::thread(&tNMEA2000_IPG100::receiveLoop, this);
    
    m_connected = true;
    std::cout << "IPG100 connection established to " << m_ipAddress << ":" << IPG100_DATA_PORT << std::endl;
    return true;
}

bool tNMEA2000_IPG100::CANGetFrame(unsigned long &id, unsigned char &len, unsigned char *buf)
{
    std::lock_guard<std::mutex> lock(m_frameMutex);
    
    if (m_frameQueue.empty()) {
        return false;
    }
    
    // Get next frame from queue
    std::vector<unsigned char> frame = m_frameQueue.front();
    m_frameQueue.pop();
    
    // Parse IPG100 frame format (based on reverse engineering)
    // Frame format: 0x02 [seq] [seq] [flags] [CAN_ID 4 bytes] [data 0-8 bytes] [checksum]
    if (frame.size() < 8) {
        return false; // Minimum frame size
    }
    
    if (frame[0] != 0x02) {
        return false; // Not a standard frame
    }
    
    // Extract CAN ID (bytes 4-7, little endian)
    id = (unsigned long)frame[4] | 
         ((unsigned long)frame[5] << 8) |
         ((unsigned long)frame[6] << 16) |
         ((unsigned long)frame[7] << 24);
    
    // Extract data length and payload
    if (frame.size() >= 16) { // Has data payload
        len = std::min((int)(frame.size() - 16), 8); // Max 8 bytes for CAN
        memcpy(buf, &frame[8], len);
    } else {
        len = 0;
    }
    
    return true;
}

bool tNMEA2000_IPG100::CANSendFrame(unsigned long id, unsigned char len, const unsigned char *buf, bool wait_sent)
{
    // Suppress unused parameter warnings
    (void)id;
    (void)len;
    (void)buf;
    (void)wait_sent;
    
    // IPG100 is read-only, cannot send frames
    return false;
}

//===================================================================================
// IPG100 Protocol Implementation (Based on Reverse Engineering)
//===================================================================================

bool tNMEA2000_IPG100::discoverIPG100Device()
{
    std::cout << "Starting IPG100 discovery on port " << IPG100_DISCOVERY_PORT << std::endl;
    
    // Create UDP socket for discovery
    m_udp_socket = socket(AF_INET, SOCK_DGRAM, 0);
    if (m_udp_socket < 0) {
        g_lastError = "Failed to create UDP discovery socket: " + std::string(strerror(errno));
        return false;
    }
    
    // Enable broadcast
    int broadcast = 1;
    if (setsockopt(m_udp_socket, SOL_SOCKET, SO_BROADCAST, &broadcast, sizeof(broadcast)) < 0) {
        g_lastError = "Failed to enable broadcast: " + std::string(strerror(errno));
        close(m_udp_socket);
        m_udp_socket = -1;
        return false;
    }
    
    // Bind to discovery port
    struct sockaddr_in addr;
    memset(&addr, 0, sizeof(addr));
    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = INADDR_ANY;
    addr.sin_port = htons(IPG100_DISCOVERY_PORT);
    
    if (bind(m_udp_socket, (struct sockaddr*)&addr, sizeof(addr)) < 0) {
        g_lastError = "Failed to bind UDP socket: " + std::string(strerror(errno));
        close(m_udp_socket);
        m_udp_socket = -1;
        return false;
    }
    
    // Set socket to non-blocking for timeout
    int flags = fcntl(m_udp_socket, F_GETFL, 0);
    fcntl(m_udp_socket, F_SETFL, flags | O_NONBLOCK);
    
    std::cout << "Listening for IPG100 discovery broadcasts..." << std::endl;
    
    // Listen for discovery packets (timeout after 30 seconds)
    auto start_time = std::chrono::steady_clock::now();
    auto timeout = std::chrono::seconds(30);
    
    char buffer[1024];
    struct sockaddr_in from_addr;
    socklen_t from_len = sizeof(from_addr);
    
    while (std::chrono::steady_clock::now() - start_time < timeout) {
        ssize_t received = recvfrom(m_udp_socket, buffer, sizeof(buffer) - 1, 0, 
                                   (struct sockaddr*)&from_addr, &from_len);
        
        if (received > 0) {
            buffer[received] = '\0';
            
            // Check for IPG100 discovery packet
            if (strncmp(buffer, IPG100_DISCOVERY_MSG, strlen(IPG100_DISCOVERY_MSG)) == 0) {
                // Verify this is an IPG100 (34 bytes) vs client response (22 bytes)
                if (received == 34) {
                    char ip_str[INET_ADDRSTRLEN];
                    inet_ntop(AF_INET, &from_addr.sin_addr, ip_str, INET_ADDRSTRLEN);
                    m_discovered_ip = ip_str;
                    
                    std::cout << "Found IPG100 at " << m_discovered_ip 
                              << " (packet size: " << received << " bytes)" << std::endl;
                    
                    // Send discovery response
                    sendDiscoveryResponse();
                    return true;
                }
            }
        }
        
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
    }
    
    g_lastError = "IPG100 discovery timeout - no device found";
    return false;
}

void tNMEA2000_IPG100::sendDiscoveryResponse()
{
    struct sockaddr_in broadcast_addr;
    memset(&broadcast_addr, 0, sizeof(broadcast_addr));
    broadcast_addr.sin_family = AF_INET;
    broadcast_addr.sin_addr.s_addr = INADDR_BROADCAST;
    broadcast_addr.sin_port = htons(IPG100_DISCOVERY_PORT);
    
    // Send client discovery response (22 bytes as observed)
    const char response[] = "IPG, return ping ACK\x00\x00";
    ssize_t sent = sendto(m_udp_socket, response, sizeof(response) - 1, 0, 
                         (struct sockaddr*)&broadcast_addr, sizeof(broadcast_addr));
    
    if (sent > 0) {
        std::cout << "Sent discovery response (" << sent << " bytes)" << std::endl;
    }
}

bool tNMEA2000_IPG100::establishDataConnection()
{
    if (m_discovered_ip.empty()) return false;
    
    // Create TCP socket
    m_tcp_socket = socket(AF_INET, SOCK_STREAM, 0);
    if (m_tcp_socket < 0) {
        g_lastError = "Failed to create TCP socket: " + std::string(strerror(errno));
        return false;
    }
    
    // Connect to IPG100 data port
    struct sockaddr_in server_addr;
    memset(&server_addr, 0, sizeof(server_addr));
    server_addr.sin_family = AF_INET;
    server_addr.sin_port = htons(IPG100_DATA_PORT);
    
    if (inet_pton(AF_INET, m_discovered_ip.c_str(), &server_addr.sin_addr) <= 0) {
        g_lastError = "Invalid IP address: " + m_discovered_ip;
        close(m_tcp_socket);
        m_tcp_socket = -1;
        return false;
    }
    
    std::cout << "Connecting to IPG100 data port " << IPG100_DATA_PORT << "..." << std::endl;
    
    if (connect(m_tcp_socket, (struct sockaddr*)&server_addr, sizeof(server_addr)) < 0) {
        g_lastError = "Failed to connect to data port: " + std::string(strerror(errno));
        close(m_tcp_socket);
        m_tcp_socket = -1;
        return false;
    }
    
    // Send handshake (observed pattern from reverse engineering)
    if (!sendHandshake()) {
        g_lastError = "Failed to send handshake";
        close(m_tcp_socket);
        m_tcp_socket = -1;
        return false;
    }
    
    std::cout << "TCP data connection established" << std::endl;
    return true;
}

bool tNMEA2000_IPG100::sendHandshake()
{
    // Send handshake (observed pattern from N2KAnalyzer traffic analysis)
    const unsigned char handshake[] = {0x00, 0x00, 0x00, 0x01};
    if (send(m_tcp_socket, handshake, sizeof(handshake), 0) < 0) {
        g_lastError = "Failed to send handshake: " + std::string(strerror(errno));
        return false;
    }
    
    std::cout << "Handshake sent" << std::endl;
    return true;
}

void tNMEA2000_IPG100::receiveLoop()
{
    std::cout << "Starting receive loop..." << std::endl;
    
    unsigned char recv_buffer[2048];
    int packet_count = 0;
    
    while (!m_shouldStop && m_connected) {
        ssize_t received = recv(m_tcp_socket, recv_buffer, sizeof(recv_buffer), 0);
        
        if (received <= 0) {
            if (received == 0) {
                std::cout << "Connection closed by IPG100" << std::endl;
            } else if (errno != EAGAIN && errno != EWOULDBLOCK) {
                std::cerr << "Receive error: " << strerror(errno) << std::endl;
            }
            m_connected = false;
            break;
        }
        
        packet_count++;
        if (packet_count <= 5) {
            std::cout << "Received " << received << " bytes (packet " << packet_count << ")" << std::endl;
        }
        
        // Add received data to buffer
        m_receiveBuffer.insert(m_receiveBuffer.end(), recv_buffer, recv_buffer + received);
        
        // Parse frames from buffer
        parseIPG100Frames(m_receiveBuffer);
        
        // Prevent unlimited buffer growth
        if (m_receiveBuffer.size() > 8192) {
            m_receiveBuffer.erase(m_receiveBuffer.begin(), m_receiveBuffer.begin() + 4096);
        }
        
        std::this_thread::sleep_for(std::chrono::milliseconds(1));
    }
    
    std::cout << "Receive loop ended. Received " << packet_count << " packets total." << std::endl;
}

bool tNMEA2000_IPG100::parseIPG100Frames(const std::vector<unsigned char>& data)
{
    // Look for frame start patterns (0x02, 0x26, 0x09, 0x19 from reverse engineering)
    std::lock_guard<std::mutex> lock(m_frameMutex);
    
    for (size_t i = 0; i < data.size(); i++) {
        if (data[i] == 0x02 && i + 15 < data.size()) { // Minimum frame size
            // Extract potential frame
            std::vector<unsigned char> frame(data.begin() + i, data.begin() + i + 16);
            m_frameQueue.push(frame);
            
            // Debug: print first few frames
            static int frame_count = 0;
            if (frame_count < 3) {
                std::cout << "Frame " << frame_count << ": ";
                for (size_t j = 0; j < std::min(frame.size(), (size_t)16); j++) {
                    printf("%02x ", frame[j]);
                }
                std::cout << std::endl;
                frame_count++;
            }
        }
    }
    
    return true;
}

//===================================================================================
// Socket Management
//===================================================================================

void tNMEA2000_IPG100::closeAllSockets()
{
    if (m_udp_socket >= 0) {
        close(m_udp_socket);
        m_udp_socket = -1;
    }
    
    if (m_tcp_socket >= 0) {
        close(m_tcp_socket);
        m_tcp_socket = -1;
    }
    
    m_connected = false;
}

void tNMEA2000_IPG100::stopReceiveThread()
{
    m_shouldStop = true;
    
    if (m_receiveThread.joinable()) {
        m_receiveThread.join();
    }
}

//===================================================================================
// Static Utility Methods
//===================================================================================

std::vector<std::string> tNMEA2000_IPG100::discoverIPG100Devices(int timeoutMs)
{
    (void)timeoutMs; // Suppress unused parameter warning
    
    std::vector<std::string> devices;
    
    // Create temporary discovery instance
    tNMEA2000_IPG100 temp_instance;
    if (temp_instance.discoverIPG100Device()) {
        devices.push_back(temp_instance.getDiscoveredIP());
    }
    
    return devices;
}

bool tNMEA2000_IPG100::isIPG100Available(const char* ipAddress, int timeoutMs)
{
    // Simple TCP connection test to data port
    int test_socket = socket(AF_INET, SOCK_STREAM, 0);
    if (test_socket < 0) return false;
    
    struct sockaddr_in addr;
    memset(&addr, 0, sizeof(addr));
    addr.sin_family = AF_INET;
    addr.sin_port = htons(IPG100_DATA_PORT);
    
    if (inet_pton(AF_INET, ipAddress, &addr.sin_addr) <= 0) {
        close(test_socket);
        return false;
    }
    
    // Set non-blocking for timeout
    int flags = fcntl(test_socket, F_GETFL, 0);
    fcntl(test_socket, F_SETFL, flags | O_NONBLOCK);
    
    bool available = false;
    if (connect(test_socket, (struct sockaddr*)&addr, sizeof(addr)) == 0 || 
        errno == EINPROGRESS) {
        
        fd_set write_fds;
        FD_ZERO(&write_fds);
        FD_SET(test_socket, &write_fds);
        
        struct timeval timeout;
        timeout.tv_sec = timeoutMs / 1000;
        timeout.tv_usec = (timeoutMs % 1000) * 1000;
        
        if (select(test_socket + 1, nullptr, &write_fds, nullptr, &timeout) > 0) {
            int error = 0;
            socklen_t len = sizeof(error);
            if (getsockopt(test_socket, SOL_SOCKET, SO_ERROR, &error, &len) == 0 && error == 0) {
                available = true;
            }
        }
    }
    
    close(test_socket);
    return available;
}

bool tNMEA2000_IPG100::isValidIPAddress(const char* ipAddress)
{
    struct sockaddr_in sa;
    return inet_pton(AF_INET, ipAddress, &(sa.sin_addr)) != 0;
}

std::string tNMEA2000_IPG100::getLastError()
{
    return g_lastError;
}
