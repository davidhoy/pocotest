/*
 * NMEA2000 SocketCAN to WebSocket Bridge
 * 
 * This daemon runs on the Raspberry Pi and bridges between:
 * - SocketCAN interface (can0) for NMEA2000 network
 * - WebSocket server for browser-based WASM clients
 * 
 * Usage: ./nmea2000_bridge --can=can0 --port=8080
 */

#include <iostream>
#include <string>
#include <thread>
#include <queue>
#include <mutex>
#include <condition_variable>
#include <chrono>
#include <signal.h>
#include <unistd.h>

// WebSocket++ for WebSocket server
#include <websocketpp/config/asio_no_tls.hpp>
#include <websocketpp/server.hpp>

// SocketCAN includes
#include <sys/socket.h>
#include <sys/ioctl.h>
#include <net/if.h>
#include <linux/can.h>
#include <linux/can/raw.h>

// JSON for message serialization
#include <nlohmann/json.hpp>

using json = nlohmann::json;
typedef websocketpp::server<websocketpp::config::asio> WebSocketServer;

class NMEA2000Bridge {
private:
    std::string can_interface;
    int websocket_port;
    int can_socket;
    WebSocketServer ws_server;
    
    std::thread can_reader_thread;
    std::thread ws_server_thread;
    
    std::queue<can_frame> can_to_ws_queue;
    std::queue<can_frame> ws_to_can_queue;
    std::mutex can_to_ws_mutex;
    std::mutex ws_to_can_mutex;
    std::condition_variable can_to_ws_cv;
    std::condition_variable ws_to_can_cv;
    
    std::set<websocketpp::connection_hdl, std::owner_less<websocketpp::connection_hdl>> connections;
    std::mutex connections_mutex;
    
    volatile bool running;

public:
    NMEA2000Bridge(const std::string& can_if, int ws_port) 
        : can_interface(can_if), websocket_port(ws_port), can_socket(-1), running(true) {}
    
    ~NMEA2000Bridge() {
        stop();
    }
    
    bool initialize() {
        // Initialize SocketCAN
        if (!init_socketcan()) {
            std::cerr << "Failed to initialize SocketCAN interface: " << can_interface << std::endl;
            return false;
        }
        
        // Initialize WebSocket server
        if (!init_websocket()) {
            std::cerr << "Failed to initialize WebSocket server on port: " << websocket_port << std::endl;
            return false;
        }
        
        return true;
    }
    
    void run() {
        std::cout << "Starting NMEA2000 Bridge..." << std::endl;
        std::cout << "CAN Interface: " << can_interface << std::endl;
        std::cout << "WebSocket Port: " << websocket_port << std::endl;
        
        // Start threads
        can_reader_thread = std::thread(&NMEA2000Bridge::can_reader_loop, this);
        ws_server_thread = std::thread(&NMEA2000Bridge::websocket_server_loop, this);
        
        // Main processing loop
        while (running) {
            process_can_to_websocket();
            process_websocket_to_can();
            std::this_thread::sleep_for(std::chrono::milliseconds(1));
        }
        
        // Wait for threads to complete
        if (can_reader_thread.joinable()) can_reader_thread.join();
        if (ws_server_thread.joinable()) ws_server_thread.join();
    }
    
    void stop() {
        running = false;
        ws_server.stop();
        if (can_socket >= 0) {
            close(can_socket);
        }
    }

private:
    bool init_socketcan() {
        can_socket = socket(PF_CAN, SOCK_RAW, CAN_RAW);
        if (can_socket < 0) {
            perror("Error creating CAN socket");
            return false;
        }
        
        struct ifreq ifr;
        strcpy(ifr.ifr_name, can_interface.c_str());
        ioctl(can_socket, SIOCGIFINDEX, &ifr);
        
        struct sockaddr_can addr;
        addr.can_family = AF_CAN;
        addr.can_ifindex = ifr.ifr_ifindex;
        
        if (bind(can_socket, (struct sockaddr *)&addr, sizeof(addr)) < 0) {
            perror("Error binding CAN socket");
            close(can_socket);
            return false;
        }
        
        std::cout << "SocketCAN initialized on " << can_interface << std::endl;
        return true;
    }
    
    bool init_websocket() {
        try {
            ws_server.set_access_channels(websocketpp::log::alevel::all);
            ws_server.clear_access_channels(websocketpp::log::alevel::frame_payload);
            ws_server.init_asio();
            
            ws_server.set_message_handler([this](websocketpp::connection_hdl hdl, WebSocketServer::message_ptr msg) {
                handle_websocket_message(hdl, msg);
            });
            
            ws_server.set_open_handler([this](websocketpp::connection_hdl hdl) {
                std::lock_guard<std::mutex> lock(connections_mutex);
                connections.insert(hdl);
                std::cout << "WebSocket client connected" << std::endl;
            });
            
            ws_server.set_close_handler([this](websocketpp::connection_hdl hdl) {
                std::lock_guard<std::mutex> lock(connections_mutex);
                connections.erase(hdl);
                std::cout << "WebSocket client disconnected" << std::endl;
            });
            
            ws_server.listen(websocket_port);
            ws_server.start_accept();
            
            return true;
        } catch (const std::exception& e) {
            std::cerr << "WebSocket initialization error: " << e.what() << std::endl;
            return false;
        }
    }
    
