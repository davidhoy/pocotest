#include "dbcdecoder.h"
#include <QDebug>
#include <QFile>
#include <QTextStream>
#include <QNetworkAccessManager>
#include <QNetworkRequest>
#include <QNetworkReply>
#include <QEventLoop>
#include <QRegularExpression>
#include <QStringList>
#include <cmath>
#include "N2kMessages.h"

DBCDecoder::DBCDecoder(QObject *parent)
    : QObject(parent)
{
    bool dbcLoaded = false;
    
    // First try to load from a local NMEA2000 DBC file
    if (QFile::exists("nmea2000.dbc")) {
        dbcLoaded = loadDBCFile("nmea2000.dbc");
        qDebug() << "Loaded local DBC file:" << dbcLoaded;
    }
    
    // If no local file, try to download from canboat repository
    if (!dbcLoaded) {
        QString dbcUrl = "https://raw.githubusercontent.com/canboat/canboat/refs/heads/master/dbc-exporter/pgns.dbc";
        qDebug() << "Attempting to download DBC file from:" << dbcUrl;
        dbcLoaded = loadDBCFromUrl(dbcUrl);
        qDebug() << "Downloaded DBC file:" << dbcLoaded;
    }
    
    // If download failed, use fallback hardcoded definitions
    if (!dbcLoaded) {
        qDebug() << "Using fallback hardcoded NMEA2000 definitions";
        initializeStandardNMEA2000();
    }
    
    qDebug() << "DBCDecoder initialized with" << m_messages.size() << "message definitions";
}

DBCDecoder::~DBCDecoder()
{
}

bool DBCDecoder::loadDBCFile(const QString& filePath)
{
    QFile file(filePath);
    if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) {
        qDebug() << "Failed to open DBC file:" << filePath;
        return false;
    }
    
    QTextStream in(&file);
    QString content = in.readAll();
    file.close();
    
    return parseDBCFile(content);
}

bool DBCDecoder::loadDBCFromUrl(const QString& url)
{
    QNetworkAccessManager manager;
    QNetworkRequest request(url);
    request.setRawHeader("User-Agent", "NMEA2000-Analyzer/1.0");
    
    QNetworkReply* reply = manager.get(request);
    QEventLoop loop;
    QObject::connect(reply, &QNetworkReply::finished, &loop, &QEventLoop::quit);
    loop.exec();
    
    bool success = false;
    if (reply->error() == QNetworkReply::NoError) {
        QString content = reply->readAll();
        success = parseDBCFile(content);
        
        // Save downloaded DBC file for future use
        QFile file("nmea2000.dbc");
        if (file.open(QIODevice::WriteOnly | QIODevice::Text)) {
            QTextStream out(&file);
            out << content;
            file.close();
            qDebug() << "Saved DBC file to nmea2000.dbc";
        }
    } else {
        qDebug() << "Failed to download DBC file:" << reply->errorString();
    }
    
    reply->deleteLater();
    return success;
}

bool DBCDecoder::parseDBCFile(const QString& content)
{
    QStringList lines = content.split('\n');
    m_messages.clear();
    
    int messagesAdded = 0;
    
    for (int i = 0; i < lines.size(); i++) {
        QString line = lines[i].trimmed();
        
        // Look for message definitions (BO_ keyword)
        if (line.startsWith("BO_ ")) {
            QRegularExpression msgRegex(R"(BO_\s+(\d+)\s+([A-Za-z0-9_]+)\s*:\s*(\d+)\s+([A-Za-z0-9_]+))");
            QRegularExpressionMatch match = msgRegex.match(line);
            
            if (match.hasMatch()) {
                unsigned long canId = match.captured(1).toULong();
                QString name = match.captured(2);
                int dlc = match.captured(3).toInt();
                
                // Extract PGN from CAN ID for NMEA 2000
                unsigned long pgn = (canId >> 8) & 0x1FFFF;
                
                // Clean up message name by removing PGN_XXXXX_ prefix
                QString cleanName = name;
                QRegularExpression pgnPrefixRegex(R"(^PGN_\d+_)");
                cleanName.remove(pgnPrefixRegex);
                
                // Check if this is a proprietary PGN
                QString displayName;
                if ((pgn >= 65280 && pgn <= 65535) ||          // 0xFF00-0xFFFF: Single-frame proprietary
                    (pgn >= 126720 && pgn <= 126975) ||        // 0x1EF00-0x1EFFF: Multi-frame proprietary
                    (pgn >= 127744 && pgn <= 128511)) {         // 0x1F300-0x1F5FF: Additional proprietary
                    // Proprietary PGN - use special format
                    displayName = QString("Proprietary %1").arg(pgn);
                } else {
                    // Standard PGN - convert camelCase to Title Case for better readability
                    displayName = cleanName;
                    if (!displayName.isEmpty()) {
                        // Insert spaces before capital letters (except the first one)
                        for (int i = displayName.length() - 1; i > 0; i--) {
                            if (displayName[i].isUpper() && displayName[i-1].isLower()) {
                                displayName.insert(i, " ");
                            }
                        }
                        // Capitalize first letter
                        displayName[0] = displayName[0].toUpper();
                    }
                }
                
                DBCMessage message;
                message.pgn = pgn;
                message.name = displayName;
                message.dlc = dlc;
                message.description = displayName;
                
                // Parse signals for this message
                i++; // Move to next line
                while (i < lines.size()) {
                    QString signalLine = lines[i].trimmed();
                    
                    // Check if this is a signal line
                    if (signalLine.startsWith("SG_ ")) {
                        DBCSignal signal = parseDBCSignal(signalLine);
                        if (!signal.name.isEmpty()) {
                            message.signalList.append(signal);
                        }
                        i++;
                    } else if (signalLine.startsWith("BO_ ") || signalLine.isEmpty()) {
                        // End of this message, back up one line
                        i--;
                        break;
                    } else {
                        i++;
                    }
                }
                
                addMessage(message);
                messagesAdded++;
            }
        }
    }
    
    qDebug() << "Parsed" << messagesAdded << "messages from DBC file";
    return messagesAdded > 0;
}

DBCSignal DBCDecoder::parseDBCSignal(const QString& signalLine)
{
    DBCSignal signal;
    
    // Parse signal format: SG_ SignalName : StartBit|Length@ByteOrder+ (Factor,Offset) [Min|Max] "Unit" Receiver
    QRegularExpression signalRegex("SG_\\s+([A-Za-z0-9_]+)\\s*:\\s*(\\d+)\\|(\\d+)@([01])([+-])\\s*\\(([^,]+),([^)]+)\\)\\s*\\[([^|]*)\\|([^\\]]*)\\]\\s*\"([^\"]*)\"\\s*");
    QRegularExpressionMatch match = signalRegex.match(signalLine);
    
    if (match.hasMatch()) {
        signal.name = match.captured(1);
        signal.startBit = match.captured(2).toInt();
        signal.bitLength = match.captured(3).toInt();
        signal.isSigned = (match.captured(5) == "-");
        signal.scale = match.captured(6).toDouble();
        signal.offset = match.captured(7).toDouble();
        signal.minimum = match.captured(8).toDouble();
        signal.maximum = match.captured(9).toDouble();
        signal.unit = match.captured(10);
        signal.description = signal.name; // Use name as description
    }
    
    return signal;
}

