/**
 * @file LumitecPoco.cpp
 * @brief Implementation of Lumitec Poco CAN Protocol Functions
 */

#include "LumitecPoco.h"
#include <string.h>
#include <cstdio>

// Parse the basic Lumitec PGN 61184 header to get the Proprietary ID
bool ParseLumitecPGN61184(const tN2kMsg &N2kMsg, uint8_t &proprietaryId) {
    if (N2kMsg.PGN != LUMITEC_PGN_61184) return false;
    if (N2kMsg.DataLen < 3) return false;
    
    int Index = 0;
    uint16_t proprietaryInfo = N2kMsg.Get2ByteUInt(Index);
    
    // Check manufacturer code (11 bits) and industry code (3 bits)
    uint16_t manufacturerCode = proprietaryInfo & 0x7FF;
    uint8_t industryCode = (proprietaryInfo >> 13) & 0x07;
    
    if (manufacturerCode != LUMITEC_MANUFACTURER_CODE) return false;
    if (industryCode != MARINE_INDUSTRY_CODE) return false;
    
    proprietaryId = N2kMsg.GetByte(Index);
    return true;
}

// Setter functions
bool SetLumitecExtSwSimpleAction(tN2kMsg &N2kMsg, uint8_t destination, uint8_t actionId, uint8_t switchId) {
    N2kMsg.SetPGN(LUMITEC_PGN_61184);
    N2kMsg.Priority = 6;
    N2kMsg.Destination = destination;
    N2kMsg.DataLen = 6;
    
    int Index = 0;
    uint16_t mfgCode = LUMITEC_MANUFACTURER_CODE | (MARINE_INDUSTRY_CODE << 13);
    N2kMsg.Set2ByteUInt(mfgCode, Index);
    
    // Proprietary ID
    N2kMsg.SetByte(PID_EXTSW_SIMPLE_ACTIONS, Index);
    N2kMsg.SetByte(actionId, Index);
    N2kMsg.SetByte(switchId, Index);
    
    return true;
}

bool SetLumitecExtSwStateInfo(tN2kMsg &N2kMsg, uint8_t extSwId, uint8_t extSwState, uint8_t extSwType) {
    N2kMsg.SetPGN(LUMITEC_PGN_61184);
    N2kMsg.Priority = 6;
    N2kMsg.Destination = 255; // Broadcast
    N2kMsg.DataLen = 7;
    
    int Index = 0;
    uint16_t mfgCode = LUMITEC_MANUFACTURER_CODE | (MARINE_INDUSTRY_CODE << 13);
    N2kMsg.Set2ByteUInt(mfgCode, Index);
    
    // Industry code (3 bits) + reserved bits (lower 2 bits set to 11)
    N2kMsg.SetByte(PID_EXTSW_STATE_INFO, Index);
    N2kMsg.SetByte(extSwId, Index);
    N2kMsg.SetByte(extSwState, Index);
    N2kMsg.SetByte(extSwType, Index);
    
    return true;
}

bool SetLumitecExtSwCustomHSB(tN2kMsg &N2kMsg, uint8_t destination, uint8_t actionId, uint8_t switchId, uint8_t hue, uint8_t saturation, uint8_t brightness) {
    N2kMsg.SetPGN(LUMITEC_PGN_61184);
    N2kMsg.Priority = 6;
    N2kMsg.Destination = destination;
    N2kMsg.DataLen = 9;
    
    int Index = 0;
    uint16_t mfgCode = LUMITEC_MANUFACTURER_CODE | (MARINE_INDUSTRY_CODE << 13);
    N2kMsg.Set2ByteUInt(mfgCode, Index);
    
    // Industry code (3 bits) + reserved bits (lower 2 bits set to 11)
    N2kMsg.SetByte(PID_EXTSW_CUSTOM_HSB, Index);
    N2kMsg.SetByte(actionId, Index);
    N2kMsg.SetByte(switchId, Index);
    N2kMsg.SetByte(hue, Index);
    N2kMsg.SetByte(saturation, Index);
    N2kMsg.SetByte(brightness, Index);
    
    return true;
}

