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

        // Minimum length check
        if (msg.DataLen < 8) {
            DecodedSignal sig;
            sig.name = "Error";
            sig.value = "Too short for 126208 decode";
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
        sigFC.value = functionCode;
        decoded.signalList.append(sigFC);

        DecodedSignal sigPGN;
        sigPGN.name = "Target PGN";
        sigPGN.value = QString("%1").arg(targetPGN);
        decoded.signalList.append(sigPGN);

        DecodedSignal sigPrio;
        sigPrio.name = "Priority";
        sigPrio.value = priority;
        decoded.signalList.append(sigPrio);

        DecodedSignal sigNumParams;
        sigNumParams.name = "Number of Parameters";
        sigNumParams.value = numParams;
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
            decoded.signalList.append(sigField);
        }
        return decoded;
    }

    // Special handling for lighting PGNs
    if (msg.PGN == 130561) {
        return decodePGN130561(msg);
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
    if (pgn == 130563 || pgn == 130564 || pgn == 130565 || pgn == 130566 || pgn == 130561) {
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

    if (msg.DataLen < 3) {
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
    sigZoneId.value = zoneId;
    if (zoneId == 253) {
        sigZoneId.value = QString("%1 (Broadcast)").arg(zoneId);
    } else if (zoneId == 254) {
        sigZoneId.value = QString("%1 (No Zone)").arg(zoneId);
    } else if (zoneId == 255) {
        sigZoneId.value = QString("%1 (NULL)").arg(zoneId);
    }
    decoded.signalList.append(sigZoneId);

    // Byte 1: Zone Name Length
    uint8_t nameLen = msg.GetByte(index);
    DecodedSignal sigNameLen;
    sigNameLen.name = "Zone Name Length";
    sigNameLen.value = nameLen;
    decoded.signalList.append(sigNameLen);

    // Bytes 2+: Zone Name (variable length string)
    QString zoneName;
    for (int i = 0; i < nameLen && index < msg.DataLen; i++) {
        char c = msg.GetByte(index);
        if (c != 0) zoneName += c;
    }
    if (!zoneName.isEmpty()) {
        DecodedSignal sigZoneName;
        sigZoneName.name = "Zone Name";
        sigZoneName.value = zoneName;
        decoded.signalList.append(sigZoneName);
    }

    // Continue decoding remaining fields if available
    if (index < msg.DataLen) {
        // Red Component (0-255)
        uint8_t red = msg.GetByte(index);
        DecodedSignal sigRed;
        sigRed.name = "Red";
        sigRed.value = red;
        decoded.signalList.append(sigRed);
    }

    if (index < msg.DataLen) {
        // Green Component (0-255)
        uint8_t green = msg.GetByte(index);
        DecodedSignal sigGreen;
        sigGreen.name = "Green";
        sigGreen.value = green;
        decoded.signalList.append(sigGreen);
    }

    if (index < msg.DataLen) {
        // Blue Component (0-255)
        uint8_t blue = msg.GetByte(index);
        DecodedSignal sigBlue;
        sigBlue.name = "Blue";
        sigBlue.value = blue;
        decoded.signalList.append(sigBlue);
    }

    if (index + 1 < msg.DataLen) {
        // Color Temperature (0-65532 Kelvin)
        uint16_t colorTemp = msg.Get2ByteUInt(index);
        DecodedSignal sigColorTemp;
        sigColorTemp.name = "Color Temperature";
        sigColorTemp.value = colorTemp;
        sigColorTemp.unit = "K";
        decoded.signalList.append(sigColorTemp);
    }

    if (index < msg.DataLen) {
        // Intensity/Brightness (0-200 * 0.5%)
        uint8_t intensity = msg.GetByte(index);
        DecodedSignal sigIntensity;
        sigIntensity.name = "Intensity";
        sigIntensity.value = QString("%1% (%2/200)").arg(intensity * 0.5, 0, 'f', 1).arg(intensity);
        decoded.signalList.append(sigIntensity);
    }

    return decoded;
}

DecodedMessage DBCDecoder::decodePGN130563(const tN2kMsg& msg)
{
    DecodedMessage decoded;
    decoded.messageName = "Lighting Device (130563)";
    decoded.description = "NMEA2000 Lighting Device Message";
    decoded.isDecoded = true;

    if (msg.DataLen < 1) {
        DecodedSignal sig;
        sig.name = "Error";
        sig.value = "Message too short";
        decoded.signalList.append(sig);
        return decoded;
    }

    int index = 0;

    // Basic decoding - this PGN structure varies by implementation
    DecodedSignal sigDeviceId;
    sigDeviceId.name = "Device ID";
    sigDeviceId.value = msg.GetByte(index);
    decoded.signalList.append(sigDeviceId);

    // Add remaining bytes as hex data for now
    if (index < msg.DataLen) {
        QString remainingData;
        while (index < msg.DataLen) {
            remainingData += QString(" %1").arg(msg.GetByte(index), 2, 16, QChar('0')).toUpper();
        }
        DecodedSignal sigData;
        sigData.name = "Additional Data";
        sigData.value = remainingData.trimmed();
        decoded.signalList.append(sigData);
    }

    return decoded;
}

DecodedMessage DBCDecoder::decodePGN130564(const tN2kMsg& msg)
{
    DecodedMessage decoded;
    decoded.messageName = "Lighting Device Enumeration (130564)";
    decoded.description = "NMEA2000 Lighting Device Enumeration Message";
    decoded.isDecoded = true;

    if (msg.DataLen < 1) {
        DecodedSignal sig;
        sig.name = "Error";
        sig.value = "Message too short";
        decoded.signalList.append(sig);
        return decoded;
    }

    int index = 0;

    // Device enumeration info
    DecodedSignal sigDeviceCount;
    sigDeviceCount.name = "Device Count";
    sigDeviceCount.value = msg.GetByte(index);
    decoded.signalList.append(sigDeviceCount);

    // Add remaining bytes as device info
    int deviceNum = 1;
    while (index < msg.DataLen) {
        DecodedSignal sigDevice;
        sigDevice.name = QString("Device %1 Info").arg(deviceNum++);
        sigDevice.value = QString("0x%1").arg(msg.GetByte(index), 2, 16, QChar('0')).toUpper();
        decoded.signalList.append(sigDevice);
    }

    return decoded;
}

DecodedMessage DBCDecoder::decodePGN130565(const tN2kMsg& msg)
{
    DecodedMessage decoded;
    decoded.messageName = "Lighting Color Sequence (130565)";
    decoded.description = "NMEA2000 Lighting Color Sequence Message";
    decoded.isDecoded = true;

    if (msg.DataLen < 2) {
        DecodedSignal sig;
        sig.name = "Error";
        sig.value = "Message too short";
        decoded.signalList.append(sig);
        return decoded;
    }

    int index = 0;

    // Sequence ID
    DecodedSignal sigSeqId;
    sigSeqId.name = "Sequence ID";
    sigSeqId.value = msg.GetByte(index);
    decoded.signalList.append(sigSeqId);

    // Number of colors in sequence
    DecodedSignal sigColorCount;
    sigColorCount.name = "Color Count";
    sigColorCount.value = msg.GetByte(index);
    decoded.signalList.append(sigColorCount);

    // Decode color entries (typically RGB triplets)
    int colorIndex = 1;
    while (index + 2 < msg.DataLen) {
        uint8_t red = msg.GetByte(index);
        uint8_t green = msg.GetByte(index);
        uint8_t blue = msg.GetByte(index);
        
        DecodedSignal sigColor;
        sigColor.name = QString("Color %1").arg(colorIndex++);
        sigColor.value = QString("RGB(%1,%2,%3)").arg(red).arg(green).arg(blue);
        decoded.signalList.append(sigColor);
    }

    return decoded;
}

DecodedMessage DBCDecoder::decodePGN130566(const tN2kMsg& msg)
{
    DecodedMessage decoded;
    decoded.messageName = "Lighting Program (130566)";
    decoded.description = "NMEA2000 Lighting Program Message";
    decoded.isDecoded = true;

    if (msg.DataLen < 2) {
        DecodedSignal sig;
        sig.name = "Error";
        sig.value = "Message too short";
        decoded.signalList.append(sig);
        return decoded;
    }

    int index = 0;

    // Program ID
    DecodedSignal sigProgId;
    sigProgId.name = "Program ID";
    sigProgId.value = msg.GetByte(index);
    decoded.signalList.append(sigProgId);

    // Program Type/Mode
    DecodedSignal sigProgType;
    sigProgType.name = "Program Type";
    uint8_t progType = msg.GetByte(index);
    QString progTypeName;
    switch (progType) {
        case 0: progTypeName = "Off"; break;
        case 1: progTypeName = "Static"; break;
        case 2: progTypeName = "Fade"; break;
        case 3: progTypeName = "Flash"; break;
        case 4: progTypeName = "Sequence"; break;
        default: progTypeName = QString("Unknown (%1)").arg(progType); break;
    }
    sigProgType.value = progTypeName;
    decoded.signalList.append(sigProgType);

    // Add remaining bytes as program parameters
    while (index < msg.DataLen) {
        static int paramNum = 1;
        DecodedSignal sigParam;
        sigParam.name = QString("Parameter %1").arg(paramNum++);
        sigParam.value = QString("0x%1").arg(msg.GetByte(index), 2, 16, QChar('0')).toUpper();
        decoded.signalList.append(sigParam);
    }

    return decoded;
}
