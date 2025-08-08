/**
 * @file LumitecPoco.h
 * @brief Lumitec Poco CAN Protocol Definitions
 * 
 * This file contains definitions for the Lumitec Poco lighting system
 * custom NMEA2000 PGNs based on the Poco CAN bus Protocol specification.
 */

#ifndef LUMITEC_POCO_H
#define LUMITEC_POCO_H

#include <stdint.h>
#include "N2kTypes.h"
#include "N2kMsg.h"

// Lumitec Constants
#define LUMITEC_MANUFACTURER_CODE   1512
#define MARINE_INDUSTRY_CODE        4
#define LUMITEC_PGN_61184          61184

// Proprietary IDs for PGN 61184
enum LumitecPocoProprietaryID {
    PID_EXTSW_SIMPLE_ACTIONS = 1,        // External Switch: Simple Actions
    PID_EXTSW_STATE_INFO = 2,            // External Switch: Switch State Information
    PID_EXTSW_CUSTOM_HSB = 3,            // External Switch: Custom Hue, Saturation, Brightness
    PID_EXTSW_START_PATTERN = 4,         // External Switch: Start Pattern ID
    PID_OUTPUT_CHANNEL_STATUS = 5,       // Poco Output Channel: Status
    PID_OUTPUT_CHANNEL_BIN = 6,          // Poco Output Channel: BIN On/Off
    PID_OUTPUT_CHANNEL_PWM = 7,          // Poco Output Channel: PWM Dimming
    PID_OUTPUT_CHANNEL_PLI = 8,          // Poco Output Channel: PLI Message
    PID_OUTPUT_CHANNEL_PLI_T2HSB = 16    // Poco Output Channel: PLI T2HSB
};

// External Switch Action IDs
enum LumitecExtSwActionID {
    ACTION_NO_ACTION = 0,
    ACTION_OFF = 1,
    ACTION_ON = 2,
    ACTION_DIM_DOWN = 3,
    ACTION_DIM_UP = 4,
    ACTION_PATTERN_START = 6,
    ACTION_PATTERN_PAUSE = 7,
    ACTION_T2HSB = 8,        // Default Hue, Sat, and Bright
    ACTION_T2HS = 9,         // Default Hue and Sat
    ACTION_T2B = 10,         // Default Bright
    ACTION_WHITE = 20,
    ACTION_RED = 21,
    ACTION_GREEN = 22,
    ACTION_BLUE = 23,
    ACTION_PLAY_PAUSE = 31,
    ACTION_TOGGLE = 32,
    ACTION_ON_SCENE_START = 33  // On[n] scenes start at 33
};

// External Switch Types
enum LumitecExtSwType {
    EXTSW_NOT_CONFIGURED = 253,
    EXTSW_OFF = 0,
    EXTSW_HUE_SATURATION = 1,
    EXTSW_WHITE_KELVIN = 2,
    EXTSW_RUNNING_PATTERN = 3,
    EXTSW_SCENE_SELECT = 4
};

// Output Channel Modes
enum LumitecChannelMode {
    CHANNEL_NONE = 0,    // Off
    CHANNEL_BIN = 1,     // On/Off only
    CHANNEL_PWM = 2,     // Dimming
    CHANNEL_PLI = 3      // Power Line Instruction
};

// Structure for PGN 61184 PID 1: External Switch Simple Actions
struct LumitecExtSwSimpleAction {
    uint16_t manufacturerCode;
    uint8_t reserved;
    uint8_t industryCode;
    uint8_t proprietaryId;
    uint8_t actionId;
    uint8_t switchId;
};

// Structure for PGN 61184 PID 2: External Switch State Information
struct LumitecExtSwStateInfo {
    uint16_t manufacturerCode;
    uint8_t reserved;
    uint8_t industryCode;
    uint8_t proprietaryId;
    uint8_t extSwId;
    uint8_t extSwState;
    uint8_t extSwType;
};

// Structure for PGN 61184 PID 3: External Switch Custom HSB
struct LumitecExtSwCustomHSB {
    uint16_t manufacturerCode;
    uint8_t reserved;
    uint8_t industryCode;
    uint8_t proprietaryId;
    uint8_t actionId;
    uint8_t switchId;
    uint8_t hue;
    uint8_t saturation;
    uint8_t brightness;
};

// Structure for PGN 61184 PID 4: External Switch Start Pattern
struct LumitecExtSwStartPattern {
    uint16_t manufacturerCode;
    uint8_t reserved;
    uint8_t industryCode;
    uint8_t proprietaryId;
    uint8_t switchId;
    uint8_t patternId;
};

// Structure for PGN 61184 PID 5: Output Channel Status
struct LumitecOutputChannelStatus {
    uint16_t manufacturerCode;
    uint8_t reserved;
    uint8_t industryCode;
    uint8_t proprietaryId;
    uint8_t channel;
    uint8_t channelMode;
    uint8_t outputLevel;
    uint8_t inputVoltage;    // in 200mV units
    uint8_t current;         // in 100mA units
};