void DBCDecoder::initializeStandardNMEA2000()
{
    // Minimal fallback - just add a few basic message stubs for when DBC file is unavailable
    // In practice, this should rarely be used since we download the comprehensive canboat DBC file
    
    qDebug() << "Using minimal fallback NMEA2000 definitions (DBC file unavailable)";
    
    // Add just a few basic message types as emergency fallback
    DBCMessage msg;
    
    // Engine Parameters Rapid (127488)
    msg.pgn = 127488;
    msg.name = "Engine Parameters, Rapid Update";
    msg.description = "Basic engine data (fallback)";
    msg.dlc = 8;
    msg.signalList.clear();
    addMessage(msg);
    
    // Wind Data (130306)
    msg.pgn = 130306;
    msg.name = "Wind Data";
    msg.description = "Wind speed and direction (fallback)";
    msg.dlc = 8;
    msg.signalList.clear();
    addMessage(msg);
    
    // Temperature (130312)
    msg.pgn = 130312;
    msg.name = "Temperature";
    msg.description = "Temperature data (fallback)";
    msg.dlc = 8;
    msg.signalList.clear();
    addMessage(msg);
    
    qDebug() << "Initialized" << m_messages.size() << "fallback message definitions";
}

void DBCDecoder::addMessage(const DBCMessage& message)
{
    m_messages[message.pgn] = message;
}

DecodedMessage DBCDecoder::decodeMessage(const tN2kMsg& msg)
{
    DecodedMessage decoded;
    decoded.isDecoded = false;

    // Special handling for PGN 126208 (Request/Command/Acknowledge Group Function)
    if (msg.PGN == 126208) {
        decoded.messageName = "Group Function (126208)";
        decoded.description = "NMEA2000 Group Function Command/Request/Ack";
        decoded.isDecoded = true;

        // Minimum length check - Group Function needs at least 6 bytes
        // (Function Code + PGN[3] + Priority + Number of Parameters)
        if (msg.DataLen < 6) {
            DecodedSignal sig;
            sig.name = "Error";
            sig.value = "Too short for 126208 decode (minimum 6 bytes)";
            sig.isValid = true;
            decoded.signalList.append(sig);
            return decoded;
        }

        // Byte 0: Function Code
        uint8_t functionCode = msg.Data[0];
        // Bytes 1-3: Target PGN (little endian)
        uint32_t targetPGN = msg.Data[1] | (msg.Data[2] << 8) | (msg.Data[3] << 16);
        // Byte 4: Priority
        uint8_t priority = msg.Data[4];
        // Byte 5: Number of parameters
        uint8_t numParams = msg.Data[5];

        DecodedSignal sigFC;
        sigFC.name = "Function Code";
        QString functionCodeDescription;
        switch (functionCode) {
            case 0: functionCodeDescription = "Request"; break;
            case 1: functionCodeDescription = "Command"; break;
            case 2: functionCodeDescription = "Acknowledge"; break;
            case 3: functionCodeDescription = "Read Response"; break;
            case 4: functionCodeDescription = "Read Request"; break;
            case 5: functionCodeDescription = "Write Request"; break;
            case 6: functionCodeDescription = "Write Response"; break;
            default: functionCodeDescription = QString("Unknown (%1)").arg(functionCode); break;
        }
        sigFC.value = QString("%1 (%2)").arg(functionCode).arg(functionCodeDescription);
        sigFC.isValid = true;
        decoded.signalList.append(sigFC);

        DecodedSignal sigPGN;
        sigPGN.name = "Target PGN";
        sigPGN.value = QString("%1").arg(targetPGN);
        sigPGN.isValid = true;
        decoded.signalList.append(sigPGN);

        DecodedSignal sigPrio;
        sigPrio.name = "Priority";
        sigPrio.value = priority;
        sigPrio.isValid = true;
        decoded.signalList.append(sigPrio);

        DecodedSignal sigNumParams;
        sigNumParams.name = "Number of Parameters";
        sigNumParams.value = numParams;
        sigNumParams.isValid = true;
        decoded.signalList.append(sigNumParams);

        // Each parameter: Byte 6 + 2*i: Field Number, Byte 7 + 2*i: Value
        int paramStart = 6;
        for (uint8_t i = 0; i < numParams; ++i) {
            int fieldIdx = paramStart + i * 2;
            if (fieldIdx + 1 >= msg.DataLen) break;
            uint8_t fieldNum = msg.Data[fieldIdx];
            uint8_t fieldVal = msg.Data[fieldIdx + 1];
            DecodedSignal sigField;
            sigField.name = QString("Field %1").arg(fieldNum);
            sigField.value = fieldVal;
            sigField.isValid = true;
            decoded.signalList.append(sigField);
        }
        return decoded;
    }

    // Special handling for lighting PGNs
    if (msg.PGN == 130330) {
        return decodePGN130330(msg);
    }
    if (msg.PGN == 130561) {
        return decodePGN130561(msg);
    }
    if (msg.PGN == 130562) {
        return decodePGN130562(msg);
    }
    if (msg.PGN == 130563) {
        return decodePGN130563(msg);
    }
    if (msg.PGN == 130564) {
        return decodePGN130564(msg);
    }
    if (msg.PGN == 130565) {
        return decodePGN130565(msg);
    }
    if (msg.PGN == 130566) {
        return decodePGN130566(msg);
    }

    if (!m_messages.contains(msg.PGN)) {
        return decoded;
    }

    const DBCMessage& dbcMsg = m_messages[msg.PGN];
    decoded.messageName = dbcMsg.name;
    decoded.description = dbcMsg.description;
    decoded.isDecoded = true;

    // Decode each signal
    for (const DBCSignal& signal : dbcMsg.signalList) {
        DecodedSignal decodedSignal;
        decodedSignal.name = signal.name;
        decodedSignal.unit = signal.unit;
        decodedSignal.description = signal.description;

        double rawValue = extractSignalValue(msg.Data, signal);
        decodedSignal.isValid = isSignalValid(rawValue, signal);

        if (decodedSignal.isValid) {
            double scaledValue = rawValue * signal.scale + signal.offset;

            // Special handling for temperature (convert from Kelvin to Celsius)
            if (signal.unit == "°C" && scaledValue > 100) {
                // Assume raw value is in 0.01K units, convert to Celsius
                scaledValue = (rawValue * 0.01) - 273.15;
            }

            // Check for enumerated values
            if (!signal.valueDescriptions.isEmpty() && signal.valueDescriptions.contains((int)rawValue)) {
                decodedSignal.value = signal.valueDescriptions[(int)rawValue];
            } else {
                decodedSignal.value = scaledValue;
            }
        } else {
            decodedSignal.value = "N/A";
        }

        decoded.signalList.append(decodedSignal);
    }

    return decoded;
}

