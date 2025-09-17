// Example JavaScript test scripts

// Basic lighting control test
function testBasicLighting() {
    startTest("Basic Lighting Control");
    log("Testing basic zone lighting control...");
    
    try {
        clearLog();
        
        // Send zone 1 on, full brightness
        sendPGN(130561, "00 FF 00 00 64 01 00 FF", 0x0E);
        log("Sent zone 1 ON command");
        
        // Wait for acknowledgment
        if (!waitForPGNFromSource(126208, "0E", 3000)) {
            throw "No acknowledgment received from device";
        }
        log("Received acknowledgment");
        
        // Wait a bit for device to process
        waitMs(500);
        
        // Send zone 1 off
        sendPGN(130561, "00 FF 00 00 00 01 00 FF", 0x0E);
        log("Sent zone 1 OFF command");
        
        if (!waitForPGNFromSource(126208, "0E", 3000)) {
            throw "No acknowledgment for OFF command";
        }
        
        log("SUCCESS: Basic lighting control test passed");
        endTest(true);
        
    } catch (error) {
        log("FAIL: " + error);
        endTest(false);
    }
}

// Device enumeration test
function testDeviceEnumeration() {
    startTest("Device Enumeration");
    log("Testing device discovery and enumeration...");
    
    try {
        clearLog();
        setTestTimeout(30); // 30 second timeout
        
        // Request ISO address claim from all devices
        sendPGN(59904, "FF FF FF FF FF FF FF FF", 255);
        log("Sent ISO address claim request");
        
        // Wait for at least one response
        waitMs(2000);
        
        var deviceCount = getDeviceCount();
        if (deviceCount < 1) {
            throw "No devices found on bus";
        }
        
        log("Found " + deviceCount + " device(s)");
        
        // Check for Lumitec devices specifically
        var devices = getDeviceAddresses();
        var lumitecFound = false;
        
        for (var i = 0; i < devices.length; i++) {
            var manufacturer = getDeviceManufacturer(devices[i]);
            log("Device " + devices[i] + ": " + manufacturer);
            
            if (manufacturer.includes("Lumitec")) {
                lumitecFound = true;
                log("Found Lumitec device at address " + devices[i]);
            }
        }
        
        if (!lumitecFound) {
            throw "No Lumitec devices found";
        }
        
        log("SUCCESS: Device enumeration test passed");
        endTest(true);
        
    } catch (error) {
        log("FAIL: " + error);
        endTest(false);
    }
}

// Comprehensive lighting sequence test
function testLightingSequence() {
    startTest("Comprehensive Lighting Sequence");
    log("Testing complete lighting sequence...");
    
    try {
        clearLog();
        setTestTimeout(60); // 1 minute timeout
        
        var zones = [1, 2, 3, 4]; // Test zones 1-4
        var brightness = [0x00, 0x32, 0x64, 0xFF]; // Off, 20%, 40%, 100%
        
        // Test each zone at different brightness levels
        for (var z = 0; z < zones.length; z++) {
            for (var b = 0; b < brightness.length; b++) {
                var zoneData = "00 FF 00 00 " + 
                             brightness[b].toString(16).padStart(2, '0') + " " +
                             zones[z].toString(16).padStart(2, '0') + " 00 FF";
                
                log("Setting zone " + zones[z] + " to " + 
                    Math.round(brightness[b] * 100 / 255) + "% brightness");
                
                sendPGN(130561, zoneData, 0x0E);
                
                if (!waitForPGNFromSource(126208, "0E", 2000)) {
                    throw "No acknowledgment for zone " + zones[z] + 
                          " brightness " + brightness[b];
                }
                
                waitMs(250); // Brief pause between commands
            }
        }
        
        // Turn all zones off
        log("Turning all zones off...");
        sendPGN(130561, "00 FF 00 00 00 FF 00 FF", 0x0E);
        
        if (!waitForPGNFromSource(126208, "0E", 2000)) {
            throw "No acknowledgment for all zones off";
        }
        
        log("SUCCESS: Lighting sequence test passed");
        endTest(true);
        
    } catch (error) {
        log("FAIL: " + error);
        endTest(false);
    }
}

// Message stress test
function testMessageStress() {
    startTest("Message Stress Test");
    log("Testing rapid message transmission...");
    
    try {
        clearLog();
        setTestTimeout(30);
        
        var messageCount = 50;
        var successCount = 0;
        
        for (var i = 0; i < messageCount; i++) {
            // Alternate between two different commands
            var brightness = (i % 2) ? 0x64 : 0x00;
            var zoneData = "00 FF 00 00 " + 
                         brightness.toString(16).padStart(2, '0') + " 01 00 FF";
            
            sendPGN(130561, zoneData, 0x0E);
            
            // Don't wait for ack on every message to test rapid transmission
            if (i % 10 === 0) {
                if (waitForPGNFromSource(126208, "0E", 1000)) {
                    successCount++;
                }
                log("Progress: " + (i + 1) + "/" + messageCount + " messages sent");
            }
            
            waitMs(50); // 20 messages per second
        }
        
        // Final verification
        waitMs(1000);
        var finalMessageCount = getMessageCount();
        
        log("Sent " + messageCount + " messages");
        log("Received " + finalMessageCount + " total messages");
        log("Acknowledgment rate: " + Math.round(successCount * 10 * 100 / messageCount) + "%");
        
        if (successCount < messageCount / 20) { // At least 5% ack rate
            throw "Too few acknowledgments received";
        }
        
        log("SUCCESS: Message stress test passed");
        endTest(true);
        
    } catch (error) {
        log("FAIL: " + error);
        endTest(false);
    }
}

// Run all tests
function runAllTests() {
    log("=== Starting Complete Test Suite ===");
    
    testDeviceEnumeration();
    waitMs(1000);
    
    testBasicLighting();
    waitMs(1000);
    
    testLightingSequence();
    waitMs(1000);
    
    testMessageStress();
    
    log("=== Test Suite Complete ===");
}