// Structure for PGN 61184 PID 6: Output Channel BIN
struct LumitecOutputChannelBin {
    uint16_t manufacturerCode;
    uint8_t reserved;
    uint8_t industryCode;
    uint8_t proprietaryId;
    uint8_t channel;
    uint8_t state;  // 0=Off, 1=On
};

// Structure for PGN 61184 PID 7: Output Channel PWM
struct LumitecOutputChannelPWM {
    uint16_t manufacturerCode;
    uint8_t reserved;
    uint8_t industryCode;
    uint8_t proprietaryId;
    uint8_t channel;
    uint8_t duty;           // PWM duty cycle 0-255
    uint16_t transitionTime; // Time to transition to new duty
};

// Structure for PGN 61184 PID 8: Output Channel PLI Message
struct LumitecOutputChannelPLI {
    uint16_t manufacturerCode;
    uint8_t reserved;
    uint8_t industryCode;
    uint8_t proprietaryId;
    uint8_t channel;
    uint32_t pliMessage;    // 32-bit PLI message
};

// Structure for PGN 61184 PID 16: Output Channel PLI T2HSB
struct LumitecOutputChannelPLIT2HSB {
    uint16_t manufacturerCode;
    uint8_t reserved;
    uint8_t industryCode;
    uint8_t proprietaryId;
    uint8_t channel;
    uint8_t pliClan;
    uint8_t transition;
    uint8_t brightness;
    uint8_t hue;
    uint8_t saturation;
};

// Function declarations for encoding/decoding Lumitec messages
bool ParseLumitecPGN61184(const tN2kMsg &N2kMsg, uint8_t &proprietaryId);
bool SetLumitecExtSwSimpleAction(tN2kMsg &N2kMsg, uint8_t destination, uint8_t actionId, uint8_t switchId);
bool ParseLumitecExtSwSimpleAction(const tN2kMsg &N2kMsg, LumitecExtSwSimpleAction &action);
bool SetLumitecExtSwStateInfo(tN2kMsg &N2kMsg, uint8_t extSwId, uint8_t extSwState, uint8_t extSwType);
bool ParseLumitecExtSwStateInfo(const tN2kMsg &N2kMsg, LumitecExtSwStateInfo &stateInfo);
bool SetLumitecExtSwCustomHSB(tN2kMsg &N2kMsg, uint8_t destination, uint8_t actionId, uint8_t switchId, uint8_t hue, uint8_t saturation, uint8_t brightness);
bool ParseLumitecExtSwCustomHSB(const tN2kMsg &N2kMsg, LumitecExtSwCustomHSB &customHSB);
bool SetLumitecExtSwStartPattern(tN2kMsg &N2kMsg, uint8_t destination, uint8_t switchId, uint8_t patternId);
bool ParseLumitecExtSwStartPattern(const tN2kMsg &N2kMsg, LumitecExtSwStartPattern &startPattern);
bool SetLumitecOutputChannelStatus(tN2kMsg &N2kMsg, uint8_t channel, uint8_t channelMode, uint8_t outputLevel, uint8_t inputVoltage, uint8_t current);
bool ParseLumitecOutputChannelStatus(const tN2kMsg &N2kMsg, LumitecOutputChannelStatus &status);
bool SetLumitecOutputChannelBin(tN2kMsg &N2kMsg, uint8_t destination, uint8_t channel, uint8_t state);
bool ParseLumitecOutputChannelBin(const tN2kMsg &N2kMsg, LumitecOutputChannelBin &binControl);
bool SetLumitecOutputChannelPWM(tN2kMsg &N2kMsg, uint8_t destination, uint8_t channel, uint8_t duty, uint16_t transitionTime);
bool ParseLumitecOutputChannelPWM(const tN2kMsg &N2kMsg, LumitecOutputChannelPWM &pwmControl);
bool SetLumitecOutputChannelPLI(tN2kMsg &N2kMsg, uint8_t destination, uint8_t channel, uint32_t pliMessage);
bool ParseLumitecOutputChannelPLI(const tN2kMsg &N2kMsg, LumitecOutputChannelPLI &pliControl);
bool SetLumitecOutputChannelPLIT2HSB(tN2kMsg &N2kMsg, uint8_t destination, uint8_t channel, uint8_t pliClan, uint8_t transition, uint8_t brightness, uint8_t hue, uint8_t saturation);
bool ParseLumitecOutputChannelPLIT2HSB(const tN2kMsg &N2kMsg, LumitecOutputChannelPLIT2HSB &pliT2HSB);

// Helper functions
const char* GetLumitecActionName(uint8_t actionId);
const char* GetLumitecExtSwTypeName(uint8_t extSwType);
const char* GetLumitecChannelModeName(uint8_t channelMode);
const char* GetLumitecProprietaryIdName(uint8_t proprietaryId);

#endif // LUMITEC_POCO_H
