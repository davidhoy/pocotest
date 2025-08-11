/**
 * @file nmea2000_integration.h
 * @brief Integration helpers for using Lumitec Poco API with NMEA2000 library
 * 
 * This file provides helper functions to convert between the portable
 * Lumitec Poco API format and the NMEA2000 library format.
 */

#ifndef NMEA2000_INTEGRATION_H
#define NMEA2000_INTEGRATION_H

#include "lumitec_poco_api.h"
#include "N2kMsg.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Convert Poco CAN frame to NMEA2000 message
 * @param poco_frame Source Poco CAN frame
 * @param n2k_msg Destination NMEA2000 message
 * @return true if successful, false on error
 */
bool poco_frame_to_n2k_msg(const lumitec_poco_can_frame_t* poco_frame, tN2kMsg* n2k_msg);

/**
 * @brief Convert NMEA2000 message to Poco CAN frame
 * @param n2k_msg Source NMEA2000 message
 * @param poco_frame Destination Poco CAN frame
 * @return true if successful, false on error
 */
bool n2k_msg_to_poco_frame(const tN2kMsg* n2k_msg, lumitec_poco_can_frame_t* poco_frame);

/**
 * @brief Create and send a simple action using Poco API + NMEA2000
 * @param nmea2000 NMEA2000 interface
 * @param destination Destination address
 * @param source Source address  
 * @param action_id Action to perform
 * @param switch_id Switch identifier
 * @return true if message was sent successfully
 */
bool send_poco_simple_action(tNMEA2000* nmea2000,
                            uint8_t destination,
                            uint8_t source,
                            lumitec_poco_action_id_t action_id,
                            uint8_t switch_id);

/**
 * @brief Create and send a custom HSB using Poco API + NMEA2000
 * @param nmea2000 NMEA2000 interface
 * @param destination Destination address
 * @param source Source address
 * @param action_id Action to perform
 * @param switch_id Switch identifier
 * @param hue Hue value (0-255)
 * @param saturation Saturation value (0-255)
 * @param brightness Brightness value (0-255)
 * @return true if message was sent successfully
 */
bool send_poco_custom_hsb(tNMEA2000* nmea2000,
                         uint8_t destination,
                         uint8_t source,
                         lumitec_poco_action_id_t action_id,
                         uint8_t switch_id,
                         uint8_t hue,
                         uint8_t saturation,
                         uint8_t brightness);

#ifdef __cplusplus
}
#endif

#endif // NMEA2000_INTEGRATION_H
