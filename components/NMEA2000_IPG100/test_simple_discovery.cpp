#include <iostream>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <errno.h>

// IPG100 Protocol Constants (from reverse engineering)
const int IPG100_DISCOVERY_PORT = 65499;
const int IPG100_DATA_PORT = 65500;
const char* IPG100_DISCOVERY_MSG = "IPG, return ping ACK";

bool testIPG100Discovery(int timeoutMs = 5000) {
    std::cout << "Testing IPG100 UDP Discovery..." << std::endl;
    
    // Create UDP socket
    int sock = socket(AF_INET, SOCK_DGRAM, 0);
    if (sock < 0) {
        std::cerr << "Failed to create socket: " << strerror(errno) << std::endl;
        return false;
    }
    
    // Set socket to non-blocking
    int flags = fcntl(sock, F_GETFL, 0);
    fcntl(sock, F_SETFL, flags | O_NONBLOCK);
    
    // Enable broadcast
    int broadcast = 1;
    if (setsockopt(sock, SOL_SOCKET, SO_BROADCAST, &broadcast, sizeof(broadcast)) < 0) {
        std::cerr << "Failed to enable broadcast: " << strerror(errno) << std::endl;
        close(sock);
        return false;
    }
    
    // Bind to discovery port
    struct sockaddr_in bind_addr;
    memset(&bind_addr, 0, sizeof(bind_addr));
    bind_addr.sin_family = AF_INET;
    bind_addr.sin_addr.s_addr = INADDR_ANY;
    bind_addr.sin_port = htons(IPG100_DISCOVERY_PORT);
    
    if (bind(sock, (struct sockaddr*)&bind_addr, sizeof(bind_addr)) < 0) {
        std::cerr << "Failed to bind to port " << IPG100_DISCOVERY_PORT << ": " << strerror(errno) << std::endl;
        close(sock);
        return false;
    }
    
    std::cout << "Listening on UDP port " << IPG100_DISCOVERY_PORT << " for " << (timeoutMs/1000) << " seconds..." << std::endl;
    
    // Listen for discovery packets
    fd_set read_fds;
    struct timeval timeout;
    timeout.tv_sec = timeoutMs / 1000;
    timeout.tv_usec = (timeoutMs % 1000) * 1000;
    
    bool found = false;
    time_t start_time = time(nullptr);
    
    while (time(nullptr) - start_time < timeoutMs/1000) {
        FD_ZERO(&read_fds);
        FD_SET(sock, &read_fds);
        
        int result = select(sock + 1, &read_fds, nullptr, nullptr, &timeout);
        if (result > 0 && FD_ISSET(sock, &read_fds)) {
            char buffer[1024];
            struct sockaddr_in sender_addr;
            socklen_t addr_len = sizeof(sender_addr);
            
            ssize_t bytes_received = recvfrom(sock, buffer, sizeof(buffer), 0, 
                                            (struct sockaddr*)&sender_addr, &addr_len);
            
            if (bytes_received > 0) {
                buffer[bytes_received] = '\0';
                char* sender_ip = inet_ntoa(sender_addr.sin_addr);
                
                std::cout << "Received " << bytes_received << " bytes from " << sender_ip 
                         << ":" << ntohs(sender_addr.sin_port) << std::endl;
                std::cout << "Message: " << buffer << std::endl;
                
                // Check if this is an IPG100 discovery packet
                if (strstr(buffer, "IPG") != nullptr) {
                    std::cout << "✓ IPG100 device discovered at " << sender_ip << std::endl;
                    found = true;
                    break;
                }
            }
        } else if (result < 0 && errno != EINTR) {
            std::cerr << "Select error: " << strerror(errno) << std::endl;
            break;
        }
        
        // Update timeout for next iteration
        timeout.tv_sec = 1;
        timeout.tv_usec = 0;
    }
    
    close(sock);
    
    if (!found) {
        std::cout << "✗ No IPG100 devices found" << std::endl;
    }
    
    return found;
}

bool testIPG100DataPort(const char* ipAddress, int timeoutMs = 3000) {
    std::cout << "Testing IPG100 TCP Data Port at " << ipAddress << "..." << std::endl;
    
    int sock = socket(AF_INET, SOCK_STREAM, 0);
    if (sock < 0) {
        std::cerr << "Failed to create TCP socket: " << strerror(errno) << std::endl;
        return false;
    }
    
    // Set non-blocking for timeout
    int flags = fcntl(sock, F_GETFL, 0);
    fcntl(sock, F_SETFL, flags | O_NONBLOCK);
    
    struct sockaddr_in addr;
    memset(&addr, 0, sizeof(addr));
    addr.sin_family = AF_INET;
    addr.sin_port = htons(IPG100_DATA_PORT);
    
    if (inet_pton(AF_INET, ipAddress, &addr.sin_addr) <= 0) {
        std::cerr << "Invalid IP address: " << ipAddress << std::endl;
        close(sock);
        return false;
    }
    
    bool connected = false;
    if (connect(sock, (struct sockaddr*)&addr, sizeof(addr)) == 0 || errno == EINPROGRESS) {
        fd_set write_fds;
        FD_ZERO(&write_fds);
        FD_SET(sock, &write_fds);
        
        struct timeval timeout;
        timeout.tv_sec = timeoutMs / 1000;
        timeout.tv_usec = (timeoutMs % 1000) * 1000;
        
        if (select(sock + 1, nullptr, &write_fds, nullptr, &timeout) > 0) {
            int error = 0;
            socklen_t len = sizeof(error);
            if (getsockopt(sock, SOL_SOCKET, SO_ERROR, &error, &len) == 0 && error == 0) {
                connected = true;
                std::cout << "✓ Successfully connected to data port " << IPG100_DATA_PORT << std::endl;
            }
        }
    }
    
    if (!connected) {
        std::cout << "✗ Failed to connect to data port " << IPG100_DATA_PORT << std::endl;
    }
    
    close(sock);
    return connected;
}

int main() {
    std::cout << "IPG100 Discovery Test (Standalone)" << std::endl;
    std::cout << "===================================" << std::endl;
    std::cout << "Using reverse-engineered protocol constants:" << std::endl;
    std::cout << "  Discovery Port: " << IPG100_DISCOVERY_PORT << std::endl;
    std::cout << "  Data Port: " << IPG100_DATA_PORT << std::endl;
    std::cout << "  Discovery Message: " << IPG100_DISCOVERY_MSG << std::endl;
    std::cout << std::endl;
    
    // Test UDP discovery
    if (testIPG100Discovery(10000)) {
        // If we found a device, test the known IP address
        std::cout << std::endl;
        testIPG100DataPort("192.168.50.208", 3000);
    }
    
    std::cout << std::endl << "Discovery test completed." << std::endl;
    return 0;
}
