/**
 * @file test_poco_api.c
 * @brief Comprehensive test suite for Lumitec Poco CAN API
 */

#include "lumitec_poco_api.h"
#include <stdio.h>
#include <string.h>
#include <assert.h>

// Test result tracking
static int tests_run = 0;
static int tests_passed = 0;

#define TEST_ASSERT(condition, description) do { \
    tests_run++; \
    if (condition) { \
        tests_passed++; \
        printf("✓ %s\n", description); \
    } else { \
        printf("✗ %s\n", description); \
    } \
} while(0)

void test_version(void)
{
    printf("\n=== Version Test ===\n");
    const char* version = lumitec_poco_get_version();
    TEST_ASSERT(version != NULL, "Version string is not NULL");
    TEST_ASSERT(strlen(version) > 0, "Version string is not empty");
    printf("Version: %s\n", version);
}

void test_simple_action(void)
{
    printf("\n=== Simple Action Test ===\n");
    
    lumitec_poco_can_frame_t frame;
    bool result = lumitec_poco_create_simple_action(&frame, 0x0E, 0x10, POCO_ACTION_ON, 1);
    TEST_ASSERT(result, "Simple action creation successful");
    TEST_ASSERT(frame.data_length == 6, "Simple action has correct data length");
    TEST_ASSERT(frame.destination_address == 0x0E, "Destination address set correctly");
    TEST_ASSERT(frame.source_address == 0x10, "Source address set correctly");
    
    // Validate frame content
    TEST_ASSERT(lumitec_poco_is_valid_frame(&frame), "Frame is valid Lumitec message");
    
    uint8_t prop_id;
    result = lumitec_poco_get_proprietary_id(&frame, &prop_id);
    TEST_ASSERT(result, "Proprietary ID extraction successful");
    TEST_ASSERT(prop_id == POCO_PID_EXTSW_SIMPLE_ACTIONS, "Correct proprietary ID");
    
    // Parse the message
    lumitec_poco_simple_action_t action;
    result = lumitec_poco_parse_simple_action(&frame, &action);
    TEST_ASSERT(result, "Simple action parsing successful");
    TEST_ASSERT(action.action_id == POCO_ACTION_ON, "Action ID parsed correctly");
    TEST_ASSERT(action.switch_id == 1, "Switch ID parsed correctly");
    TEST_ASSERT(action.manufacturer_code == LUMITEC_MANUFACTURER_CODE, "Manufacturer code correct");
    TEST_ASSERT(action.industry_code == MARINE_INDUSTRY_CODE, "Industry code correct");
}

void test_custom_hsb(void)
{
    printf("\n=== Custom HSB Test ===\n");
    
    lumitec_poco_can_frame_t frame;
    bool result = lumitec_poco_create_custom_hsb(&frame, 0x0E, 0x10, POCO_ACTION_T2HSB, 2, 128, 255, 200);
    TEST_ASSERT(result, "Custom HSB creation successful");
    TEST_ASSERT(frame.data_length == 8, "Custom HSB has correct data length");
    
    // Validate frame content
    TEST_ASSERT(lumitec_poco_is_valid_frame(&frame), "Frame is valid Lumitec message");
    
    uint8_t prop_id;
    result = lumitec_poco_get_proprietary_id(&frame, &prop_id);
    TEST_ASSERT(result, "Proprietary ID extraction successful");
    TEST_ASSERT(prop_id == POCO_PID_EXTSW_CUSTOM_HSB, "Correct proprietary ID");
    
    // Parse the message
    lumitec_poco_custom_hsb_t hsb;
    result = lumitec_poco_parse_custom_hsb(&frame, &hsb);
    TEST_ASSERT(result, "Custom HSB parsing successful");
    TEST_ASSERT(hsb.action_id == POCO_ACTION_T2HSB, "Action ID parsed correctly");
    TEST_ASSERT(hsb.switch_id == 2, "Switch ID parsed correctly");
    TEST_ASSERT(hsb.hue == 128, "Hue parsed correctly");
    TEST_ASSERT(hsb.saturation == 255, "Saturation parsed correctly");
    TEST_ASSERT(hsb.brightness == 200, "Brightness parsed correctly");
}

void test_state_info(void)
{
    printf("\n=== State Info Test ===\n");
    
    lumitec_poco_can_frame_t frame;
    bool result = lumitec_poco_create_state_info(&frame, 0x10, 3, POCO_SWITCH_STATE_HELD, POCO_SWITCH_TYPE_LATCHING);
    TEST_ASSERT(result, "State info creation successful");
    TEST_ASSERT(frame.data_length == 7, "State info has correct data length");
    TEST_ASSERT(frame.destination_address == 255, "Broadcast destination set correctly");
    
    // Validate frame content
    TEST_ASSERT(lumitec_poco_is_valid_frame(&frame), "Frame is valid Lumitec message");
    
    // Parse the message
    lumitec_poco_state_info_t state_info;
    result = lumitec_poco_parse_state_info(&frame, &state_info);
    TEST_ASSERT(result, "State info parsing successful");
    TEST_ASSERT(state_info.switch_id == 3, "Switch ID parsed correctly");
    TEST_ASSERT(state_info.switch_state == POCO_SWITCH_STATE_HELD, "Switch state parsed correctly");
    TEST_ASSERT(state_info.switch_type == POCO_SWITCH_TYPE_LATCHING, "Switch type parsed correctly");
}