double DBCDecoder::extractSignalValue(const uint8_t* data, const DBCSignal& signal)
{
    uint64_t rawValue = 0;
    
    // Simple byte-aligned extraction for most NMEA2000 signals
    if (signal.startBit % 8 == 0 && signal.bitLength % 8 == 0) {
        // Byte-aligned signal
        int startByte = signal.startBit / 8;
        int numBytes = signal.bitLength / 8;
        
        for (int i = 0; i < numBytes && (startByte + i) < 8; i++) {
            rawValue |= ((uint64_t)data[startByte + i]) << (i * 8);
        }
    } else {
        // Bit-level extraction for non-byte-aligned signals
        int startByte = signal.startBit / 8;
        int startBitInByte = signal.startBit % 8;
        int remainingBits = signal.bitLength;
        int bitPosition = 0;
        
        for (int i = startByte; i < 8 && remainingBits > 0; i++) {
            int bitsToRead = qMin(8 - (i == startByte ? startBitInByte : 0), remainingBits);
            int shift = i == startByte ? startBitInByte : 0;
            uint8_t mask = ((1 << bitsToRead) - 1);
            uint8_t maskedByte = (data[i] >> shift) & mask;
            
            rawValue |= ((uint64_t)maskedByte) << bitPosition;
            bitPosition += bitsToRead;
            remainingBits -= bitsToRead;
        }
    }
    
    // Handle signed values
    if (signal.isSigned && signal.bitLength < 64) {
        uint64_t signBit = 1ULL << (signal.bitLength - 1);
        if (rawValue & signBit) {
            // Sign extend
            uint64_t mask = ~((1ULL << signal.bitLength) - 1);
            rawValue |= mask;
        }
    }
    
    return (double)rawValue;
}

bool DBCDecoder::isSignalValid(double rawValue, const DBCSignal& signal)
{
    // Check for NMEA2000 "not available" values
    if (signal.bitLength == 8 && rawValue >= 250) return false;
    if (signal.bitLength == 16 && rawValue >= 65530) return false;
    if (signal.bitLength == 32 && rawValue >= 4294967290ULL) return false;
    
    return true;
}

bool DBCDecoder::canDecode(unsigned long pgn) const
{
    // Special handling for PGN 126208 (Group Function)
    if (pgn == 126208) {
        return true;
    }
    
    // Special handling for lighting PGNs
    if (pgn == 130330 || pgn == 130561 || pgn == 130562 || pgn == 130563 || pgn == 130564 || pgn == 130565 || pgn == 130566) {
        return true;
    }
    
    return m_messages.contains(pgn);
}

QString DBCDecoder::getMessageName(unsigned long pgn) const
{
    // Special handling for PGN 126208 (Group Function)
    if (pgn == 126208) {
        return "Group Function";
    }
    
    // Special handling for lighting PGNs
    if (pgn == 130330) {
        return "Lighting System Settings";
    }
    if (pgn == 130561) {
        return "Zone Lighting Control";
    }
    if (pgn == 130563) {
        return "Lighting Device";
    }
    if (pgn == 130564) {
        return "Lighting Device Enumeration";
    }
    if (pgn == 130565) {
        return "Lighting Color Sequence";
    }
    if (pgn == 130566) {
        return "Lighting Program";
    }
    
    if (m_messages.contains(pgn)) {
        return m_messages[pgn].name;
    }
    
    // Check if this is a proprietary PGN
    if ((pgn >= 65280 && pgn <= 65535) ||          // 0xFF00-0xFFFF: Single-frame proprietary
        (pgn >= 126720 && pgn <= 126975) ||        // 0x1EF00-0x1EFFF: Multi-frame proprietary
        (pgn >= 127744 && pgn <= 128511)) {         // 0x1F300-0x1F5FF: Additional proprietary
        return QString("Proprietary %1").arg(pgn);
    }
    
    return QString("PGN %1").arg(pgn);
}

QString DBCDecoder::getCleanMessageName(unsigned long pgn) const
{
    QString name = getMessageName(pgn);
    
    // Enhanced formatting inspired by cantools experience
    
    // Remove redundant "PGN" prefix if present
    if (name.startsWith("PGN ")) {
        name = name.mid(4);
    }
    
    // Clean up common prefixes/suffixes
    if (name.startsWith("NMEA2000_")) {
        name = name.mid(9);
    }
    
    // Convert underscores to spaces for better readability  
    name = name.replace('_', ' ');
    
    // Capitalize first letter of each word
    QStringList words = name.split(' ', Qt::SkipEmptyParts);
    for (int i = 0; i < words.length(); i++) {
        if (!words[i].isEmpty()) {
            words[i][0] = words[i][0].toUpper();
        }
    }
    name = words.join(' ');
    
    // Add PGN number for unknown messages in a cleaner format
    if (name.startsWith("PGN ") || name.contains("Unknown") || name.contains("Proprietary")) {
        name = QString("%1 (PGN %2)").arg(name).arg(pgn);
    }
    
    return name;
}

QString DBCDecoder::getFormattedDecoded(const tN2kMsg& msg)
{
    DecodedMessage decoded = decodeMessage(msg);
    
    if (!decoded.isDecoded) {
        return "Raw data";
    }
    
    QStringList parts;
    for (const DecodedSignal& signal : decoded.signalList) {
        if (signal.isValid && signal.value.toString() != "N/A") {
            QString part = QString("%1: %2").arg(signal.name);
            
            if (signal.value.type() == QVariant::Double) {
                part = part.arg(signal.value.toDouble(), 0, 'f', 2);
            } else {
                part = part.arg(signal.value.toString());
            }
            
            if (!signal.unit.isEmpty()) {
                part += " " + signal.unit;
            }
            
            parts.append(part);
        }
    }
    
    return parts.join(", ");
}

QString DBCDecoder::getFormattedDecodedForSave(const tN2kMsg& msg)
{
    DecodedMessage decoded = decodeMessage(msg);
    
    if (!decoded.isDecoded) {
        return "Raw data";
    }
    
    QStringList parts;
    for (const DecodedSignal& signal : decoded.signalList) {
        // Skip reserved fields when saving
        if (signal.name.contains("Reserved", Qt::CaseInsensitive)) {
            continue;
        }
        
        if (signal.isValid && signal.value.toString() != "N/A") {
            QString part = QString("%1: %2").arg(signal.name);
            
            if (signal.value.type() == QVariant::Double) {
                part = part.arg(signal.value.toDouble(), 0, 'f', 2);
            } else {
                part = part.arg(signal.value.toString());
            }
            
            if (!signal.unit.isEmpty()) {
                part += " " + signal.unit;
            }
            
            parts.append(part);
        }
    }
    
    return parts.join(", ");
}

