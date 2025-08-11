/**
 * @file example_usage.c
 * @brief Example usage of the Lumitec Poco CAN API
 * 
 * This example demonstrates how to use the Lumitec Poco API to create
 * and parse CAN messages for controlling Lumitec lighting systems.
 */

#include "lumitec_poco_api.h"
#include <stdio.h>
#include <string.h>

// Example function to print a CAN frame in hex format
void print_can_frame(const lumitec_poco_can_frame_t* frame)
{
    printf("CAN ID: 0x%08X, DLC: %d, Data: ", frame->can_id, frame->data_length);
    for (int i = 0; i < frame->data_length; i++) {
        printf("%02X ", frame->data[i]);
    }
    printf("\n");
}

int main(void)
{
    printf("Lumitec Poco CAN API Example\n");
    printf("API Version: %s\n\n", lumitec_poco_get_version());
    
    // Example 1: Create a simple action message
    printf("=== Example 1: Simple Action Message ===\n");
    lumitec_poco_can_frame_t frame;
    
    if (lumitec_poco_create_simple_action(&frame, 0x0E, 0x10, POCO_ACTION_ON, 1)) {
        printf("Created simple action message (Turn On, Switch 1):\n");
        print_can_frame(&frame);
        
        // Parse the message back
        lumitec_poco_simple_action_t action;
        if (lumitec_poco_parse_simple_action(&frame, &action)) {
            printf("Parsed: Action=%s, Switch=%d\n",
                   lumitec_poco_action_to_string(action.action_id),
                   action.switch_id);
        }
    }
    printf("\n");
    
    // Example 2: Create a custom HSB message
    printf("=== Example 2: Custom HSB Message ===\n");
    if (lumitec_poco_create_custom_hsb(&frame, 0x0E, 0x10, POCO_ACTION_T2HSB, 1, 128, 255, 200)) {
        printf("Created custom HSB message (Hue=128, Sat=255, Bright=200):\n");
        print_can_frame(&frame);
        
        // Parse the message back
        lumitec_poco_custom_hsb_t hsb;
        if (lumitec_poco_parse_custom_hsb(&frame, &hsb)) {
            printf("Parsed: Action=%s, Switch=%d, H=%d, S=%d, B=%d\n",
                   lumitec_poco_action_to_string(hsb.action_id),
                   hsb.switch_id, hsb.hue, hsb.saturation, hsb.brightness);
        }
    }
    printf("\n");
    
    // Example 3: Create a state information message
    printf("=== Example 3: State Information Message ===\n");
    if (lumitec_poco_create_state_info(&frame, 0x10, 2, POCO_SWITCH_STATE_PRESSED, POCO_SWITCH_TYPE_MOMENTARY)) {
        printf("Created state info message (Switch 2 pressed, momentary):\n");
        print_can_frame(&frame);
        
        // Parse the message back
        lumitec_poco_state_info_t state_info;
        if (lumitec_poco_parse_state_info(&frame, &state_info)) {
            printf("Parsed: Switch=%d, State=%s, Type=%s\n",
                   state_info.switch_id,
                   lumitec_poco_state_to_string(state_info.switch_state),
                   lumitec_poco_type_to_string(state_info.switch_type));
        }
    }
    printf("\n");
    
    // Example 4: Create a start pattern message
    printf("=== Example 4: Start Pattern Message ===\n");
    if (lumitec_poco_create_start_pattern(&frame, 0x0E, 0x10, 1, 5)) {
        printf("Created start pattern message (Switch 1, Pattern 5):\n");
        print_can_frame(&frame);
        
        // Parse the message back
        lumitec_poco_start_pattern_t pattern;
        if (lumitec_poco_parse_start_pattern(&frame, &pattern)) {
            printf("Parsed: Switch=%d, Pattern=%d\n",
                   pattern.switch_id, pattern.pattern_id);
        }
    }
    printf("\n");
    
    // Example 5: Validate frame
    printf("=== Example 5: Frame Validation ===\n");
    printf("Frame is valid Lumitec Poco message: %s\n",
           lumitec_poco_is_valid_frame(&frame) ? "Yes" : "No");
    
    uint8_t prop_id;
    if (lumitec_poco_get_proprietary_id(&frame, &prop_id)) {
        printf("Proprietary ID: %d\n", prop_id);
    }
    
    return 0;
}