bool SetLumitecExtSwStartPattern(tN2kMsg &N2kMsg, uint8_t destination, uint8_t switchId, uint8_t patternId) {
    N2kMsg.SetPGN(LUMITEC_PGN_61184);
    N2kMsg.Priority = 6;
    N2kMsg.Destination = destination;
    N2kMsg.DataLen = 6;
    
    int Index = 0;
    uint16_t mfgCode = LUMITEC_MANUFACTURER_CODE | (MARINE_INDUSTRY_CODE << 13);
    N2kMsg.Set2ByteUInt(mfgCode, Index);
    
    // Proprietary ID
    N2kMsg.SetByte(PID_EXTSW_START_PATTERN, Index);
    N2kMsg.SetByte(switchId, Index);
    N2kMsg.SetByte(patternId, Index);
    
    return true;
}

bool SetLumitecOutputChannelStatus(tN2kMsg &N2kMsg, uint8_t channel, uint8_t channelMode, uint8_t outputLevel, uint8_t inputVoltage, uint8_t current) {
    N2kMsg.SetPGN(LUMITEC_PGN_61184);
    N2kMsg.Priority = 6;
    N2kMsg.Destination = 255; // Broadcast status
    N2kMsg.DataLen = 9;
    
    int Index = 0;
    uint16_t mfgCode = LUMITEC_MANUFACTURER_CODE | (MARINE_INDUSTRY_CODE << 13);
    N2kMsg.Set2ByteUInt(mfgCode, Index);
    
    // Industry code (3 bits) + reserved bits (lower 2 bits set to 11)
    N2kMsg.SetByte(PID_OUTPUT_CHANNEL_STATUS, Index);
    N2kMsg.SetByte(channel, Index);
    N2kMsg.SetByte(channelMode, Index);
    N2kMsg.SetByte(outputLevel, Index);
    N2kMsg.SetByte(inputVoltage, Index);
    N2kMsg.SetByte(current, Index);
    
    return true;
}

bool SetLumitecOutputChannelBin(tN2kMsg &N2kMsg, uint8_t destination, uint8_t channel, uint8_t state) {
    N2kMsg.SetPGN(LUMITEC_PGN_61184);
    N2kMsg.Priority = 6;
    N2kMsg.Destination = destination;
    N2kMsg.DataLen = 6;
    
    int Index = 0;
    uint16_t mfgCode = LUMITEC_MANUFACTURER_CODE | (MARINE_INDUSTRY_CODE << 13);
    N2kMsg.Set2ByteUInt(mfgCode, Index);
    
    // Industry code (3 bits) + reserved bits (lower 2 bits set to 11)
    N2kMsg.SetByte(PID_OUTPUT_CHANNEL_BIN, Index);
    N2kMsg.SetByte(channel, Index);
    N2kMsg.SetByte(state, Index);
    
    return true;
}

bool SetLumitecOutputChannelPWM(tN2kMsg &N2kMsg, uint8_t destination, uint8_t channel, uint8_t duty, uint16_t transitionTime) {
    N2kMsg.SetPGN(LUMITEC_PGN_61184);
    N2kMsg.Priority = 6;
    N2kMsg.Destination = destination;
    N2kMsg.DataLen = 8;
    
    int Index = 0;
    uint16_t mfgCode = LUMITEC_MANUFACTURER_CODE | (MARINE_INDUSTRY_CODE << 13);
    N2kMsg.Set2ByteUInt(mfgCode, Index);
    
    // Industry code (3 bits) + reserved bits (lower 2 bits set to 11)
    N2kMsg.SetByte(PID_OUTPUT_CHANNEL_PWM, Index);
    N2kMsg.SetByte(channel, Index);
    N2kMsg.SetByte(duty, Index);
    N2kMsg.Set2ByteUInt(transitionTime, Index);
    
    return true;
}