bool DBCDecoder::isInitialized() const
{
    // Decoder is considered initialized if we have message definitions loaded
    return m_messages.count() > 0;
}

QString DBCDecoder::getDecoderInfo() const
{
    QString info = QString("DBC Decoder Status:\n");
    info += QString("- Messages loaded: %1\n").arg(m_messages.count());
    info += QString("- Decoder type: Original/Fast C++\n");
    
    if (m_messages.count() > 0) {
        QStringList samplePGNs;
        auto it = m_messages.constBegin();
        for (int i = 0; i < qMin(5, m_messages.count()) && it != m_messages.constEnd(); ++it, ++i) {
            samplePGNs.append(QString("%1 (%2)").arg(it.value().name).arg(it.key()));
        }
        info += QString("- Sample messages: %1\n").arg(samplePGNs.join(", "));
    }
    
    return info;
}

QString DBCDecoder::formatSignalValue(const DecodedSignal& signal)
{
    if (!signal.isValid || signal.value.toString() == "N/A") {
        return "N/A";
    }
    
    if (signal.value.type() == QVariant::Double) {
        return QString("%1 %2").arg(signal.value.toDouble(), 0, 'f', 2).arg(signal.unit);
    }
    
    return signal.value.toString();
}

QString DBCDecoder::decodePGN(const tN2kMsg& msg)
{
    unsigned long pgn = msg.PGN;
    QString decoded;
    
    // First try to find in loaded DBC messages
    for (const DBCMessage& message : m_messages) {
        if (message.pgn == pgn) {
            DecodedMessage decodedMsg = decodeMessage(msg);
            if (!decodedMsg.messageName.isEmpty()) {
                return getFormattedDecoded(msg);
            }
        }
    }
    
    // If no DBC decoder found, return basic information
    if (decoded.isEmpty()) {
        decoded = QString("PGN %1 (no decoder available)").arg(pgn);
    }
    
    return decoded;
}

// Lighting PGN Decoders

DecodedMessage DBCDecoder::decodePGN130561(const tN2kMsg& msg)
{
    DecodedMessage decoded;
    decoded.messageName = "Zone Lighting Control (130561)";
    decoded.description = "NMEA2000 Zone Lighting Control Message";
    decoded.isDecoded = true;

    if (msg.DataLen < 1) {
        DecodedSignal sig;
        sig.name = "Error";
        sig.value = "Message too short for PGN 130561";
        decoded.signalList.append(sig);
        return decoded;
    }

    int index = 0;

    // Byte 0: Zone ID (0-252, 253=Broadcast, 254=No Zone, 255=NULL)
    uint8_t zoneId = msg.GetByte(index);
    DecodedSignal sigZoneId;
    sigZoneId.name = "Zone ID";
    sigZoneId.isValid = true;
    if (zoneId == 253) {
        sigZoneId.value = QString("%1 (Broadcast)").arg(zoneId);
    } else if (zoneId == 254) {
        sigZoneId.value = QString("%1 (No Zone)").arg(zoneId);
    } else if (zoneId == 255) {
        sigZoneId.value = QString("%1 (NULL/Not Available)").arg(zoneId);
    } else {
        sigZoneId.value = zoneId;
    }
    decoded.signalList.append(sigZoneId);

    // Byte 1: Control Type/Command
    if (index < msg.DataLen) {
        uint8_t controlType = msg.GetByte(index);
        DecodedSignal sigControlType;
        sigControlType.name = "Control Type";
        sigControlType.isValid = true;
        QString controlTypeName;
        switch (controlType) {
            case 0: controlTypeName = "Off"; break;
            case 1: controlTypeName = "Set Color/Intensity"; break;
            case 2: controlTypeName = "Dim Up"; break;
            case 3: controlTypeName = "Dim Down"; break;
            case 4: controlTypeName = "Toggle"; break;
            case 5: controlTypeName = "Flash"; break;
            case 6: controlTypeName = "Sequence"; break;
            case 7: controlTypeName = "Program"; break;
            case 255: controlTypeName = "Not Available"; break;
            default: controlTypeName = QString("Reserved (%1)").arg(controlType); break;
        }
        sigControlType.value = QString("%1 (%2)").arg(controlTypeName).arg(controlType);
        decoded.signalList.append(sigControlType);
    }

    // Byte 2: Red Component (0-255)
    if (index < msg.DataLen) {
        uint8_t red = msg.GetByte(index);
        DecodedSignal sigRed;
        sigRed.name = "Red Component";
        sigRed.isValid = true;
        if (red == 255) {
            sigRed.value = "Not Available";
        } else {
            sigRed.value = QString("%1 (%2%)").arg(red).arg((red * 100.0 / 255.0), 0, 'f', 1);
        }
        decoded.signalList.append(sigRed);
    }

    // Byte 3: Green Component (0-255)
    if (index < msg.DataLen) {
        uint8_t green = msg.GetByte(index);
        DecodedSignal sigGreen;
        sigGreen.name = "Green Component";
        sigGreen.isValid = true;
        if (green == 255) {
            sigGreen.value = "Not Available";
        } else {
            sigGreen.value = QString("%1 (%2%)").arg(green).arg((green * 100.0 / 255.0), 0, 'f', 1);
        }
        decoded.signalList.append(sigGreen);
    }

    // Byte 4: Blue Component (0-255)
    if (index < msg.DataLen) {
        uint8_t blue = msg.GetByte(index);
        DecodedSignal sigBlue;
        sigBlue.name = "Blue Component";
        sigBlue.isValid = true;
        if (blue == 255) {
            sigBlue.value = "Not Available";
        } else {
            sigBlue.value = QString("%1 (%2%)").arg(blue).arg((blue * 100.0 / 255.0), 0, 'f', 1);
        }
        decoded.signalList.append(sigBlue);
    }

    // Bytes 5-6: Color Temperature (0-65532 Kelvin, 65535=N/A)
    if (index + 1 < msg.DataLen) {
        uint16_t colorTemp = msg.Get2ByteUInt(index);
        DecodedSignal sigColorTemp;
        sigColorTemp.name = "Color Temperature";
        sigColorTemp.isValid = true;
        if (colorTemp == 65535) {
            sigColorTemp.value = "Not Available";
        } else if (colorTemp == 65534) {
            sigColorTemp.value = "Error";
        } else {
            sigColorTemp.value = QString("%1 K").arg(colorTemp);
        }
        decoded.signalList.append(sigColorTemp);
    }

    // Byte 7: Intensity/Brightness (0-200 * 0.5%, 255=N/A)
    if (index < msg.DataLen) {
        uint8_t intensity = msg.GetByte(index);
        DecodedSignal sigIntensity;
        sigIntensity.name = "Intensity";
        sigIntensity.isValid = true;
        if (intensity == 255) {
            sigIntensity.value = "Not Available";
        } else if (intensity == 254) {
            sigIntensity.value = "Error";
        } else {
            double percentage = intensity * 0.5;
            sigIntensity.value = QString("%1% (%2/200)").arg(percentage, 0, 'f', 1).arg(intensity);
        }
        decoded.signalList.append(sigIntensity);
    }

    // Byte 8: Transition Time (0-254 seconds, 255=N/A)
    if (index < msg.DataLen) {
        uint8_t transitionTime = msg.GetByte(index);
        DecodedSignal sigTransitionTime;
        sigTransitionTime.name = "Transition Time";
        sigTransitionTime.isValid = true;
        if (transitionTime == 255) {
            sigTransitionTime.value = "Not Available";
        } else {
            sigTransitionTime.value = QString("%1 seconds").arg(transitionTime);
        }
        decoded.signalList.append(sigTransitionTime);
    }

    // Additional bytes as reserved/manufacturer specific
    while (index < msg.DataLen) {
        static int paramNum = 1;
        DecodedSignal sigReserved;
        sigReserved.name = QString("Reserved %1").arg(paramNum++);
        sigReserved.value = QString("0x%1").arg(msg.GetByte(index), 2, 16, QChar('0')).toUpper();
        decoded.signalList.append(sigReserved);
    }

    return decoded;
}