    void can_reader_loop() {
        while (running) {
            can_frame frame;
            ssize_t nbytes = read(can_socket, &frame, sizeof(can_frame));
            
            if (nbytes == sizeof(can_frame)) {
                std::lock_guard<std::mutex> lock(can_to_ws_mutex);
                can_to_ws_queue.push(frame);
                can_to_ws_cv.notify_one();
            } else if (nbytes < 0 && running) {
                perror("CAN read error");
                std::this_thread::sleep_for(std::chrono::milliseconds(100));
            }
        }
    }
    
    void websocket_server_loop() {
        try {
            ws_server.run();
        } catch (const std::exception& e) {
            std::cerr << "WebSocket server error: " << e.what() << std::endl;
        }
    }
    
    void process_can_to_websocket() {
        std::unique_lock<std::mutex> lock(can_to_ws_mutex);
        can_to_ws_cv.wait_for(lock, std::chrono::milliseconds(10), [this] { 
            return !can_to_ws_queue.empty() || !running; 
        });
        
        while (!can_to_ws_queue.empty() && running) {
            can_frame frame = can_to_ws_queue.front();
            can_to_ws_queue.pop();
            lock.unlock();
            
            // Convert CAN frame to JSON
            json msg = can_frame_to_json(frame);
            std::string json_str = msg.dump();
            
            // Broadcast to all connected WebSocket clients
            std::lock_guard<std::mutex> conn_lock(connections_mutex);
            for (auto hdl : connections) {
                try {
                    ws_server.send(hdl, json_str, websocketpp::frame::opcode::text);
                } catch (const std::exception& e) {
                    std::cerr << "Error sending to WebSocket client: " << e.what() << std::endl;
                }
            }
            
            lock.lock();
        }
    }
    
    void process_websocket_to_can() {
        std::unique_lock<std::mutex> lock(ws_to_can_mutex);
        ws_to_can_cv.wait_for(lock, std::chrono::milliseconds(10), [this] { 
            return !ws_to_can_queue.empty() || !running; 
        });
        
        while (!ws_to_can_queue.empty() && running) {
            can_frame frame = ws_to_can_queue.front();
            ws_to_can_queue.pop();
            lock.unlock();
            
            // Send CAN frame
            ssize_t nbytes = write(can_socket, &frame, sizeof(can_frame));
            if (nbytes != sizeof(can_frame)) {
                perror("CAN write error");
            }
            
            lock.lock();
        }
    }
    
    void handle_websocket_message(websocketpp::connection_hdl hdl, WebSocketServer::message_ptr msg) {
        try {
            json j = json::parse(msg->get_payload());
            can_frame frame = json_to_can_frame(j);
            
            std::lock_guard<std::mutex> lock(ws_to_can_mutex);
            ws_to_can_queue.push(frame);
            ws_to_can_cv.notify_one();
        } catch (const std::exception& e) {
            std::cerr << "Error processing WebSocket message: " << e.what() << std::endl;
        }
    }
    
    json can_frame_to_json(const can_frame& frame) {
        json j;
        j["id"] = frame.can_id;
        j["dlc"] = frame.can_dlc;
        j["data"] = std::vector<uint8_t>(frame.data, frame.data + frame.can_dlc);
        j["timestamp"] = std::chrono::duration_cast<std::chrono::milliseconds>(
            std::chrono::system_clock::now().time_since_epoch()).count();
        return j;
    }
    
    can_frame json_to_can_frame(const json& j) {
        can_frame frame = {};
        frame.can_id = j["id"];
        frame.can_dlc = j["dlc"];
        
        std::vector<uint8_t> data = j["data"];
        std::copy(data.begin(), data.end(), frame.data);
        
        return frame;
    }
};

// Signal handler for graceful shutdown
static NMEA2000Bridge* bridge_instance = nullptr;
void signal_handler(int signal) {
    std::cout << "\nShutting down bridge..." << std::endl;
    if (bridge_instance) {
        bridge_instance->stop();
    }
}

int main(int argc, char* argv[]) {
    std::string can_interface = "can0";
    int websocket_port = 8080;
    
    // Parse command line arguments
    for (int i = 1; i < argc; i++) {
        std::string arg = argv[i];
        if (arg.find("--can=") == 0) {
            can_interface = arg.substr(6);
        } else if (arg.find("--port=") == 0) {
            websocket_port = std::stoi(arg.substr(7));
        } else if (arg == "--help" || arg == "-h") {
            std::cout << "Usage: " << argv[0] << " [--can=interface] [--port=port]" << std::endl;
            std::cout << "  --can=interface  CAN interface (default: can0)" << std::endl;
            std::cout << "  --port=port      WebSocket port (default: 8080)" << std::endl;
            return 0;
        }
    }
    
    // Setup signal handlers
    signal(SIGINT, signal_handler);
    signal(SIGTERM, signal_handler);
    
    // Create and run bridge
    NMEA2000Bridge bridge(can_interface, websocket_port);
    bridge_instance = &bridge;
    
    if (!bridge.initialize()) {
        std::cerr << "Failed to initialize bridge" << std::endl;
        return 1;
    }
    
    bridge.run();
    
    std::cout << "Bridge stopped" << std::endl;
    return 0;
}