bool SetLumitecOutputChannelPLI(tN2kMsg &N2kMsg, uint8_t destination, uint8_t channel, uint32_t pliMessage) {
    N2kMsg.SetPGN(LUMITEC_PGN_61184);
    N2kMsg.Priority = 6;
    N2kMsg.Destination = destination;
    N2kMsg.DataLen = 9;
    
    int Index = 0;
    uint16_t mfgCode = LUMITEC_MANUFACTURER_CODE | (MARINE_INDUSTRY_CODE << 13);
    N2kMsg.Set2ByteUInt(mfgCode, Index);
    
    // Industry code (3 bits) + reserved bits (lower 2 bits set to 11)
    N2kMsg.SetByte(PID_OUTPUT_CHANNEL_PLI, Index);
    N2kMsg.SetByte(channel, Index);
    
    // Set 4-byte PLI message (little-endian)
    N2kMsg.SetByte((uint8_t)(pliMessage & 0xFF), Index);
    N2kMsg.SetByte((uint8_t)((pliMessage >> 8) & 0xFF), Index);
    N2kMsg.SetByte((uint8_t)((pliMessage >> 16) & 0xFF), Index);
    N2kMsg.SetByte((uint8_t)((pliMessage >> 24) & 0xFF), Index);
    
    return true;
}

bool SetLumitecOutputChannelPLIT2HSB(tN2kMsg &N2kMsg, uint8_t destination, uint8_t channel, uint8_t pliClan, uint8_t transition, uint8_t brightness, uint8_t hue, uint8_t saturation) {
    N2kMsg.SetPGN(LUMITEC_PGN_61184);
    N2kMsg.Priority = 6;
    N2kMsg.Destination = destination;
    N2kMsg.DataLen = 8;
    
    int Index = 0;
    uint16_t mfgCode = LUMITEC_MANUFACTURER_CODE | (MARINE_INDUSTRY_CODE << 13);
    N2kMsg.Set2ByteUInt(mfgCode, Index);
    
    // Industry code (3 bits) + reserved bits (lower 2 bits set to 11)
    N2kMsg.SetByte(PID_OUTPUT_CHANNEL_PLI_T2HSB, Index);
    N2kMsg.SetByte(channel, Index);
    
    // Pack PLI Clan and Transition
    uint8_t packed1 = (pliClan & 0x3F) | ((transition & 0x07) << 6);
    N2kMsg.SetByte(packed1, Index);
    
    // Pack Brightness and Hue (high bits)
    uint8_t packed2 = (brightness & 0x0F) | (hue & 0xF0);
    N2kMsg.SetByte(packed2, Index);
    
    // Pack Hue (low bits) and Saturation
    uint8_t packed3 = ((hue & 0x0F) << 4) | ((saturation & 0x07) << 1);
    N2kMsg.SetByte(packed3, Index);
    
    return true;
}

// Parser functions
bool ParseLumitecExtSwSimpleAction(const tN2kMsg &N2kMsg, LumitecExtSwSimpleAction &action) {
    uint8_t proprietaryId;
    if (!ParseLumitecPGN61184(N2kMsg, proprietaryId) || proprietaryId != PID_EXTSW_SIMPLE_ACTIONS) return false;
    if (N2kMsg.DataLen < 6) return false;
    
    int Index = 0;
    action.manufacturerCode = N2kMsg.Get2ByteUInt(Index) & 0x7FF;
    uint8_t temp = N2kMsg.GetByte(Index);
    action.reserved = (temp >> 5) & 0x07;
    action.industryCode = (temp >> 2) & 0x07;
    action.proprietaryId = N2kMsg.GetByte(Index);
    action.actionId = N2kMsg.GetByte(Index);
    action.switchId = N2kMsg.GetByte(Index);
    
    return true;
}