DecodedMessage DBCDecoder::decodePGN130562(const tN2kMsg& msg)
{
    DecodedMessage decoded;
    decoded.messageName = "Lighting Scene (130562)";
    decoded.description = "NMEA2000 Lighting Scene Message";
    decoded.isDecoded = true;

    if (msg.DataLen < 1) {
        DecodedSignal sig;
        sig.name = "Error";
        sig.value = "Message too short for PGN 130562";
        decoded.signalList.append(sig);
        return decoded;
    }

    int index = 0;

    // Byte 0: Scene ID (0-252, 253=All Scenes, 254=Current Scene, 255=N/A)
    uint8_t sceneId = msg.GetByte(index);
    DecodedSignal sigSceneId;
    sigSceneId.name = "Scene ID";
    sigSceneId.isValid = true;
    if (sceneId == 253) {
        sigSceneId.value = QString("%1 (All Scenes)").arg(sceneId);
    } else if (sceneId == 254) {
        sigSceneId.value = QString("%1 (Current Scene)").arg(sceneId);
    } else if (sceneId == 255) {
        sigSceneId.value = QString("%1 (Not Available)").arg(sceneId);
    } else {
        sigSceneId.value = sceneId;
    }
    decoded.signalList.append(sigSceneId);

    // Byte 1: Scene Command
    if (index < msg.DataLen) {
        uint8_t sceneCommand = msg.GetByte(index);
        DecodedSignal sigSceneCommand;
        sigSceneCommand.name = "Scene Command";
        sigSceneCommand.isValid = true;
        QString commandName;
        switch (sceneCommand) {
            case 0: commandName = "No Action"; break;
            case 1: commandName = "Activate Scene"; break;
            case 2: commandName = "Store Current as Scene"; break;
            case 3: commandName = "Delete Scene"; break;
            case 4: commandName = "Fade to Scene"; break;
            case 5: commandName = "Recall Scene"; break;
            case 255: commandName = "Not Available"; break;
            default: commandName = QString("Reserved (%1)").arg(sceneCommand); break;
        }
        sigSceneCommand.value = QString("%1 (%2)").arg(commandName).arg(sceneCommand);
        decoded.signalList.append(sigSceneCommand);
    }

    // Byte 2: Scene Name Length
    if (index < msg.DataLen) {
        uint8_t nameLen = msg.GetByte(index);
        DecodedSignal sigNameLen;
        sigNameLen.name = "Scene Name Length";
        sigNameLen.value = nameLen;
        decoded.signalList.append(sigNameLen);

        // Scene Name (variable length string)
        QString sceneName;
        for (int i = 0; i < nameLen && index < msg.DataLen; i++) {
            char c = msg.GetByte(index);
            if (c != 0) sceneName += c;
        }
        if (!sceneName.isEmpty()) {
            DecodedSignal sigSceneName;
            sigSceneName.name = "Scene Name";
            sigSceneName.value = sceneName;
            decoded.signalList.append(sigSceneName);
        }
    }

    // Byte N: Transition Time (0-254 seconds, 255=N/A)
    if (index < msg.DataLen) {
        uint8_t transitionTime = msg.GetByte(index);
        DecodedSignal sigTransitionTime;
        sigTransitionTime.name = "Transition Time";
        if (transitionTime == 255) {
            sigTransitionTime.value = "Not Available";
        } else {
            sigTransitionTime.value = QString("%1 seconds").arg(transitionTime);
        }
        decoded.signalList.append(sigTransitionTime);
    }

    // Additional bytes as reserved/manufacturer specific
    while (index < msg.DataLen) {
        static int paramNum = 1;
        DecodedSignal sigReserved;
        sigReserved.name = QString("Reserved %1").arg(paramNum++);
        sigReserved.value = QString("0x%1").arg(msg.GetByte(index), 2, 16, QChar('0')).toUpper();
        decoded.signalList.append(sigReserved);
    }

    return decoded;
}

