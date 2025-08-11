/**
 * @file lumitec_poco_api.h
 * @brief Lumitec Poco CAN Protocol API - Portable Implementation
 * 
 * This library provides a portable implementation of the Lumitec Poco lighting
 * system CAN protocol that can be used with any CAN stack or raw CAN interface.
 * 
 * The library is designed to be independent of any specific NMEA2000 library
 * and works directly with raw CAN frame data.
 * 
 * @version 1.0
 * @author Lumitec
 */

#ifndef LUMITEC_POCO_API_H
#define LUMITEC_POCO_API_H

#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

// Library version
#define LUMITEC_POCO_API_VERSION_MAJOR 1
#define LUMITEC_POCO_API_VERSION_MINOR 0
#define LUMITEC_POCO_API_VERSION_PATCH 0

// Lumitec Constants
#define LUMITEC_MANUFACTURER_CODE 1512
#define LUMITEC_PGN_61184 61184
#define MARINE_INDUSTRY_CODE 4  // Standard marine industry code

// Maximum CAN frame data length
#define LUMITEC_POCO_MAX_DATA_LEN 8

/**
 * @brief CAN Frame structure for Poco messages
 * 
 * This structure represents a standard CAN frame that can be used
 * with any CAN stack or raw CAN interface.
 */
typedef struct {
    uint32_t can_id;                                    ///< CAN ID (29-bit for extended frames)
    uint8_t data[LUMITEC_POCO_MAX_DATA_LEN];           ///< CAN data payload
    uint8_t data_length;                               ///< Number of valid bytes in data
    uint8_t priority;                                  ///< Message priority (0-7)
    uint8_t source_address;                           ///< Source node address
    uint8_t destination_address;                      ///< Destination address (255 = broadcast)
} lumitec_poco_can_frame_t;

/**
 * @brief Proprietary IDs for PGN 61184
 */
typedef enum {
    POCO_PID_EXTSW_SIMPLE_ACTIONS = 1,        ///< External Switch: Simple Actions
    POCO_PID_EXTSW_STATE_INFO = 2,            ///< External Switch: Switch State Information
    POCO_PID_EXTSW_CUSTOM_HSB = 3,            ///< External Switch: Custom Hue, Saturation, Brightness
    POCO_PID_EXTSW_START_PATTERN = 4,         ///< External Switch: Start Pattern ID
    POCO_PID_OUTPUT_CHANNEL_STATUS = 5,       ///< Poco Output Channel: Status
    POCO_PID_OUTPUT_CHANNEL_BIN = 6,          ///< Poco Output Channel: BIN On/Off
    POCO_PID_OUTPUT_CHANNEL_PWM = 7,          ///< Poco Output Channel: PWM Dimming
    POCO_PID_OUTPUT_CHANNEL_PLI = 8,          ///< Poco Output Channel: PLI Message
    POCO_PID_OUTPUT_CHANNEL_PLI_T2HSB = 16    ///< Poco Output Channel: PLI T2HSB
} lumitec_poco_proprietary_id_t;

/**
 * @brief External Switch Action IDs
 */
typedef enum {
    POCO_ACTION_NO_ACTION = 0,
    POCO_ACTION_OFF = 1,
    POCO_ACTION_ON = 2,
    POCO_ACTION_DIM_DOWN = 3,
    POCO_ACTION_DIM_UP = 4,
    POCO_ACTION_PATTERN_START = 6,
    POCO_ACTION_PATTERN_PAUSE = 7,
    POCO_ACTION_T2HSB = 8,        ///< Default Hue, Sat, and Bright
    POCO_ACTION_T2HS = 9,         ///< Default Hue and Sat
    POCO_ACTION_T2B = 10,         ///< Default Bright
    POCO_ACTION_WHITE = 20,
    POCO_ACTION_RED = 21,
    POCO_ACTION_GREEN = 22,
    POCO_ACTION_BLUE = 23,
    POCO_ACTION_PLAY_PAUSE = 31,
    POCO_ACTION_PATTERN_NEXT = 32,
    POCO_ACTION_PATTERN_PREV = 33
} lumitec_poco_action_id_t;

/**
 * @brief External Switch State values
 */
typedef enum {
    POCO_SWITCH_STATE_RELEASED = 0,
    POCO_SWITCH_STATE_PRESSED = 1,
    POCO_SWITCH_STATE_HELD = 2
} lumitec_poco_switch_state_t;

/**
 * @brief External Switch Type values
 */
typedef enum {
    POCO_SWITCH_TYPE_MOMENTARY = 0,
    POCO_SWITCH_TYPE_LATCHING = 1
} lumitec_poco_switch_type_t;

// Data structures for parsed messages

/**
 * @brief External Switch Simple Action message data
 */
typedef struct {
    uint16_t manufacturer_code;
    uint8_t industry_code;
    uint8_t proprietary_id;
    uint8_t action_id;
    uint8_t switch_id;
} lumitec_poco_simple_action_t;

/**
 * @brief External Switch State Information message data
 */
typedef struct {
    uint16_t manufacturer_code;
    uint8_t industry_code;
    uint8_t proprietary_id;
    uint8_t switch_id;
    uint8_t switch_state;
    uint8_t switch_type;
} lumitec_poco_state_info_t;

/**
 * @brief External Switch Custom HSB message data
 */
typedef struct {
    uint16_t manufacturer_code;
    uint8_t industry_code;
    uint8_t proprietary_id;
    uint8_t action_id;
    uint8_t switch_id;
    uint8_t hue;
    uint8_t saturation;
    uint8_t brightness;
} lumitec_poco_custom_hsb_t;

/**
 * @brief External Switch Start Pattern message data
 */