bool ParseLumitecExtSwStateInfo(const tN2kMsg &N2kMsg, LumitecExtSwStateInfo &stateInfo) {
    uint8_t proprietaryId;
    if (!ParseLumitecPGN61184(N2kMsg, proprietaryId) || proprietaryId != PID_EXTSW_STATE_INFO) return false;
    if (N2kMsg.DataLen < 7) return false;
    
    int Index = 0;
    stateInfo.manufacturerCode = N2kMsg.Get2ByteUInt(Index) & 0x7FF;
    uint8_t temp = N2kMsg.GetByte(Index);
    stateInfo.reserved = (temp >> 5) & 0x07;
    stateInfo.industryCode = (temp >> 2) & 0x07;
    stateInfo.proprietaryId = N2kMsg.GetByte(Index);
    stateInfo.extSwId = N2kMsg.GetByte(Index);
    stateInfo.extSwState = N2kMsg.GetByte(Index);
    stateInfo.extSwType = N2kMsg.GetByte(Index);
    
    return true;
}

bool ParseLumitecExtSwCustomHSB(const tN2kMsg &N2kMsg, LumitecExtSwCustomHSB &customHSB) {
    uint8_t proprietaryId;
    if (!ParseLumitecPGN61184(N2kMsg, proprietaryId) || proprietaryId != PID_EXTSW_CUSTOM_HSB) return false;
    if (N2kMsg.DataLen < 8) return false;
    
    int Index = 0;
    customHSB.manufacturerCode = N2kMsg.Get2ByteUInt(Index) & 0x7FF;
    uint8_t temp = N2kMsg.GetByte(Index);
    customHSB.reserved = (temp >> 5) & 0x07;
    customHSB.industryCode = (temp >> 2) & 0x07;
    customHSB.proprietaryId = N2kMsg.GetByte(Index);
    customHSB.actionId = N2kMsg.GetByte(Index);
    customHSB.switchId = N2kMsg.GetByte(Index);
    customHSB.hue = N2kMsg.GetByte(Index);
    customHSB.saturation = N2kMsg.GetByte(Index);
    customHSB.brightness = N2kMsg.GetByte(Index);
    
    return true;
}

bool ParseLumitecExtSwStartPattern(const tN2kMsg &N2kMsg, LumitecExtSwStartPattern &startPattern) {
    uint8_t proprietaryId;
    if (!ParseLumitecPGN61184(N2kMsg, proprietaryId) || proprietaryId != PID_EXTSW_START_PATTERN) return false;
    if (N2kMsg.DataLen < 6) return false;
    
    int Index = 0;
    startPattern.manufacturerCode = N2kMsg.Get2ByteUInt(Index) & 0x7FF;
    uint8_t temp = N2kMsg.GetByte(Index);
    startPattern.reserved = (temp >> 5) & 0x07;
    startPattern.industryCode = (temp >> 2) & 0x07;
    startPattern.proprietaryId = N2kMsg.GetByte(Index);
    startPattern.switchId = N2kMsg.GetByte(Index);
    startPattern.patternId = N2kMsg.GetByte(Index);
    
    return true;
}

bool ParseLumitecOutputChannelStatus(const tN2kMsg &N2kMsg, LumitecOutputChannelStatus &status) {
    uint8_t proprietaryId;
    if (!ParseLumitecPGN61184(N2kMsg, proprietaryId) || proprietaryId != PID_OUTPUT_CHANNEL_STATUS) return false;
    if (N2kMsg.DataLen < 8) return false;
    
    int Index = 0;
    status.manufacturerCode = N2kMsg.Get2ByteUInt(Index) & 0x7FF;
    uint8_t temp = N2kMsg.GetByte(Index);
    status.reserved = (temp >> 5) & 0x07;
    status.industryCode = (temp >> 2) & 0x07;
    status.proprietaryId = N2kMsg.GetByte(Index);
    status.channel = N2kMsg.GetByte(Index);
    status.channelMode = N2kMsg.GetByte(Index);
    status.outputLevel = N2kMsg.GetByte(Index);
    status.inputVoltage = N2kMsg.GetByte(Index);
    status.current = N2kMsg.GetByte(Index);
    
    return true;
}

