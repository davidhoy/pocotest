/**
 * @file lumitec_poco_api.c
 * @brief Implementation of Lumitec Poco CAN Protocol API
 */

#include "lumitec_poco_api.h"
#include <string.h>
#include <stdio.h>

// Helper function to calculate NMEA2000 CAN ID
static uint32_t calculate_nmea2k_can_id(uint32_t pgn, uint8_t source, uint8_t destination, uint8_t priority)
{
    uint32_t can_id = 0;
    
    // NMEA2000 29-bit CAN ID format:
    // Bits 28-26: Priority (3 bits)
    // Bit 25: Reserved (1 bit) = 0
    // Bit 24: Data Page (1 bit) = 0 for standard PGNs
    // Bit 23: PDU Format (1 bit) = 1 for PGN >= 240
    // Bits 23-16: PGN high byte
    // Bits 15-8: PGN low byte or destination
    // Bits 7-0: Source address
    
    can_id |= ((uint32_t)priority & 0x07) << 26;  // Priority
    can_id |= ((uint32_t)pgn & 0x3FFFF) << 8;     // PGN (18 bits)
    can_id |= (uint32_t)source & 0xFF;            // Source address
    
    // For PGN 61184, set destination in appropriate bits
    if (destination != 255) {  // Not broadcast
        can_id |= ((uint32_t)destination & 0xFF) << 8;
    }
    
    return can_id;
}

// Helper function to extract manufacturer and industry codes from data
static bool extract_manufacturer_info(const uint8_t* data, uint16_t* manufacturer_code, uint8_t* industry_code)
{
    if (!data || !manufacturer_code || !industry_code) {
        return false;
    }
    
    // First approach: manufacturer and industry codes in first 2 bytes
    uint16_t combined = (uint16_t)data[1] << 8 | data[0];
    *manufacturer_code = combined & 0x7FF;  // 11 bits
    *industry_code = (combined >> 13) & 0x07;  // 3 bits
    
    return true;
}

// Helper function to pack manufacturer and industry codes
static void pack_manufacturer_info(uint8_t* data, uint16_t manufacturer_code, uint8_t industry_code)
{
    uint16_t combined = (manufacturer_code & 0x7FF) | ((uint16_t)(industry_code & 0x07) << 13);
    data[0] = combined & 0xFF;
    data[1] = (combined >> 8) & 0xFF;
}

const char* lumitec_poco_get_version(void)
{
    static char version_str[16];
    snprintf(version_str, sizeof(version_str), "%d.%d.%d",
             LUMITEC_POCO_API_VERSION_MAJOR,
             LUMITEC_POCO_API_VERSION_MINOR,
             LUMITEC_POCO_API_VERSION_PATCH);
    return version_str;
}

bool lumitec_poco_is_valid_frame(const lumitec_poco_can_frame_t* frame)
{
    if (!frame) {
        return false;
    }
    
    // Check minimum data length
    if (frame->data_length < 3) {
        return false;
    }
    
    // Extract and validate manufacturer and industry codes
    uint16_t manufacturer_code;
    uint8_t industry_code;
    if (!extract_manufacturer_info(frame->data, &manufacturer_code, &industry_code)) {
        return false;
    }
    
    return (manufacturer_code == LUMITEC_MANUFACTURER_CODE && 
            industry_code == MARINE_INDUSTRY_CODE);
}

bool lumitec_poco_get_proprietary_id(const lumitec_poco_can_frame_t* frame, uint8_t* proprietary_id)
{
    if (!frame || !proprietary_id) {
        return false;
    }
    
    if (!lumitec_poco_is_valid_frame(frame)) {
        return false;
    }
    
    *proprietary_id = frame->data[2];
    return true;
}

bool lumitec_poco_create_simple_action(lumitec_poco_can_frame_t* frame,
                                      uint8_t destination,
                                      uint8_t source,
                                      lumitec_poco_action_id_t action_id,
                                      uint8_t switch_id)
{
    if (!frame) {
        return false;
    }
    
    memset(frame, 0, sizeof(lumitec_poco_can_frame_t));
    
    frame->can_id = calculate_nmea2k_can_id(LUMITEC_PGN_61184, source, destination, 6);
    frame->priority = 6;
    frame->source_address = source;
    frame->destination_address = destination;
    frame->data_length = 6;
    
    // Pack manufacturer and industry codes
    pack_manufacturer_info(frame->data, LUMITEC_MANUFACTURER_CODE, MARINE_INDUSTRY_CODE);
    
    frame->data[2] = POCO_PID_EXTSW_SIMPLE_ACTIONS;
    frame->data[3] = action_id;
    frame->data[4] = switch_id;
    frame->data[5] = 0;  // Reserved
    
    return true;
}

