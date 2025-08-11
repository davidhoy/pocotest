/**
 * @file nmea2000_integration.cpp
 * @brief Implementation of NMEA2000 integration helpers
 */

#include "nmea2000_integration.h"
#include <string.h>

bool poco_frame_to_n2k_msg(const lumitec_poco_can_frame_t* poco_frame, tN2kMsg* n2k_msg)
{
    if (!poco_frame || !n2k_msg) {
        return false;
    }
    
    // Set basic message properties
    n2k_msg->SetPGN(LUMITEC_PGN_61184);
    n2k_msg->Priority = poco_frame->priority;
    n2k_msg->Source = poco_frame->source_address;
    n2k_msg->Destination = poco_frame->destination_address;
    n2k_msg->DataLen = poco_frame->data_length;
    
    // Copy data payload
    memcpy(n2k_msg->Data, poco_frame->data, poco_frame->data_length);
    
    return true;
}

bool n2k_msg_to_poco_frame(const tN2kMsg* n2k_msg, lumitec_poco_can_frame_t* poco_frame)
{
    if (!n2k_msg || !poco_frame || n2k_msg->PGN != LUMITEC_PGN_61184) {
        return false;
    }
    
    memset(poco_frame, 0, sizeof(lumitec_poco_can_frame_t));
    
    // Extract basic properties
    poco_frame->priority = n2k_msg->Priority;
    poco_frame->source_address = n2k_msg->Source;
    poco_frame->destination_address = n2k_msg->Destination;
    poco_frame->data_length = n2k_msg->DataLen;
    
    // Copy data payload
    if (poco_frame->data_length <= LUMITEC_POCO_MAX_DATA_LEN) {
        memcpy(poco_frame->data, n2k_msg->Data, poco_frame->data_length);
    } else {
        return false;
    }
    
    // Calculate CAN ID (this is simplified - actual implementation depends on NMEA2000 library)
    poco_frame->can_id = ((uint32_t)poco_frame->priority << 26) |
                        (LUMITEC_PGN_61184 << 8) |
                        poco_frame->source_address;
    
    return true;
}

bool send_poco_simple_action(tNMEA2000* nmea2000,
                            uint8_t destination,
                            uint8_t source,
                            lumitec_poco_action_id_t action_id,
                            uint8_t switch_id)
{
    if (!nmea2000) {
        return false;
    }
    
    // Use Poco API to create the message
    lumitec_poco_can_frame_t poco_frame;
    if (!lumitec_poco_create_simple_action(&poco_frame, destination, source, action_id, switch_id)) {
        return false;
    }
    
    // Convert to NMEA2000 format
    tN2kMsg n2k_msg;
    if (!poco_frame_to_n2k_msg(&poco_frame, &n2k_msg)) {
        return false;
    }
    
    // Send via NMEA2000
    return nmea2000->SendMsg(n2k_msg);
}

bool send_poco_custom_hsb(tNMEA2000* nmea2000,
                         uint8_t destination,
                         uint8_t source,
                         lumitec_poco_action_id_t action_id,
                         uint8_t switch_id,
                         uint8_t hue,
                         uint8_t saturation,
                         uint8_t brightness)
{
    if (!nmea2000) {
        return false;
    }
    
    // Use Poco API to create the message
    lumitec_poco_can_frame_t poco_frame;
    if (!lumitec_poco_create_custom_hsb(&poco_frame, destination, source, action_id, switch_id, hue, saturation, brightness)) {
        return false;
    }
    
    // Convert to NMEA2000 format
    tN2kMsg n2k_msg;
    if (!poco_frame_to_n2k_msg(&poco_frame, &n2k_msg)) {
        return false;
    }
    
    // Send via NMEA2000
    return nmea2000->SendMsg(n2k_msg);
}