void test_start_pattern(void)
{
    printf("\n=== Start Pattern Test ===\n");
    
    lumitec_poco_can_frame_t frame;
    bool result = lumitec_poco_create_start_pattern(&frame, 0x0E, 0x10, 1, 5);
    TEST_ASSERT(result, "Start pattern creation successful");
    TEST_ASSERT(frame.data_length == 6, "Start pattern has correct data length");
    
    // Validate frame content
    TEST_ASSERT(lumitec_poco_is_valid_frame(&frame), "Frame is valid Lumitec message");
    
    // Parse the message
    lumitec_poco_start_pattern_t pattern;
    result = lumitec_poco_parse_start_pattern(&frame, &pattern);
    TEST_ASSERT(result, "Start pattern parsing successful");
    TEST_ASSERT(pattern.switch_id == 1, "Switch ID parsed correctly");
    TEST_ASSERT(pattern.pattern_id == 5, "Pattern ID parsed correctly");
}

void test_string_conversions(void)
{
    printf("\n=== String Conversion Test ===\n");
    
    const char* action_str = lumitec_poco_action_to_string(POCO_ACTION_ON);
    TEST_ASSERT(strcmp(action_str, "On") == 0, "Action to string conversion correct");
    
    const char* state_str = lumitec_poco_state_to_string(POCO_SWITCH_STATE_PRESSED);
    TEST_ASSERT(strcmp(state_str, "Pressed") == 0, "State to string conversion correct");
    
    const char* type_str = lumitec_poco_type_to_string(POCO_SWITCH_TYPE_MOMENTARY);
    TEST_ASSERT(strcmp(type_str, "Momentary") == 0, "Type to string conversion correct");
}

void test_invalid_inputs(void)
{
    printf("\n=== Invalid Input Test ===\n");
    
    lumitec_poco_can_frame_t frame;
    bool result;
    
    // Test NULL pointer handling
    result = lumitec_poco_create_simple_action(NULL, 0x0E, 0x10, POCO_ACTION_ON, 1);
    TEST_ASSERT(!result, "NULL frame pointer handled correctly");
    
    result = lumitec_poco_is_valid_frame(NULL);
    TEST_ASSERT(!result, "NULL frame validation handled correctly");
    
    // Test invalid frame data
    memset(&frame, 0, sizeof(frame));
    frame.data_length = 2;  // Too short
    result = lumitec_poco_is_valid_frame(&frame);
    TEST_ASSERT(!result, "Short frame rejected correctly");
    
    // Test wrong manufacturer code
    frame.data_length = 6;
    frame.data[0] = 0x00;  // Wrong manufacturer code
    frame.data[1] = 0x00;
    result = lumitec_poco_is_valid_frame(&frame);
    TEST_ASSERT(!result, "Wrong manufacturer code rejected");
}

void test_roundtrip(void)
{
    printf("\n=== Round-trip Test ===\n");
    
    // Test that create->parse->create produces identical results
    lumitec_poco_can_frame_t frame1, frame2;
    lumitec_poco_custom_hsb_t hsb;
    
    // Create original message
    bool result = lumitec_poco_create_custom_hsb(&frame1, 0x0E, 0x10, POCO_ACTION_T2HSB, 1, 100, 150, 200);
    TEST_ASSERT(result, "Original message created");
    
    // Parse it
    result = lumitec_poco_parse_custom_hsb(&frame1, &hsb);
    TEST_ASSERT(result, "Message parsed successfully");
    
    // Create new message from parsed data
    result = lumitec_poco_create_custom_hsb(&frame2, 0x0E, 0x10, 
                                          (lumitec_poco_action_id_t)hsb.action_id, 
                                          hsb.switch_id, hsb.hue, hsb.saturation, hsb.brightness);
    TEST_ASSERT(result, "Recreated message created");
    
    // Compare frames
    TEST_ASSERT(frame1.data_length == frame2.data_length, "Data lengths match");
    TEST_ASSERT(memcmp(frame1.data, frame2.data, frame1.data_length) == 0, "Data content matches");
}

int main(void)
{
    printf("Lumitec Poco CAN API Test Suite\n");
    printf("================================\n");
    
    test_version();
    test_simple_action();
    test_custom_hsb();
    test_state_info();
    test_start_pattern();
    test_string_conversions();
    test_invalid_inputs();
    test_roundtrip();
    
    printf("\n=== Test Results ===\n");
    printf("Tests run: %d\n", tests_run);
    printf("Tests passed: %d\n", tests_passed);
    printf("Tests failed: %d\n", tests_run - tests_passed);
    
    if (tests_passed == tests_run) {
        printf("✓ All tests passed!\n");
        return 0;
    } else {
        printf("✗ Some tests failed!\n");
        return 1;
    }
}
