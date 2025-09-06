# NMEA2000 SocketCAN to WebSocket Bridge
cmake_minimum_required(VERSION 3.16)
project(nmea2000_bridge)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Find required packages
find_package(PkgConfig REQUIRED)
find_package(Threads REQUIRED)

# WebSocket++
find_path(WEBSOCKETPP_INCLUDE_DIR websocketpp/config/asio_no_tls.hpp)
if(NOT WEBSOCKETPP_INCLUDE_DIR)
    message(STATUS "WebSocket++ not found, downloading...")
    include(FetchContent)
    FetchContent_Declare(
        websocketpp
        GIT_REPOSITORY https://github.com/zaphoyd/websocketpp.git
        GIT_TAG 0.8.2
    )
    FetchContent_MakeAvailable(websocketpp)
    set(WEBSOCKETPP_INCLUDE_DIR ${websocketpp_SOURCE_DIR})
endif()

# nlohmann/json
find_package(nlohmann_json QUIET)
if(NOT nlohmann_json_FOUND)
    message(STATUS "nlohmann/json not found, downloading...")
    include(FetchContent)
    FetchContent_Declare(
        json
        GIT_REPOSITORY https://github.com/nlohmann/json.git
        GIT_TAG v3.11.2
    )
    FetchContent_MakeAvailable(json)
endif()

# ASIO (standalone)
find_path(ASIO_INCLUDE_DIR asio.hpp)
if(NOT ASIO_INCLUDE_DIR)
    message(STATUS "ASIO not found, downloading...")
    include(FetchContent)
    FetchContent_Declare(
        asio
        GIT_REPOSITORY https://github.com/chriskohlhoff/asio.git
        GIT_TAG asio-1-24-0
    )
    FetchContent_MakeAvailable(asio)
    set(ASIO_INCLUDE_DIR ${asio_SOURCE_DIR}/asio/include)
endif()

# Create the bridge executable
add_executable(nmea2000_bridge nmea2000_bridge.cpp)

target_include_directories(nmea2000_bridge PRIVATE 
    ${WEBSOCKETPP_INCLUDE_DIR}
    ${ASIO_INCLUDE_DIR}
)

target_link_libraries(nmea2000_bridge 
    Threads::Threads
    nlohmann_json::nlohmann_json
)

# Compiler definitions
target_compile_definitions(nmea2000_bridge PRIVATE 
    ASIO_STANDALONE
    _WEBSOCKETPP_CPP11_STL_
)

# Install
install(TARGETS nmea2000_bridge DESTINATION bin)

# Create systemd service file
configure_file(
    ${CMAKE_CURRENT_SOURCE_DIR}/nmea2000-bridge.service.in
    ${CMAKE_CURRENT_BINARY_DIR}/nmea2000-bridge.service
    @ONLY
)

install(FILES ${CMAKE_CURRENT_BINARY_DIR}/nmea2000-bridge.service
    DESTINATION /etc/systemd/system
)

# Create configuration file
install(FILES nmea2000-bridge.conf
    DESTINATION /etc/default
)
