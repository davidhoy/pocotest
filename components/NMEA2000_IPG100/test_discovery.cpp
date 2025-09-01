#include <iostream>
#include <vector>
#include <string>
#include "NMEA2000_IPG100.h"

int main() {
    std::cout << "IPG100 Discovery Test" << std::endl;
    std::cout << "===================" << std::endl;
    
    std::cout << "Starting UDP discovery on port 65499..." << std::endl;
    
    // Test the static discovery method
    std::vector<std::string> devices = tNMEA2000_IPG100::discoverIPG100Devices(5000);
    
    if (devices.empty()) {
        std::cout << "No IPG100 devices found" << std::endl;
    } else {
        std::cout << "Found " << devices.size() << " IPG100 device(s):" << std::endl;
        for (size_t i = 0; i < devices.size(); i++) {
            std::cout << "  " << (i+1) << ". " << devices[i] << std::endl;
            
            // Test if the device is available on the data port
            if (tNMEA2000_IPG100::isIPG100Available(devices[i].c_str(), 2000)) {
                std::cout << "     ✓ Data port 65500 accessible" << std::endl;
            } else {
                std::cout << "     ✗ Data port 65500 not accessible" << std::endl;
            }
        }
    }
    
    std::cout << std::endl << "Discovery test completed." << std::endl;
    
    return 0;
}