bool ParseLumitecOutputChannelBin(const tN2kMsg &N2kMsg, LumitecOutputChannelBin &binControl) {
    uint8_t proprietaryId;
    if (!ParseLumitecPGN61184(N2kMsg, proprietaryId) || proprietaryId != PID_OUTPUT_CHANNEL_BIN) return false;
    if (N2kMsg.DataLen < 5) return false;
    
    int Index = 0;
    binControl.manufacturerCode = N2kMsg.Get2ByteUInt(Index) & 0x7FF;
    uint8_t temp = N2kMsg.GetByte(Index);
    binControl.reserved = (temp >> 5) & 0x07;
    binControl.industryCode = (temp >> 2) & 0x07;
    binControl.proprietaryId = N2kMsg.GetByte(Index);
    binControl.channel = N2kMsg.GetByte(Index);
    binControl.state = N2kMsg.GetByte(Index);
    
    return true;
}

bool ParseLumitecOutputChannelPWM(const tN2kMsg &N2kMsg, LumitecOutputChannelPWM &pwmControl) {
    uint8_t proprietaryId;
    if (!ParseLumitecPGN61184(N2kMsg, proprietaryId) || proprietaryId != PID_OUTPUT_CHANNEL_PWM) return false;
    if (N2kMsg.DataLen < 7) return false;
    
    int Index = 0;
    pwmControl.manufacturerCode = N2kMsg.Get2ByteUInt(Index) & 0x7FF;
    uint8_t temp = N2kMsg.GetByte(Index);
    pwmControl.reserved = (temp >> 5) & 0x07;
    pwmControl.industryCode = (temp >> 2) & 0x07;
    pwmControl.proprietaryId = N2kMsg.GetByte(Index);
    pwmControl.channel = N2kMsg.GetByte(Index);
    pwmControl.duty = N2kMsg.GetByte(Index);
    pwmControl.transitionTime = N2kMsg.Get2ByteUInt(Index);
    
    return true;
}

bool ParseLumitecOutputChannelPLI(const tN2kMsg &N2kMsg, LumitecOutputChannelPLI &pliControl) {
    uint8_t proprietaryId;
    if (!ParseLumitecPGN61184(N2kMsg, proprietaryId) || proprietaryId != PID_OUTPUT_CHANNEL_PLI) return false;
    if (N2kMsg.DataLen < 8) return false;
    
    int Index = 0;
    pliControl.manufacturerCode = N2kMsg.Get2ByteUInt(Index) & 0x7FF;
    uint8_t temp = N2kMsg.GetByte(Index);
    pliControl.reserved = (temp >> 5) & 0x07;
    pliControl.industryCode = (temp >> 2) & 0x07;
    pliControl.proprietaryId = N2kMsg.GetByte(Index);
    pliControl.channel = N2kMsg.GetByte(Index);
    pliControl.pliMessage = N2kMsg.Get4ByteUInt(Index);
    
    return true;
}

bool ParseLumitecOutputChannelPLIT2HSB(const tN2kMsg &N2kMsg, LumitecOutputChannelPLIT2HSB &pliT2HSB) {
    uint8_t proprietaryId;
    if (!ParseLumitecPGN61184(N2kMsg, proprietaryId) || proprietaryId != PID_OUTPUT_CHANNEL_PLI_T2HSB) return false;
    if (N2kMsg.DataLen < 7) return false;
    
    int Index = 0;
    pliT2HSB.manufacturerCode = N2kMsg.Get2ByteUInt(Index) & 0x7FF;
    uint8_t temp = N2kMsg.GetByte(Index);
    pliT2HSB.reserved = (temp >> 5) & 0x07;
    pliT2HSB.industryCode = (temp >> 2) & 0x07;
    pliT2HSB.proprietaryId = N2kMsg.GetByte(Index);
    pliT2HSB.channel = N2kMsg.GetByte(Index);
    
    // Unpack PLI Clan and Transition
    uint8_t packed = N2kMsg.GetByte(Index);
    pliT2HSB.pliClan = packed & 0x3F;
    pliT2HSB.transition = (packed >> 6) & 0x07;
    
    // Unpack Brightness and Hue (high bits)
    uint8_t packed2 = N2kMsg.GetByte(Index);
    pliT2HSB.brightness = packed2 & 0x0F;
    uint8_t hueHigh = packed2 & 0xF0;
    
    // Unpack Hue (low bits) and Saturation
    uint8_t packed3 = N2kMsg.GetByte(Index);
    uint8_t hueLow = (packed3 >> 4) & 0x0F;
    pliT2HSB.hue = hueHigh | hueLow;
    pliT2HSB.saturation = (packed3 >> 1) & 0x07;
    
    return true;
}