DecodedMessage DBCDecoder::decodePGN130563(const tN2kMsg& msg)
{
    DecodedMessage decoded;
    decoded.messageName = "Lighting Device (130563)";
    decoded.description = "NMEA2000 Lighting Device Status Message";
    decoded.isDecoded = true;

    if (msg.DataLen < 3) {
        DecodedSignal sig;
        sig.name = "Error";
        sig.value = "Message too short for PGN 130563";
        decoded.signalList.append(sig);
        return decoded;
    }

    int index = 0;

    // Byte 0: Device Instance
    DecodedSignal sigDeviceInstance;
    sigDeviceInstance.name = "Device Instance";
    uint8_t deviceInstance = msg.GetByte(index);
    if (deviceInstance == 0xFF) {
        sigDeviceInstance.value = "NULL/Unknown";
    } else {
        sigDeviceInstance.value = QString::number(deviceInstance);
    }
    sigDeviceInstance.isValid = true;
    decoded.signalList.append(sigDeviceInstance);

    // Byte 1: Device Type
    DecodedSignal sigDeviceType;
    sigDeviceType.name = "Device Type";
    uint8_t deviceType = msg.GetByte(index);
    QString deviceTypeName;
    switch (deviceType) {
        case 0: deviceTypeName = "Not Specified"; break;
        case 1: deviceTypeName = "Navigation Light"; break;
        case 2: deviceTypeName = "Anchor Light"; break;
        case 3: deviceTypeName = "Strobe Light"; break;
        case 4: deviceTypeName = "Deck Light"; break;
        case 5: deviceTypeName = "Cabin Light"; break;
        case 6: deviceTypeName = "Underwater Light"; break;
        case 7: deviceTypeName = "Search Light"; break;
        case 8: deviceTypeName = "RGB Light"; break;
        case 9: deviceTypeName = "White Light"; break;
        case 10: deviceTypeName = "Warning Light"; break;
        case 255: deviceTypeName = "NULL"; break;
        default: deviceTypeName = QString("Reserved (%1)").arg(deviceType); break;
    }
    sigDeviceType.value = deviceTypeName;
    sigDeviceType.isValid = true;
    decoded.signalList.append(sigDeviceType);

    // Byte 2: Device Status
    DecodedSignal sigDeviceStatus;
    sigDeviceStatus.name = "Device Status";
    uint8_t deviceStatus = msg.GetByte(index);
    QString statusName;
    switch (deviceStatus & 0x0F) {
        case 0: statusName = "Off"; break;
        case 1: statusName = "On"; break;
        case 2: statusName = "Error"; break;
        case 3: statusName = "Fault"; break;
        case 4: statusName = "Disabled"; break;
        case 15: statusName = "NULL"; break;
        default: statusName = QString("Reserved (%1)").arg(deviceStatus & 0x0F); break;
    }
    if ((deviceStatus & 0x30) >> 4 != 0) {
        statusName += QString(" | Fault Code: %1").arg((deviceStatus & 0x30) >> 4);
    }
    sigDeviceStatus.value = statusName;
    sigDeviceStatus.isValid = true;
    decoded.signalList.append(sigDeviceStatus);

    // If more data available, decode additional parameters
    if (index < msg.DataLen) {
        // Byte 3+: Brightness/Intensity (if available)
        if (index < msg.DataLen) {
            DecodedSignal sigBrightness;
            sigBrightness.name = "Brightness";
            uint8_t brightness = msg.GetByte(index);
            if (brightness == 0xFF) {
                sigBrightness.value = "NULL/Unknown";
            } else {
                sigBrightness.value = QString("%1%").arg((brightness * 100) / 255);
            }
            sigBrightness.isValid = true;
            decoded.signalList.append(sigBrightness);
        }

        // Remaining bytes as device-specific data
        if (index < msg.DataLen) {
            QString remainingData;
            while (index < msg.DataLen) {
                remainingData += QString(" %1").arg(msg.GetByte(index), 2, 16, QChar('0')).toUpper();
            }
            DecodedSignal sigDeviceData;
            sigDeviceData.name = "Device Specific Data";
            sigDeviceData.value = remainingData.trimmed();
            sigDeviceData.isValid = true;
            decoded.signalList.append(sigDeviceData);
        }
    }

    return decoded;
}

DecodedMessage DBCDecoder::decodePGN130564(const tN2kMsg& msg)
{
    DecodedMessage decoded;
    decoded.messageName = "Lighting Device Enumeration (130564)";
    decoded.description = "NMEA2000 Lighting Device Enumeration Message";
    decoded.isDecoded = true;

    if (msg.DataLen < 2) {
        DecodedSignal sig;
        sig.name = "Error";
        sig.value = "Message too short for PGN 130564";
        decoded.signalList.append(sig);
        return decoded;
    }

    int index = 0;

    // Byte 0: Command/Request Type
    DecodedSignal sigCommand;
    sigCommand.name = "Command";
    uint8_t command = msg.GetByte(index);
    QString commandName;
    switch (command) {
        case 0: commandName = "Enumerate Request"; break;
        case 1: commandName = "Enumerate Response"; break;
        case 2: commandName = "Get Device Info"; break;
        case 3: commandName = "Device Info Response"; break;
        case 255: commandName = "NULL"; break;
        default: commandName = QString("Reserved (%1)").arg(command); break;
    }
    sigCommand.value = commandName;
    sigCommand.isValid = true;
    decoded.signalList.append(sigCommand);

    // Byte 1: Device Count (for enumerate response) or Device Instance (for device info)
    DecodedSignal sigCount;
    uint8_t countOrInstance = msg.GetByte(index);
    if (command == 1) { // Enumerate Response
        sigCount.name = "Device Count";
        if (countOrInstance == 0xFF) {
            sigCount.value = "NULL/Unknown";
        } else {
            sigCount.value = QString::number(countOrInstance);
        }
    } else {
        sigCount.name = "Device Instance";
        if (countOrInstance == 0xFF) {
            sigCount.value = "NULL/Unknown";
        } else {
            sigCount.value = QString::number(countOrInstance);
        }
    }
    sigCount.isValid = true;
    decoded.signalList.append(sigCount);

    // Remaining bytes: Device enumeration data
    if (command == 1 && index < msg.DataLen) { // Enumerate Response
        int deviceNum = 1;
        while (index + 1 < msg.DataLen) {
            uint8_t deviceInstance = msg.GetByte(index);
            uint8_t deviceType = msg.GetByte(index);
            
            DecodedSignal sigDevice;
            sigDevice.name = QString("Device %1").arg(deviceNum++);
            
            QString deviceTypeName;
            switch (deviceType) {
                case 0: deviceTypeName = "Not Specified"; break;
                case 1: deviceTypeName = "Navigation Light"; break;
                case 2: deviceTypeName = "Anchor Light"; break;
                case 3: deviceTypeName = "Strobe Light"; break;
                case 4: deviceTypeName = "Deck Light"; break;
                case 5: deviceTypeName = "Cabin Light"; break;
                case 6: deviceTypeName = "Underwater Light"; break;
                case 7: deviceTypeName = "Search Light"; break;
                case 8: deviceTypeName = "RGB Light"; break;
                case 9: deviceTypeName = "White Light"; break;
                case 10: deviceTypeName = "Warning Light"; break;
                case 255: deviceTypeName = "NULL"; break;
                default: deviceTypeName = QString("Reserved (%1)").arg(deviceType); break;
            }
            
            sigDevice.value = QString("Instance %1: %2").arg(deviceInstance).arg(deviceTypeName);
            sigDevice.isValid = true;
            decoded.signalList.append(sigDevice);
        }
    } else if (index < msg.DataLen) {
        // Additional data as hex
        QString remainingData;
        while (index < msg.DataLen) {
            remainingData += QString(" %1").arg(msg.GetByte(index), 2, 16, QChar('0')).toUpper();
        }
        DecodedSignal sigData;
        sigData.name = "Additional Data";
        sigData.value = remainingData.trimmed();
        sigData.isValid = true;
        decoded.signalList.append(sigData);
    }

    return decoded;
}

