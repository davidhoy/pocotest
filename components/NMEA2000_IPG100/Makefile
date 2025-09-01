# Makefile for NMEA2000_IPG100 Component
# Core component implementation

CXX = g++
CXXFLAGS = -std=c++11 -Wall -Wextra -O2 -g
INCLUDES = -I../external/NMEA2000/src -I.

# NMEA2000 library objects
NMEA2000_DIR = ../external/NMEA2000/src
NMEA2000_OBJECTS = NMEA2000.o N2kMsg.o N2kMessages.o N2kTimer.o N2kDeviceList.o

$(NMEA2000_OBJECTS): %.o: $(NMEA2000_DIR)/%.cpp
	$(CXX) $(CXXFLAGS) $(INCLUDES) -c $< -o $@

# Core component object
NMEA2000_IPG100.o: NMEA2000_IPG100.cpp NMEA2000_IPG100.h
	$(CXX) $(CXXFLAGS) $(INCLUDES) -c NMEA2000_IPG100.cpp

# Static library
libNMEA2000_IPG100.a: NMEA2000_IPG100.o $(NMEA2000_OBJECTS)
	ar rcs libNMEA2000_IPG100.a NMEA2000_IPG100.o $(NMEA2000_OBJECTS)

# Integration test
test_integration: test_integration.cpp
	$(CXX) $(CXXFLAGS) -o test_integration test_integration.cpp

# Discovery test
test_discovery: test_discovery.cpp NMEA2000_IPG100.o $(NMEA2000_OBJECTS)
	$(CXX) $(CXXFLAGS) $(INCLUDES) -o test_discovery test_discovery.cpp NMEA2000_IPG100.o $(NMEA2000_OBJECTS) -lpthread

# Simple standalone discovery test
test_simple_discovery: test_simple_discovery.cpp
	$(CXX) $(CXXFLAGS) -o test_simple_discovery test_simple_discovery.cpp

# Clean
clean:
	rm -f *.o libNMEA2000_IPG100.a test_integration test_discovery

# Development tools (in reverse-engineering directory)
dev-tools:
	cd reverse-engineering && $(MAKE)

dev-clean:
	cd reverse-engineering && $(MAKE) clean

# Test targets
test: test_integration
	@echo "Running integration test..."
	./test_integration

discovery: test_discovery
	@echo "Running IPG100 discovery test..."
	./test_discovery

simple-discovery: test_simple_discovery
	@echo "Running simple IPG100 discovery test..."
	./test_simple_discovery

.PHONY: clean dev-tools dev-clean test