// Helper functions to get human-readable names
const char* GetLumitecActionName(uint8_t actionId) {
    switch (actionId) {
        case ACTION_NO_ACTION: return "No Action";
        case ACTION_OFF: return "Off";
        case ACTION_ON: return "On";
        case ACTION_DIM_DOWN: return "Dim Down";
        case ACTION_DIM_UP: return "Dim Up";
        case ACTION_PATTERN_START: return "Pattern Start";
        case ACTION_PATTERN_PAUSE: return "Pattern Pause";
        case ACTION_T2HSB: return "To HSB";
        case ACTION_T2HS: return "To HS";
        case ACTION_T2B: return "To Brightness";
        case ACTION_WHITE: return "White";
        case ACTION_RED: return "Red";
        case ACTION_GREEN: return "Green";
        case ACTION_BLUE: return "Blue";
        case ACTION_PLAY_PAUSE: return "Play/Pause";
        case ACTION_TOGGLE: return "Toggle";
        default:
            if (actionId >= ACTION_ON_SCENE_START && actionId <= 65) {
                static char buffer[16];
                snprintf(buffer, sizeof(buffer), "On[%d]", actionId - ACTION_ON_SCENE_START + 1);
                return buffer;
            }
            return "Unknown";
    }
}

const char* GetLumitecExtSwTypeName(uint8_t extSwType) {
    switch (extSwType) {
        case EXTSW_NOT_CONFIGURED: return "Not Configured";
        case EXTSW_OFF: return "Off";
        case EXTSW_HUE_SATURATION: return "Hue/Saturation";
        case EXTSW_WHITE_KELVIN: return "White Kelvin";
        case EXTSW_RUNNING_PATTERN: return "Running Pattern";
        case EXTSW_SCENE_SELECT: return "Scene Select";
        default: return "Unknown";
    }
}

const char* GetLumitecChannelModeName(uint8_t channelMode) {
    switch (channelMode) {
        case CHANNEL_NONE: return "None/Off";
        case CHANNEL_BIN: return "Binary On/Off";
        case CHANNEL_PWM: return "PWM Dimming";
        case CHANNEL_PLI: return "PLI";
        default: return "Unknown";
    }
}

const char* GetLumitecProprietaryIdName(uint8_t proprietaryId) {
    switch (proprietaryId) {
        case PID_EXTSW_SIMPLE_ACTIONS: return "ExtSw Simple Actions";
        case PID_EXTSW_STATE_INFO: return "ExtSw State Info";
        case PID_EXTSW_CUSTOM_HSB: return "ExtSw Custom HSB";
        case PID_EXTSW_START_PATTERN: return "ExtSw Start Pattern";
        case PID_OUTPUT_CHANNEL_STATUS: return "Output Channel Status";
        case PID_OUTPUT_CHANNEL_BIN: return "Output Channel Binary";
        case PID_OUTPUT_CHANNEL_PWM: return "Output Channel PWM";
        case PID_OUTPUT_CHANNEL_PLI: return "Output Channel PLI";
        case PID_OUTPUT_CHANNEL_PLI_T2HSB: return "Output Channel PLI T2HSB";
        default: return "Unknown";
    }
}