bool lumitec_poco_create_state_info(lumitec_poco_can_frame_t* frame,
                                   uint8_t source,
                                   uint8_t switch_id,
                                   lumitec_poco_switch_state_t switch_state,
                                   lumitec_poco_switch_type_t switch_type)
{
    if (!frame) {
        return false;
    }
    
    memset(frame, 0, sizeof(lumitec_poco_can_frame_t));
    
    frame->can_id = calculate_nmea2k_can_id(LUMITEC_PGN_61184, source, 255, 6);  // Broadcast
    frame->priority = 6;
    frame->source_address = source;
    frame->destination_address = 255;  // Broadcast
    frame->data_length = 7;
    
    // Pack manufacturer and industry codes
    pack_manufacturer_info(frame->data, LUMITEC_MANUFACTURER_CODE, MARINE_INDUSTRY_CODE);
    
    frame->data[2] = POCO_PID_EXTSW_STATE_INFO;
    frame->data[3] = switch_id;
    frame->data[4] = switch_state;
    frame->data[5] = switch_type;
    frame->data[6] = 0;  // Reserved
    
    return true;
}

bool lumitec_poco_create_custom_hsb(lumitec_poco_can_frame_t* frame,
                                   uint8_t destination,
                                   uint8_t source,
                                   lumitec_poco_action_id_t action_id,
                                   uint8_t switch_id,
                                   uint8_t hue,
                                   uint8_t saturation,
                                   uint8_t brightness)
{
    if (!frame) {
        return false;
    }
    
    memset(frame, 0, sizeof(lumitec_poco_can_frame_t));
    
    frame->can_id = calculate_nmea2k_can_id(LUMITEC_PGN_61184, source, destination, 6);
    frame->priority = 6;
    frame->source_address = source;
    frame->destination_address = destination;
    frame->data_length = 8;
    
    // Pack manufacturer and industry codes
    pack_manufacturer_info(frame->data, LUMITEC_MANUFACTURER_CODE, MARINE_INDUSTRY_CODE);
    
    frame->data[2] = POCO_PID_EXTSW_CUSTOM_HSB;
    frame->data[3] = action_id;
    frame->data[4] = switch_id;
    frame->data[5] = hue;
    frame->data[6] = saturation;
    frame->data[7] = brightness;
    
    return true;
}

bool lumitec_poco_create_start_pattern(lumitec_poco_can_frame_t* frame,
                                      uint8_t destination,
                                      uint8_t source,
                                      uint8_t switch_id,
                                      uint8_t pattern_id)
{
    if (!frame) {
        return false;
    }
    
    memset(frame, 0, sizeof(lumitec_poco_can_frame_t));
    
    frame->can_id = calculate_nmea2k_can_id(LUMITEC_PGN_61184, source, destination, 6);
    frame->priority = 6;
    frame->source_address = source;
    frame->destination_address = destination;
    frame->data_length = 6;
    
    // Pack manufacturer and industry codes
    pack_manufacturer_info(frame->data, LUMITEC_MANUFACTURER_CODE, MARINE_INDUSTRY_CODE);
    
    frame->data[2] = POCO_PID_EXTSW_START_PATTERN;
    frame->data[3] = switch_id;
    frame->data[4] = pattern_id;
    frame->data[5] = 0;  // Reserved
    
    return true;
}

bool lumitec_poco_parse_simple_action(const lumitec_poco_can_frame_t* frame,
                                     lumitec_poco_simple_action_t* action)
{
    if (!frame || !action) {
        return false;
    }
    
    uint8_t proprietary_id;
    if (!lumitec_poco_get_proprietary_id(frame, &proprietary_id) || 
        proprietary_id != POCO_PID_EXTSW_SIMPLE_ACTIONS ||
        frame->data_length < 6) {
        return false;
    }
    
    extract_manufacturer_info(frame->data, &action->manufacturer_code, &action->industry_code);
    action->proprietary_id = frame->data[2];
    action->action_id = frame->data[3];
    action->switch_id = frame->data[4];
    
    return true;
}