DecodedMessage DBCDecoder::decodePGN130565(const tN2kMsg& msg)
{
    DecodedMessage decoded;
    decoded.messageName = "Lighting Color Sequence (130565)";
    decoded.description = "NMEA2000 Lighting Color Sequence Message";
    decoded.isDecoded = true;

    if (msg.DataLen < 3) {
        DecodedSignal sig;
        sig.name = "Error";
        sig.value = "Message too short for PGN 130565";
        decoded.signalList.append(sig);
        return decoded;
    }

    int index = 0;

    // Byte 0: Sequence ID
    DecodedSignal sigSeqId;
    sigSeqId.name = "Sequence ID";
    uint8_t seqId = msg.GetByte(index);
    if (seqId == 0xFF) {
        sigSeqId.value = "NULL/All Sequences";
    } else {
        sigSeqId.value = QString::number(seqId);
    }
    sigSeqId.isValid = true;
    decoded.signalList.append(sigSeqId);

    // Byte 1: Sequence Command
    DecodedSignal sigCommand;
    sigCommand.name = "Command";
    uint8_t command = msg.GetByte(index);
    QString commandName;
    switch (command) {
        case 0: commandName = "Stop Sequence"; break;
        case 1: commandName = "Start Sequence"; break;
        case 2: commandName = "Pause Sequence"; break;
        case 3: commandName = "Resume Sequence"; break;
        case 4: commandName = "Define Sequence"; break;
        case 5: commandName = "Delete Sequence"; break;
        case 255: commandName = "NULL"; break;
        default: commandName = QString("Reserved (%1)").arg(command); break;
    }
    sigCommand.value = commandName;
    sigCommand.isValid = true;
    decoded.signalList.append(sigCommand);

    // Byte 2: Number of colors/steps in sequence (for define command)
    if (command == 4 && index < msg.DataLen) { // Define Sequence
        DecodedSignal sigColorCount;
        sigColorCount.name = "Color Count";
        uint8_t colorCount = msg.GetByte(index);
        if (colorCount == 0xFF) {
            sigColorCount.value = "NULL";
        } else {
            sigColorCount.value = QString::number(colorCount);
        }
        sigColorCount.isValid = true;
        decoded.signalList.append(sigColorCount);

        // Decode color entries (RGB triplets + timing)
        int colorIndex = 1;
        while (index + 3 < msg.DataLen) {
            uint8_t red = msg.GetByte(index);
            uint8_t green = msg.GetByte(index);
            uint8_t blue = msg.GetByte(index);
            uint8_t duration = msg.GetByte(index); // Duration in 0.1 second units
            
            DecodedSignal sigColor;
            sigColor.name = QString("Color %1").arg(colorIndex++);
            QString durationStr = (duration == 0xFF) ? "NULL" : QString("%1s").arg(duration * 0.1, 0, 'f', 1);
            sigColor.value = QString("RGB(%1,%2,%3) Duration: %4").arg(red).arg(green).arg(blue).arg(durationStr);
            sigColor.isValid = true;
            decoded.signalList.append(sigColor);
        }
    } else {
        // For other commands, remaining bytes might contain timing or other parameters
        if (index < msg.DataLen) {
            DecodedSignal sigParam;
            sigParam.name = "Sequence Parameter";
            uint8_t param = msg.GetByte(index);
            if (param == 0xFF) {
                sigParam.value = "NULL";
            } else {
                sigParam.value = QString::number(param);
            }
            sigParam.isValid = true;
            decoded.signalList.append(sigParam);
        }
    }

    // Any remaining bytes as additional data
    if (index < msg.DataLen) {
        QString remainingData;
        while (index < msg.DataLen) {
            remainingData += QString(" %1").arg(msg.GetByte(index), 2, 16, QChar('0')).toUpper();
        }
        DecodedSignal sigData;
        sigData.name = "Additional Data";
        sigData.value = remainingData.trimmed();
        sigData.isValid = true;
        decoded.signalList.append(sigData);
    }

    return decoded;
}

DecodedMessage DBCDecoder::decodePGN130566(const tN2kMsg& msg)
{
    DecodedMessage decoded;
    decoded.messageName = "Lighting Program (130566)";
    decoded.description = "NMEA2000 Lighting Program Message";
    decoded.isDecoded = true;

    if (msg.DataLen < 3) {
        DecodedSignal sig;
        sig.name = "Error";
        sig.value = "Message too short for PGN 130566";
        decoded.signalList.append(sig);
        return decoded;
    }

    int index = 0;

    // Byte 0: Program ID
    DecodedSignal sigProgId;
    sigProgId.name = "Program ID";
    uint8_t progId = msg.GetByte(index);
    if (progId == 0xFF) {
        sigProgId.value = "NULL/All Programs";
    } else {
        sigProgId.value = QString::number(progId);
    }
    sigProgId.isValid = true;
    decoded.signalList.append(sigProgId);

    // Byte 1: Program Command
    DecodedSignal sigCommand;
    sigCommand.name = "Command";
    uint8_t command = msg.GetByte(index);
    QString commandName;
    switch (command) {
        case 0: commandName = "Stop Program"; break;
        case 1: commandName = "Start Program"; break;
        case 2: commandName = "Pause Program"; break;
        case 3: commandName = "Resume Program"; break;
        case 4: commandName = "Define Program"; break;
        case 5: commandName = "Delete Program"; break;
        case 6: commandName = "Get Program Info"; break;
        case 7: commandName = "Program Info Response"; break;
        case 255: commandName = "NULL"; break;
        default: commandName = QString("Reserved (%1)").arg(command); break;
    }
    sigCommand.value = commandName;
    sigCommand.isValid = true;
    decoded.signalList.append(sigCommand);

    // Byte 2: Program Type/Mode
    DecodedSignal sigProgType;
    sigProgType.name = "Program Type";
    uint8_t progType = msg.GetByte(index);
    QString progTypeName;
    switch (progType) {
        case 0: progTypeName = "Off"; break;
        case 1: progTypeName = "Static Color"; break;
        case 2: progTypeName = "Fade"; break;
        case 3: progTypeName = "Flash/Strobe"; break;
        case 4: progTypeName = "Color Sequence"; break;
        case 5: progTypeName = "Random"; break;
        case 6: progTypeName = "Pulse"; break;
        case 7: progTypeName = "Breathing"; break;
        case 8: progTypeName = "Rainbow"; break;
        case 255: progTypeName = "NULL"; break;
        default: progTypeName = QString("Reserved (%1)").arg(progType); break;
    }
    sigProgType.value = progTypeName;
    sigProgType.isValid = true;
    decoded.signalList.append(sigProgType);

    // Decode program-specific parameters based on type
    if (index < msg.DataLen) {
        if (progType == 1) { // Static Color
            if (index + 2 < msg.DataLen) {
                uint8_t red = msg.GetByte(index);
                uint8_t green = msg.GetByte(index);
                uint8_t blue = msg.GetByte(index);
                
                DecodedSignal sigColor;
                sigColor.name = "Static Color";
                sigColor.value = QString("RGB(%1,%2,%3)").arg(red).arg(green).arg(blue);
                sigColor.isValid = true;
                decoded.signalList.append(sigColor);
            }
        } else if (progType == 2 || progType == 3 || progType == 6 || progType == 7) { // Fade, Flash, Pulse, Breathing
            if (index < msg.DataLen) {
                DecodedSignal sigSpeed;
                sigSpeed.name = "Speed/Rate";
                uint8_t speed = msg.GetByte(index);
                if (speed == 0xFF) {
                    sigSpeed.value = "NULL";
                } else {
                    sigSpeed.value = QString("%1 (0.1Hz units)").arg(speed * 0.1, 0, 'f', 1);
                }
                sigSpeed.isValid = true;
                decoded.signalList.append(sigSpeed);
            }
            
            if (index < msg.DataLen) {
                DecodedSignal sigIntensity;
                sigIntensity.name = "Intensity";
                uint8_t intensity = msg.GetByte(index);
                if (intensity == 0xFF) {
                    sigIntensity.value = "NULL";
                } else {
                    sigIntensity.value = QString("%1%").arg((intensity * 100) / 255);
                }
                sigIntensity.isValid = true;
                decoded.signalList.append(sigIntensity);
            }
        } else if (progType == 4) { // Color Sequence
            if (index < msg.DataLen) {
                DecodedSignal sigSeqRef;
                sigSeqRef.name = "Sequence Reference";
                uint8_t seqRef = msg.GetByte(index);
                if (seqRef == 0xFF) {
                    sigSeqRef.value = "NULL";
                } else {
                    sigSeqRef.value = QString::number(seqRef);
                }
                sigSeqRef.isValid = true;
                decoded.signalList.append(sigSeqRef);
            }
        }
    }

    // Any remaining bytes as additional parameters
    int paramNum = 1;
    while (index < msg.DataLen) {
        DecodedSignal sigParam;
        sigParam.name = QString("Parameter %1").arg(paramNum++);
        uint8_t param = msg.GetByte(index);
        if (param == 0xFF) {
            sigParam.value = "NULL";
        } else {
            sigParam.value = QString("0x%1").arg(param, 2, 16, QChar('0')).toUpper();
        }
        sigParam.isValid = true;
        decoded.signalList.append(sigParam);
    }

    return decoded;
}