typedef struct {
    uint16_t manufacturer_code;
    uint8_t industry_code;
    uint8_t proprietary_id;
    uint8_t switch_id;
    uint8_t pattern_id;
} lumitec_poco_start_pattern_t;

// Core API functions

/**
 * @brief Get library version string
 * @return Version string in format "major.minor.patch"
 */
const char* lumitec_poco_get_version(void);

/**
 * @brief Validate if a CAN frame is a Lumitec Poco message
 * @param frame Pointer to CAN frame
 * @return true if frame is a valid Lumitec Poco message, false otherwise
 */
bool lumitec_poco_is_valid_frame(const lumitec_poco_can_frame_t* frame);

/**
 * @brief Get the proprietary ID from a Lumitec Poco CAN frame
 * @param frame Pointer to CAN frame
 * @param proprietary_id Pointer to store the extracted proprietary ID
 * @return true if successful, false if frame is invalid
 */
bool lumitec_poco_get_proprietary_id(const lumitec_poco_can_frame_t* frame, uint8_t* proprietary_id);

// Message creation functions

/**
 * @brief Create an External Switch Simple Action message
 * @param frame Pointer to CAN frame to populate
 * @param destination Destination node address (255 for broadcast)
 * @param source Source node address
 * @param action_id Action to perform
 * @param switch_id Switch identifier
 * @return true if successful, false on error
 */
bool lumitec_poco_create_simple_action(lumitec_poco_can_frame_t* frame,
                                      uint8_t destination,
                                      uint8_t source,
                                      lumitec_poco_action_id_t action_id,
                                      uint8_t switch_id);

/**
 * @brief Create an External Switch State Information message
 * @param frame Pointer to CAN frame to populate
 * @param source Source node address
 * @param switch_id Switch identifier
 * @param switch_state Current switch state
 * @param switch_type Type of switch
 * @return true if successful, false on error
 */
bool lumitec_poco_create_state_info(lumitec_poco_can_frame_t* frame,
                                   uint8_t source,
                                   uint8_t switch_id,
                                   lumitec_poco_switch_state_t switch_state,
                                   lumitec_poco_switch_type_t switch_type);

/**
 * @brief Create an External Switch Custom HSB message
 * @param frame Pointer to CAN frame to populate
 * @param destination Destination node address
 * @param source Source node address
 * @param action_id Action to perform
 * @param switch_id Switch identifier
 * @param hue Hue value (0-255)
 * @param saturation Saturation value (0-255)
 * @param brightness Brightness value (0-255)
 * @return true if successful, false on error
 */
bool lumitec_poco_create_custom_hsb(lumitec_poco_can_frame_t* frame,
                                   uint8_t destination,
                                   uint8_t source,
                                   lumitec_poco_action_id_t action_id,
                                   uint8_t switch_id,
                                   uint8_t hue,
                                   uint8_t saturation,
                                   uint8_t brightness);

/**
 * @brief Create an External Switch Start Pattern message
 * @param frame Pointer to CAN frame to populate
 * @param destination Destination node address
 * @param source Source node address
 * @param switch_id Switch identifier
 * @param pattern_id Pattern to start
 * @return true if successful, false on error
 */
bool lumitec_poco_create_start_pattern(lumitec_poco_can_frame_t* frame,
                                      uint8_t destination,
                                      uint8_t source,
                                      uint8_t switch_id,
                                      uint8_t pattern_id);

// Message parsing functions

/**
 * @brief Parse an External Switch Simple Action message
 * @param frame Pointer to CAN frame
 * @param action Pointer to store parsed action data
 * @return true if successful, false if frame is not a simple action message
 */
bool lumitec_poco_parse_simple_action(const lumitec_poco_can_frame_t* frame,
                                     lumitec_poco_simple_action_t* action);

/**
 * @brief Parse an External Switch State Information message
 * @param frame Pointer to CAN frame
 * @param state_info Pointer to store parsed state info data
 * @return true if successful, false if frame is not a state info message
 */
bool lumitec_poco_parse_state_info(const lumitec_poco_can_frame_t* frame,
                                  lumitec_poco_state_info_t* state_info);

/**
 * @brief Parse an External Switch Custom HSB message
 * @param frame Pointer to CAN frame
 * @param custom_hsb Pointer to store parsed HSB data
 * @return true if successful, false if frame is not a custom HSB message
 */
bool lumitec_poco_parse_custom_hsb(const lumitec_poco_can_frame_t* frame,
                                  lumitec_poco_custom_hsb_t* custom_hsb);

/**
 * @brief Parse an External Switch Start Pattern message
 * @param frame Pointer to CAN frame
 * @param start_pattern Pointer to store parsed pattern data
 * @return true if successful, false if frame is not a start pattern message
 */
bool lumitec_poco_parse_start_pattern(const lumitec_poco_can_frame_t* frame,
                                     lumitec_poco_start_pattern_t* start_pattern);

// Utility functions

/**
 * @brief Convert action ID to human-readable string
 * @param action_id Action ID to convert
 * @return String representation of action ID
 */
const char* lumitec_poco_action_to_string(lumitec_poco_action_id_t action_id);

/**
 * @brief Convert switch state to human-readable string
 * @param state Switch state to convert
 * @return String representation of switch state
 */
const char* lumitec_poco_state_to_string(lumitec_poco_switch_state_t state);

/**
 * @brief Convert switch type to human-readable string
 * @param type Switch type to convert
 * @return String representation of switch type
 */
const char* lumitec_poco_type_to_string(lumitec_poco_switch_type_t type);

#ifdef __cplusplus
}
#endif

#endif // LUMITEC_POCO_API_H