bool lumitec_poco_parse_state_info(const lumitec_poco_can_frame_t* frame,
                                  lumitec_poco_state_info_t* state_info)
{
    if (!frame || !state_info) {
        return false;
    }
    
    uint8_t proprietary_id;
    if (!lumitec_poco_get_proprietary_id(frame, &proprietary_id) || 
        proprietary_id != POCO_PID_EXTSW_STATE_INFO ||
        frame->data_length < 7) {
        return false;
    }
    
    extract_manufacturer_info(frame->data, &state_info->manufacturer_code, &state_info->industry_code);
    state_info->proprietary_id = frame->data[2];
    state_info->switch_id = frame->data[3];
    state_info->switch_state = frame->data[4];
    state_info->switch_type = frame->data[5];
    
    return true;
}

bool lumitec_poco_parse_custom_hsb(const lumitec_poco_can_frame_t* frame,
                                  lumitec_poco_custom_hsb_t* custom_hsb)
{
    if (!frame || !custom_hsb) {
        return false;
    }
    
    uint8_t proprietary_id;
    if (!lumitec_poco_get_proprietary_id(frame, &proprietary_id) || 
        proprietary_id != POCO_PID_EXTSW_CUSTOM_HSB ||
        frame->data_length < 8) {
        return false;
    }
    
    extract_manufacturer_info(frame->data, &custom_hsb->manufacturer_code, &custom_hsb->industry_code);
    custom_hsb->proprietary_id = frame->data[2];
    custom_hsb->action_id = frame->data[3];
    custom_hsb->switch_id = frame->data[4];
    custom_hsb->hue = frame->data[5];
    custom_hsb->saturation = frame->data[6];
    custom_hsb->brightness = frame->data[7];
    
    return true;
}

bool lumitec_poco_parse_start_pattern(const lumitec_poco_can_frame_t* frame,
                                     lumitec_poco_start_pattern_t* start_pattern)
{
    if (!frame || !start_pattern) {
        return false;
    }
    
    uint8_t proprietary_id;
    if (!lumitec_poco_get_proprietary_id(frame, &proprietary_id) || 
        proprietary_id != POCO_PID_EXTSW_START_PATTERN ||
        frame->data_length < 6) {
        return false;
    }
    
    extract_manufacturer_info(frame->data, &start_pattern->manufacturer_code, &start_pattern->industry_code);
    start_pattern->proprietary_id = frame->data[2];
    start_pattern->switch_id = frame->data[3];
    start_pattern->pattern_id = frame->data[4];
    
    return true;
}

const char* lumitec_poco_action_to_string(lumitec_poco_action_id_t action_id)
{
    switch (action_id) {
        case POCO_ACTION_NO_ACTION: return "No Action";
        case POCO_ACTION_OFF: return "Off";
        case POCO_ACTION_ON: return "On";
        case POCO_ACTION_DIM_DOWN: return "Dim Down";
        case POCO_ACTION_DIM_UP: return "Dim Up";
        case POCO_ACTION_PATTERN_START: return "Pattern Start";
        case POCO_ACTION_PATTERN_PAUSE: return "Pattern Pause";
        case POCO_ACTION_T2HSB: return "T2HSB";
        case POCO_ACTION_T2HS: return "T2HS";
        case POCO_ACTION_T2B: return "T2B";
        case POCO_ACTION_WHITE: return "White";
        case POCO_ACTION_RED: return "Red";
        case POCO_ACTION_GREEN: return "Green";
        case POCO_ACTION_BLUE: return "Blue";
        case POCO_ACTION_PLAY_PAUSE: return "Play/Pause";
        case POCO_ACTION_PATTERN_NEXT: return "Pattern Next";
        case POCO_ACTION_PATTERN_PREV: return "Pattern Previous";
        default: return "Unknown";
    }
}

const char* lumitec_poco_state_to_string(lumitec_poco_switch_state_t state)
{
    switch (state) {
        case POCO_SWITCH_STATE_RELEASED: return "Released";
        case POCO_SWITCH_STATE_PRESSED: return "Pressed";
        case POCO_SWITCH_STATE_HELD: return "Held";
        default: return "Unknown";
    }
}

const char* lumitec_poco_type_to_string(lumitec_poco_switch_type_t type)
{
    switch (type) {
        case POCO_SWITCH_TYPE_MOMENTARY: return "Momentary";
        case POCO_SWITCH_TYPE_LATCHING: return "Latching";
        default: return "Unknown";
    }
}