DecodedMessage DBCDecoder::decodePGN130330(const tN2kMsg& msg)
{
    DecodedMessage decoded;
    decoded.messageName = "Lighting System Settings (130330)";
    decoded.description = "NMEA2000 Lighting System Settings Message";
    decoded.isDecoded = true;

    if (msg.DataLen < 1) {
        DecodedSignal sig;
        sig.name = "Error";
        sig.value = "Message too short for PGN 130330";
        decoded.signalList.append(sig);
        return decoded;
    }

    int index = 0;

    // Byte 0: System ID (0-252, 253=Broadcast, 254=No System, 255=NULL)
    uint8_t systemId = msg.GetByte(index);
    DecodedSignal sigSystemId;
    sigSystemId.name = "System ID";
    if (systemId == 253) {
        sigSystemId.value = QString("%1 (Broadcast)").arg(systemId);
    } else if (systemId == 254) {
        sigSystemId.value = QString("%1 (No System)").arg(systemId);
    } else if (systemId == 255) {
        sigSystemId.value = QString("%1 (NULL)").arg(systemId);
    } else {
        sigSystemId.value = systemId;
    }
    sigSystemId.isValid = true;
    decoded.signalList.append(sigSystemId);

    // Byte 1: System Name Length (if available)
    if (index < msg.DataLen) {
        uint8_t nameLen = msg.GetByte(index);
        DecodedSignal sigNameLen;
        sigNameLen.name = "System Name Length";
        sigNameLen.value = nameLen;
        sigNameLen.isValid = true;
        decoded.signalList.append(sigNameLen);

        // System Name (variable length string)
        QString systemName;
        for (int i = 0; i < nameLen && index < msg.DataLen; i++) {
            char c = msg.GetByte(index);
            if (c != 0) systemName += c;
        }
        if (!systemName.isEmpty()) {
            DecodedSignal sigSystemName;
            sigSystemName.name = "System Name";
            sigSystemName.value = systemName;
            sigSystemName.isValid = true;
            decoded.signalList.append(sigSystemName);
        }
    }

    // Byte N: Configuration flags (if available)
    if (index < msg.DataLen) {
        uint8_t configFlags = msg.GetByte(index);
        DecodedSignal sigConfigFlags;
        sigConfigFlags.name = "Configuration Flags";
        sigConfigFlags.value = QString("0x%1").arg(configFlags, 2, 16, QChar('0')).toUpper();
        sigConfigFlags.isValid = true;
        decoded.signalList.append(sigConfigFlags);

        // Decode individual flags
        if (configFlags & 0x01) {
            DecodedSignal sigFlag;
            sigFlag.name = "Auto Dimming Enabled";
            sigFlag.value = "Yes";
            sigFlag.isValid = true;
            decoded.signalList.append(sigFlag);
        }
        if (configFlags & 0x02) {
            DecodedSignal sigFlag;
            sigFlag.name = "Remote Control Enabled";
            sigFlag.value = "Yes";
            sigFlag.isValid = true;
            decoded.signalList.append(sigFlag);
        }
        if (configFlags & 0x04) {
            DecodedSignal sigFlag;
            sigFlag.name = "Zone Control Enabled";
            sigFlag.value = "Yes";
            sigFlag.isValid = true;
            decoded.signalList.append(sigFlag);
        }
    }

    // Byte N+1: Default brightness level (0-100%, if available)
    if (index < msg.DataLen) {
        uint8_t defaultBrightness = msg.GetByte(index);
        DecodedSignal sigBrightness;
        sigBrightness.name = "Default Brightness";
        if (defaultBrightness <= 100) {
            sigBrightness.value = QString("%1%").arg(defaultBrightness);
        } else if (defaultBrightness == 255) {
            sigBrightness.value = "Not Available";
        } else {
            sigBrightness.value = QString("Invalid (%1)").arg(defaultBrightness);
        }
        sigBrightness.isValid = true;
        decoded.signalList.append(sigBrightness);
    }

    // Remaining bytes as raw data
    while (index < msg.DataLen) {
        static int paramNum = 1;
        DecodedSignal sigParam;
        sigParam.name = QString("Reserved %1").arg(paramNum++);
        sigParam.value = QString("0x%1").arg(msg.GetByte(index), 2, 16, QChar('0')).toUpper();
        sigParam.isValid = true;
        decoded.signalList.append(sigParam);
    }

    return decoded;
}
