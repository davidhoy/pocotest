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

    // Field 1 - Zone ID (0-252, 253=Broadcast, 254=No Zone, 255=NULL)
    uint8_t zoneId = msg.GetByte(index);
    DecodedSignal sigZoneId;
    sigZoneId.name = "Zone ID";
    sigZoneId.isValid = true;
    sigZoneId.value = zoneId;
    decoded.signalList.append(sigZoneId);

    // Field 2 - Zone Name
    char zoneName[80];
    size_t zoneNameSize = sizeof(zoneName);
    msg.GetVarStr(zoneNameSize, zoneName, index);
    DecodedSignal sigZoneName;
    sigZoneName.name = "Zone Name";
    sigZoneName.isValid = true;
    sigZoneName.value = QString::fromUtf8(zoneName);
    decoded.signalList.append(sigZoneName);

    // Field 3 - Red Component (0-255)
    uint8_t red = msg.GetByte(index);
    DecodedSignal sigRed;
    sigRed.name = "Red";
    sigRed.isValid = true;
    if (red == 255) {
        sigRed.value = "Not Available";
    } else {
        sigRed.value = QString("%1 (%2%)").arg(red).arg((red * 100.0 / 255.0), 0, 'f', 1);
    }
    decoded.signalList.append(sigRed);

    // Field 4 - Green Component (0-255)
    uint8_t green = msg.GetByte(index);
    DecodedSignal sigGreen;
    sigGreen.name = "Green";
    sigGreen.isValid = true;
    if (green == 255) {
        sigGreen.value = "Not Available";
    } else {
        sigGreen.value = QString("%1 (%2%)").arg(green).arg((green * 100.0 / 255.0), 0, 'f', 1);
    }
    decoded.signalList.append(sigGreen);

    // Field 5 - Blue Component (0-255)
    uint8_t blue = msg.GetByte(index);
    DecodedSignal sigBlue;
    sigBlue.name = "Blue";
    sigBlue.isValid = true;
    if (blue == 255) {
        sigBlue.value = "Not Available";
    } else {
        sigBlue.value = QString("%1 (%2%)").arg(blue).arg((blue * 100.0 / 255.0), 0, 'f', 1);
    }
    decoded.signalList.append(sigBlue);

    // Field 6 - Color Temperature (0-65535)
    uint16_t colorTemp = msg.Get2ByteUInt(index);
    DecodedSignal sigColorTemp;
    sigColorTemp.name = "Color Temp";
    sigColorTemp.isValid = true;
    if (colorTemp == 65535) {
        sigColorTemp.value = "Not Available";
    } else {
        sigColorTemp.value = QString("%1 K").arg(colorTemp);
    }
    decoded.signalList.append(sigColorTemp);

    // Field 7 - Intensity (0-255)
    uint8_t intensity = msg.GetByte(index);
    DecodedSignal sigIntensity;
    sigIntensity.name = "Intensity";
    sigIntensity.isValid = true;
    if (intensity == 255) {
        sigIntensity.value = "Not Available";
    } else {
        sigIntensity.value = QString("%1 (%2%)").arg(intensity).arg((intensity * 100.0 / 255.0), 0, 'f', 1);
    }
    decoded.signalList.append(sigIntensity);    

    // Field 8 - Program Id (0-255)
    uint8_t programId = msg.GetByte(index);
    DecodedSignal sigProgramId;
    sigProgramId.name = "Program Id";
    sigProgramId.isValid = true;
    if (programId == 255) {
        sigProgramId.value = "Not Available";
    } else {
        sigProgramId.value = QString::number(programId);
    }
    decoded.signalList.append(sigProgramId);

    // Field 9 - Program Color Sequence Index (0-255)
    uint8_t programColorSeqIndex = msg.GetByte(index);
    DecodedSignal sigProgramColorSeqIndex;
    sigProgramColorSeqIndex.name = "Color Seq Index";
    sigProgramColorSeqIndex.isValid = true;
    if (programColorSeqIndex == 255) {
        sigProgramColorSeqIndex.value = "Not Available";
    } else {
        sigProgramColorSeqIndex.value = QString::number(programColorSeqIndex);
    }
    decoded.signalList.append(sigProgramColorSeqIndex);

    // Field 10 - Program Intensity (0-255)
    uint8_t programIntensity = msg.GetByte(index);
    DecodedSignal sigProgramIntensity;
    sigProgramIntensity.name = "Intensity";
    sigProgramIntensity.isValid = true;
    if (programIntensity == 255) {
        sigProgramIntensity.value = "Not Available";
    } else {
        sigProgramIntensity.value = QString("%1 (%2%)").arg(programIntensity).arg((programIntensity * 100.0 / 255.0), 0, 'f', 1);
    }
    decoded.signalList.append(sigProgramIntensity);

    // Field 11 - Program Rate (0-255)
    uint8_t programRate = msg.GetByte(index);
    DecodedSignal sigProgramRate;
    sigProgramRate.name = "Rate";
    sigProgramRate.isValid = true;
    if (programRate == 255) {
        sigProgramRate.value = "Not Available";
    } else {
        sigProgramRate.value = QString("%1 (%2%)").arg(programRate).arg((programRate * 100.0 / 255.0), 0, 'f', 1);
    }
    decoded.signalList.append(sigProgramRate);

    // Field 12 - Program Color Sequence (0-255)
    uint8_t programColorSeq = msg.GetByte(index);
    DecodedSignal sigProgramColorSeq;
    sigProgramColorSeq.name = "Color Seq";
    sigProgramColorSeq.isValid = true;
    if (programColorSeq == 255) {
        sigProgramColorSeq.value = "Not Available";
    } else {
        sigProgramColorSeq.value = QString::number(programColorSeq);
    }
    decoded.signalList.append(sigProgramColorSeq);

    // Field 13 - Zone Enabled (2 bits)
    uint8_t byte = msg.GetByte(index);
    uint8_t zoneEnabled = byte & 0x03;
    DecodedSignal sigZoneEnabled;
    sigZoneEnabled.name = "Zone Enabled";
    sigZoneEnabled.isValid = true;
    switch (zoneEnabled) {
        case 0: sigZoneEnabled.value = "Off"; break;
        case 1: sigZoneEnabled.value = "On"; break;
        case 2: sigZoneEnabled.value = "Error 2"; break;
        case 3: sigZoneEnabled.value = "Unavailable"; break;
        default: sigZoneEnabled.value = QString::number(zoneEnabled);
    }
    decoded.signalList.append(sigZoneEnabled);

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

    // Field 1: Scene ID (0-252, 253=All Scenes, 254=Current Scene, 255=N/A)
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

    // Field 2 - Scene Name (variable)
    char sceneName[80];
    size_t sceneNameSize = sizeof(sceneName);
    msg.GetVarStr(sceneNameSize, sceneName, index);
    DecodedSignal sigSceneName;
    sigSceneName.name = "Scene Name";
    sigSceneName.isValid = true;
    sigSceneName.value = QString::fromUtf8(sceneName);
    decoded.signalList.append(sigSceneName);

    // Field 3 - Control (0-255)
    uint8_t control = msg.GetByte(index);
    DecodedSignal sigControl;
    sigControl.name = "Control";
    sigControl.isValid = true;
    sigControl.value = QString::number(control);
    decoded.signalList.append(sigControl);

    // Field 4 - Configuration Count (0-255)
    uint8_t configCount = msg.GetByte(index);
    DecodedSignal sigConfigCount;
    sigConfigCount.name = "Config Count";
    sigConfigCount.isValid = true;
    sigConfigCount.value = QString::number(configCount);
    decoded.signalList.append(sigConfigCount);

    // Repeat fields 5-12 as needed based on value from field #4
    for (int i = 0; i < configCount; ++i) {
        // Field 5 - Configuration Index (0-255)
        uint8_t configIndex = msg.GetByte(index);
        DecodedSignal sigConfigIndex;
        sigConfigIndex.name = QString("Config Index [%1]").arg(i);
        sigConfigIndex.isValid = true;
        sigConfigIndex.value = QString::number(configIndex);
        decoded.signalList.append(sigConfigIndex);

        // Field 6 - Zone Index (0-255)
        uint8_t zoneIndex = msg.GetByte(index);
        DecodedSignal sigZoneIndex;
        sigZoneIndex.name = QString("Zone Index [%1]").arg(i);
        sigZoneIndex.isValid = true;
        sigZoneIndex.value = QString::number(zoneIndex);
        decoded.signalList.append(sigZoneIndex);

        // Field 7 - Devices ID (0-4,294,967,295)
        uint32_t devicesId = msg.Get4ByteUInt(index);
        DecodedSignal sigDevicesId;
        sigDevicesId.name = QString("Devices ID [%1]").arg(i);
        sigDevicesId.isValid = true;
        sigDevicesId.value = QString::number(devicesId);
        decoded.signalList.append(sigDevicesId);

        // Field 8 - Program Index (0-255)
        uint8_t programIndex = msg.GetByte(index);
        DecodedSignal sigProgramIndex;
        sigProgramIndex.name = QString("Index [%1]").arg(i);
        sigProgramIndex.isValid = true;
        sigProgramIndex.value = QString::number(programIndex);
        decoded.signalList.append(sigProgramIndex);

        // Field 9 - Program Color Sequence Index (0-255)
        uint8_t programColorSeqIndex = msg.GetByte(index);
        DecodedSignal sigProgramColorSeqIndex;
        sigProgramColorSeqIndex.name = QString("Color Sequence Index [%1]").arg(i);
        sigProgramColorSeqIndex.isValid = true;
        sigProgramColorSeqIndex.value = QString::number(programColorSeqIndex);
        decoded.signalList.append(sigProgramColorSeqIndex); 

        // Field 10 - Program Intensity (0-255)
        uint8_t programIntensity = msg.GetByte(index);
        DecodedSignal sigProgramIntensity;
        sigProgramIntensity.name = QString("Intensity [%1]").arg(i);
        sigProgramIntensity.isValid = true;
        sigProgramIntensity.value = QString::number(programIntensity);
        decoded.signalList.append(sigProgramIntensity);

        // Field 11 - Program Rate (0-255)
        uint8_t programRate = msg.GetByte(index);
        DecodedSignal sigProgramRate;
        sigProgramRate.name = QString("Rate [%1]").arg(i);
        sigProgramRate.isValid = true;
        sigProgramRate.value = QString::number(programRate);
        decoded.signalList.append(sigProgramRate);

        // Field 12 - Program Color Sequence Rate (0-100)
        uint8_t programColorSeqRate = msg.GetByte(index);
        DecodedSignal sigProgramColorSeqRate;
        sigProgramColorSeqRate.name = QString("Color Seq Rate [%1]").arg(i);
        sigProgramColorSeqRate.isValid = true;
        sigProgramColorSeqRate.value = QString::number(programColorSeqRate);
        decoded.signalList.append(sigProgramColorSeqRate);
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

    // Field 1  - Device ID (0-4,294,967,295)
    uint32_t deviceId = msg.Get4ByteUInt(index);
    DecodedSignal sigDeviceId;
    sigDeviceId.name = "Device ID";
    sigDeviceId.isValid = true;
    sigDeviceId.value = QString::number(deviceId);
    decoded.signalList.append(sigDeviceId);

    // Field 2 - Device Capabilities (0-0xff)
    uint8_t deviceCapabilities = msg.GetByte(index);
    DecodedSignal sigDeviceCapabilities;
    sigDeviceCapabilities.name = "Device Capabilities";
    sigDeviceCapabilities.isValid = true;
    sigDeviceCapabilities.value = QString::number(deviceCapabilities);
    decoded.signalList.append(sigDeviceCapabilities);

    // Field 3 - Color Capabilities (0-0xff)
    uint8_t colorCapabilities = msg.GetByte(index);
    DecodedSignal sigColorCapabilities;
    sigColorCapabilities.name = "Color Capabilities";
    sigColorCapabilities.isValid = true;
    sigColorCapabilities.value = QString::number(colorCapabilities);
    decoded.signalList.append(sigColorCapabilities);

    // Field 4 - Zone Index (0-255)
    uint8_t zoneIndex = msg.GetByte(index);
    DecodedSignal sigZoneIndex;
    sigZoneIndex.name = "Zone Index";
    sigZoneIndex.isValid = true;
    sigZoneIndex.value = QString::number(zoneIndex);
    decoded.signalList.append(sigZoneIndex);

    // Field 5 - Device Name (variable string)
    char deviceName[80];
    size_t deviceNameSize = sizeof(deviceName);
    msg.GetVarStr(deviceNameSize, deviceName, index);
    DecodedSignal sigDeviceName;
    sigDeviceName.name = "Device Name";
    sigDeviceName.isValid = true;
    sigDeviceName.value = QString::fromUtf8(deviceName);
    decoded.signalList.append(sigDeviceName);

    // Field 6 - Status (0-252)
    uint8_t deviceStatus = msg.GetByte(index);
    DecodedSignal sigDeviceStatus;
    sigDeviceStatus.name = "Device Status";
    sigDeviceStatus.isValid = true;
    QString statusDesc;
    switch (deviceStatus) {
        case 0x00: statusDesc = "Normal"; break;
        case 0x01: statusDesc = "Undetected"; break;
        case 0x02: statusDesc = "General Error"; break;
        case 0x03: statusDesc = "Temperature Error"; break;
        case 0x04: statusDesc = "Voltage Error"; break;
        case 0x05: statusDesc = "Maintenance Req"; break;
        case 0x06: statusDesc = "Over Current"; break;
        default: statusDesc = "Reserved"; break;
    }
    sigDeviceStatus.value = QString("%1 (0x%2)").arg(statusDesc).arg(deviceStatus, 2, 16, QChar('0')).toUpper();
    decoded.signalList.append(sigDeviceStatus);

    // Field 7 - Red Component (0-255)
    uint8_t redComponent = msg.GetByte(index);
    DecodedSignal sigRedComponent;
    sigRedComponent.name = "Red";
    sigRedComponent.isValid = true;
    sigRedComponent.value = QString::number(redComponent);
    decoded.signalList.append(sigRedComponent);

    // Field 8 - Green Component (0-255)
    uint8_t greenComponent = msg.GetByte(index);
    DecodedSignal sigGreenComponent;
    sigGreenComponent.name = "Green";
    sigGreenComponent.isValid = true;
    sigGreenComponent.value = QString::number(greenComponent);
    decoded.signalList.append(sigGreenComponent);

    // Field 9 - Blue Component (0-255)
    uint8_t blueComponent = msg.GetByte(index);
    DecodedSignal sigBlueComponent;
    sigBlueComponent.name = "Blue";
    sigBlueComponent.isValid = true;
    sigBlueComponent.value = QString::number(blueComponent);
    decoded.signalList.append(sigBlueComponent);

    // Field 10 - Color Temperature (0-65535)
    uint16_t colorTemperature = msg.Get2ByteUInt(index);
    DecodedSignal sigColorTemperature;
    sigColorTemperature.name = "Color Temp";
    sigColorTemperature.isValid = true;
    sigColorTemperature.value = QString::number(colorTemperature);
    decoded.signalList.append(sigColorTemperature);

    // Field 11 - Intensity (0-255)
    uint8_t intensity = msg.GetByte(index);
    DecodedSignal sigIntensity;
    sigIntensity.name = "Intensity";
    sigIntensity.isValid = true;
    sigIntensity.value = QString::number(intensity);
    decoded.signalList.append(sigIntensity);

    // Field 12 - Program ID (0-255)
    uint8_t programID = msg.GetByte(index);
    DecodedSignal sigProgramID;
    sigProgramID.name = "Program ID";
    sigProgramID.isValid = true;
    sigProgramID.value = QString::number(programID);
    decoded.signalList.append(sigProgramID);

    // Field 13 - Program Color Sequence ID (0-255)
    uint8_t programColorSeqID = msg.GetByte(index);
    DecodedSignal sigProgramColorSeqID;
    sigProgramColorSeqID.name = "Color Seq ID";
    sigProgramColorSeqID.isValid = true;
    sigProgramColorSeqID.value = QString::number(programColorSeqID);
    decoded.signalList.append(sigProgramColorSeqID);

    // Field 14 - Program Intensity (0-255)
    uint8_t programIntensity = msg.GetByte(index);
    DecodedSignal sigProgramIntensity;
    sigProgramIntensity.name = "Intensity";
    sigProgramIntensity.isValid = true;
    sigProgramIntensity.value = QString::number(programIntensity);
    decoded.signalList.append(sigProgramIntensity);

    // Field 15 - Program Rate (0-100)
    uint8_t programRate = msg.GetByte(index);
    DecodedSignal sigProgramRate;
    sigProgramRate.name = "Rate";
    sigProgramRate.isValid = true;
    sigProgramRate.value = QString::number(programRate);
    decoded.signalList.append(sigProgramRate);

    // Field 16 - Program Color Sequence Rate (0-100)
    uint8_t programColorSeqRate = msg.GetByte(index);
    DecodedSignal sigProgramColorSeqRate;
    sigProgramColorSeqRate.name = "Color Seq Rate";
    sigProgramColorSeqRate.isValid = true;
    sigProgramColorSeqRate.value = QString::number(programColorSeqRate);
    decoded.signalList.append(sigProgramColorSeqRate);

    // Field 17 - Enabled (2 bits)
    uint8_t byte = msg.GetByte(index);
    uint8_t programEnabled = byte & 0x03;
    DecodedSignal sigProgramEnabled;
    sigProgramEnabled.name = "Program Enabled";
    sigProgramEnabled.isValid = true;
    switch (programEnabled) {
        case 0:  sigProgramEnabled.value = "Off"; break;
        case 1:  sigProgramEnabled.value = "On";  break;
        case 2:  sigProgramEnabled.value = "Error"; break;
        case 3:  sigProgramEnabled.value = "Unknown"; break;
        default: sigProgramEnabled.value = QString::number(programEnabled);  break;
    }
    decoded.signalList.append(sigProgramEnabled);

    // Field 18 - Reserved (6 bits)
    uint8_t reserved = (byte >> 2) & 0x3F;
    DecodedSignal sigReserved;
    sigReserved.name = "Reserved";
    sigReserved.isValid = true;
    sigReserved.value = QString::number(reserved);
    decoded.signalList.append(sigReserved);

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

    // Field 1: Index of First Device (0-65536)
    uint16_t firstDeviceIndex = msg.Get2ByteUInt(index);
    DecodedSignal sigFirstDeviceIndex;
    sigFirstDeviceIndex.name = "First Device Index";
    sigFirstDeviceIndex.value = QString::number(firstDeviceIndex);
    sigFirstDeviceIndex.isValid = true;
    decoded.signalList.append(sigFirstDeviceIndex);

    // Field 2: Total Number of Devices (0-65536)
    uint16_t totalNumberOfDevices = msg.Get2ByteUInt(index);
    DecodedSignal sigNumberOfDevices;
    sigNumberOfDevices.name = "Total Number of Devices";
    sigNumberOfDevices.value = QString::number(totalNumberOfDevices);
    sigNumberOfDevices.isValid = true;
    decoded.signalList.append(sigNumberOfDevices);

    // Field 3 - Number of Devices (0-65536)
    uint16_t numberOfDevices = msg.Get2ByteUInt(index);
    DecodedSignal sigNumberOfDevices;
    sigNumberOfDevices.name = "Number of Devices";
    sigNumberOfDevices.value = QString::number(numberOfDevices);
    sigNumberOfDevices.isValid = true;
    decoded.signalList.append(sigNumberOfDevices);

    // Field 4 - Device ID (4 bytes)
    uint32_t deviceID = msg.Get4ByteUInt(index);
    DecodedSignal sigDeviceID;
    sigDeviceID.name = "Device ID";
    sigDeviceID.value = QString::number(deviceID);
    sigDeviceID.isValid = true;
    decoded.signalList.append(sigDeviceID);

    // Field 5 - Status (0-255)
    uint8_t status = msg.GetByte(index);
    DecodedSignal sigStatus;
    sigStatus.name = "Status";
    QString statusDesc;
    switch (status) {
        case 0x00: statusDesc = "Detected / Normal"; break;
        case 0x01: statusDesc = "Undetected"; break;
        case 0x02: statusDesc = "General Error"; break;
        case 0x03: statusDesc = "Temperature Error"; break;
        case 0x04: statusDesc = "Voltage Error"; break;
        case 0x05: statusDesc = "Maintenance Required"; break;
        case 0x06: statusDesc = "Over Current Detected"; break;
        default:   statusDesc = "Reserved for future use"; break;
    }
    sigStatus.value = QString("%1 (0x%2)").arg(statusDesc).arg(status, 2, 16, QChar('0')).toUpper();
    sigStatus.isValid = true;
    decoded.signalList.append(sigStatus);
    
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

    // Field 1 - Sequence Index (0-252)
    uint8_t sequenceIndex = msg.GetByte(index);
    DecodedSignal sigSeqIndex;
    sigSeqIndex.name = "Sequence Index";
    sigSeqIndex.value = QString::number(sequenceIndex);
    sigSeqIndex.isValid = true;
    decoded.signalList.append(sigSeqIndex);

    // Field 2 - Color Count (0-252)
    uint8_t colorCount = msg.GetByte(index);
    DecodedSignal sigColorCount;
    sigColorCount.name = "Color Count";
    sigColorCount.value = QString::number(colorCount);
    sigColorCount.isValid = true;
    decoded.signalList.append(sigColorCount);

    // Repeat field 3-8 as needed
    for (int i = 0; i < colorCount; i++) {
        // Field 3 - Color Index (0-252)
        uint8_t colorIndex = msg.GetByte(index);
        DecodedSignal sigColorIndex;
        sigColorIndex.name = QString("Color Index [%1]").arg(i);
        sigColorIndex.value = QString::number(colorIndex);
        sigColorIndex.isValid = true;
        decoded.signalList.append(sigColorIndex);

        // Field 4 - Red Component (0-255)
        uint8_t redComponent = msg.GetByte(index);
        DecodedSignal sigRedComponent;
        sigRedComponent.name = QString("Red [%1]").arg(i);
        sigRedComponent.value = QString::number(redComponent);
        sigRedComponent.isValid = true;
        decoded.signalList.append(sigRedComponent);

        // Field 5 - Green Component (0-255)
        uint8_t greenComponent = msg.GetByte(index);
        DecodedSignal sigGreenComponent;
        sigGreenComponent.name = QString("Green [%1]").arg(i);
        sigGreenComponent.value = QString::number(greenComponent);
        sigGreenComponent.isValid = true;
        decoded.signalList.append(sigGreenComponent);

        // Field 6 - Blue Component (0-255)
        uint8_t blueComponent = msg.GetByte(index);
        DecodedSignal sigBlueComponent;
        sigBlueComponent.name = QString("Blue [%1]").arg(i);
        sigBlueComponent.value = QString::number(blueComponent);
        sigBlueComponent.isValid = true;
        decoded.signalList.append(sigBlueComponent);

        // Field 7 - Color Temperature (0-65536)
        uint16_t colorTemperature = msg.Get2ByteUInt(index);
        DecodedSignal sigColorTemp;
        sigColorTemp.name = QString("Color Temp [%1]").arg(i);
        sigColorTemp.value = QString::number(colorTemperature);
        sigColorTemp.isValid = true;
        decoded.signalList.append(sigColorTemp);

        // Field 8 - Intensity (0-100)
        uint8_t intensity = msg.GetByte(index);
        DecodedSignal sigIntensity;
        sigIntensity.name = QString("Intensity [%1]").arg(i);
        sigIntensity.value = QString::number(intensity);
        sigIntensity.isValid = true;
        decoded.signalList.append(sigIntensity);
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
