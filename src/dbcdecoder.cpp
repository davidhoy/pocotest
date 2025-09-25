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
#include "NMEA2000.h"

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
    
    // Initialize custom decoder lookup table
    initializeCustomDecoders();
    
    qDebug() << "DBCDecoder initialized with" << m_messages.size() << "message definitions and" << m_customDecoders.size() << "custom decoders";
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

    // Check if we have a custom decoder for this PGN
    auto customDecoderIt = m_customDecoders.find(msg.PGN);
    if (customDecoderIt != m_customDecoders.end()) {
        return customDecoderIt.value().decoder(this, msg);
    }

    // Fall back to DBC-based decoding if no custom decoder exists
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
        
        // Check if we have a better field name available for lighting PGNs
        QString enhancedName = signal.name;
        if (signal.name.contains(QRegularExpression("^Field\\s*\\d+$|^Field_\\d+$", QRegularExpression::CaseInsensitiveOption))) {
            // Extract field number from generic field name
            QRegularExpression fieldRegex("(\\d+)");
            QRegularExpressionMatch match = fieldRegex.match(signal.name);
            if (match.hasMatch()) {
                uint8_t fieldNum = match.captured(1).toUInt();
                enhancedName = getFieldName(msg.PGN, fieldNum);
            }
        }
        
        decodedSignal.name = enhancedName;
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
    // Check if we have a custom decoder for this PGN
    if (m_customDecoders.contains(pgn)) {
        return true;
    }
    
    // Check if we have a DBC definition for this PGN
    return m_messages.contains(pgn);
}

QString DBCDecoder::getMessageName(unsigned long pgn) const
{
    // First check if we have a custom decoder with a name for this PGN
    if (m_customDecoders.contains(pgn)) {
        return m_customDecoders[pgn].name;
    }
    
    // Fall back to DBC message names
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
            
            if (signal.value.typeId() == QMetaType::Double) {
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
            
            if (signal.value.typeId() == QMetaType::Double) {
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

QList<unsigned long> DBCDecoder::getCustomDecoderPGNs() const
{
    return m_customDecoders.keys();
}

bool DBCDecoder::hasCustomDecoder(unsigned long pgn) const
{
    return m_customDecoders.contains(pgn);
}

QString DBCDecoder::formatSignalValue(const DecodedSignal& signal)
{
    if (!signal.isValid || signal.value.toString() == "N/A") {
        return "N/A";
    }
    
    if (signal.value.typeId() == QMetaType::Double) {
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

void DBCDecoder::initializeCustomDecoders()
{
    // Initialize the custom decoder lookup table with PGN number, name, and decoder function
    // This makes it easy to add new custom decoders and maintain them in one place
    
    // NMEA2000 Standard PGN decoders
    m_customDecoders[126208] = {126208, "Group Function",              [](DBCDecoder* decoder, const tN2kMsg& msg) { return decoder->decodePGN126208(msg); }};
    m_customDecoders[126464] = {126464, "PGN List",                    [](DBCDecoder* decoder, const tN2kMsg& msg) { return decoder->decodePGN126464(msg); }};
    m_customDecoders[126996] = {126996, "Product Information",         [](DBCDecoder* decoder, const tN2kMsg& msg) { return decoder->decodePGN126996(msg); }};
    m_customDecoders[126998] = {126998, "Configuration Information",   [](DBCDecoder* decoder, const tN2kMsg& msg) { return decoder->decodePGN126998(msg); }};
    m_customDecoders[127501] = {127501, "Binary Switch Bank Status",   [](DBCDecoder* decoder, const tN2kMsg& msg) { return decoder->decodePGN127501(msg); }};
    
    // Lighting PGN decoders
    m_customDecoders[130330] = {130330, "Lighting System Settings",    [](DBCDecoder* decoder, const tN2kMsg& msg) { return decoder->decodePGN130330(msg); }};
    m_customDecoders[130561] = {130561, "Zone Lighting Control",       [](DBCDecoder* decoder, const tN2kMsg& msg) { return decoder->decodePGN130561(msg); }};
    m_customDecoders[130562] = {130562, "Lighting Scene",              [](DBCDecoder* decoder, const tN2kMsg& msg) { return decoder->decodePGN130562(msg); }};
    m_customDecoders[130563] = {130563, "Lighting Device",             [](DBCDecoder* decoder, const tN2kMsg& msg) { return decoder->decodePGN130563(msg); }};
    m_customDecoders[130564] = {130564, "Lighting Device Enumeration", [](DBCDecoder* decoder, const tN2kMsg& msg) { return decoder->decodePGN130564(msg); }};
    m_customDecoders[130565] = {130565, "Lighting Color Sequence",     [](DBCDecoder* decoder, const tN2kMsg& msg) { return decoder->decodePGN130565(msg); }};
    m_customDecoders[130566] = {130566, "Lighting Program",            [](DBCDecoder* decoder, const tN2kMsg& msg) { return decoder->decodePGN130566(msg); }};
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
    sigRed.value = QString("%1 (%2%)").arg(red).arg((red * 100.0 / 255.0), 0, 'f', 1);
    decoded.signalList.append(sigRed);

    // Field 4 - Green Component (0-255)
    uint8_t green = msg.GetByte(index);
    DecodedSignal sigGreen;
    sigGreen.name = "Green";
    sigGreen.isValid = true;
    sigGreen.value = QString("%1 (%2%)").arg(green).arg((green * 100.0 / 255.0), 0, 'f', 1);
    decoded.signalList.append(sigGreen);

    // Field 5 - Blue Component (0-255)
    uint8_t blue = msg.GetByte(index);
    DecodedSignal sigBlue;
    sigBlue.name = "Blue";
    sigBlue.isValid = true;
    sigBlue.value = QString("%1 (%2%)").arg(blue).arg((blue * 100.0 / 255.0), 0, 'f', 1);
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
    if (intensity <= 200) {
        sigIntensity.value = QString("%1 (%2%)").arg(intensity).arg((intensity / 2.0), 0, 'f', 1);
    } else {
        sigIntensity.value = QString("Out of range (%1)").arg(intensity);
    }
    decoded.signalList.append(sigIntensity);    

    // Field 8 - Program Id (0-255)
    uint8_t programId = msg.GetByte(index);
    DecodedSignal sigProgramId;
    sigProgramId.name = "Program Id";
    sigProgramId.isValid = true;
    if (programId >= 252) {
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
    if (programColorSeqIndex >= 252) {
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

    if (msg.DataLen < 19) {
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
    // Device Capabilities bitfield decoding
    QStringList capabilities;
    if (deviceCapabilities & 0x01) capabilities << "Dimmable";
    if (deviceCapabilities & 0x02) capabilities << "Programmable";
    if (deviceCapabilities & 0x04) capabilities << "Color Configurable";
    // Bits 3-7 are reserved for future use
    //if (deviceCapabilities & 0x08) capabilities << "Reserved (0x08)";
    //if (deviceCapabilities & 0x10) capabilities << "Reserved (0x10)";
    //if (deviceCapabilities & 0x20) capabilities << "Reserved (0x20)";
    //if (deviceCapabilities & 0x40) capabilities << "Reserved (0x40)";
    //if (deviceCapabilities & 0x80) capabilities << "Reserved (0x80)";
    if (capabilities.isEmpty()) capabilities << "Default";
    sigDeviceCapabilities.value = QString("%1 (0x%2)").arg(capabilities.join(", ")).arg(deviceCapabilities, 2, 16, QChar('0')).toUpper();
    decoded.signalList.append(sigDeviceCapabilities);

    // Field 3 - Color Capabilities (0-0xff)
    uint8_t colorCapabilities = msg.GetByte(index);
    DecodedSignal sigColorCapabilities;
    sigColorCapabilities.name = "Color Capabilities";
    sigColorCapabilities.isValid = true;
    QStringList capDesc;
    if (colorCapabilities == 0) {
        capDesc << "Not Changeable";
    } else {
        if (colorCapabilities & 0x01) capDesc << "R";
        if (colorCapabilities & 0x02) capDesc << "G";
        if (colorCapabilities & 0x04) capDesc << "B";
        if (colorCapabilities & 0x08) capDesc << "K";
        if (colorCapabilities & 0x10) capDesc << "Daylight (~65XXK)";
        if (colorCapabilities & 0x20) capDesc << "Warm (~35XXK)";
        // Join with comma separator
        capDesc = QStringList{capDesc.join(", ")};
        //if (colorCapabilities & 0x40) capDesc << "Reserved (future)";
        //if (colorCapabilities & 0x80) capDesc << "Reserved (future)";
    }
    sigColorCapabilities.value = QString("%1 (0x%2)").arg(capDesc.join(", ")).arg(colorCapabilities, 2, 16, QChar('0')).toUpper();
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
    DecodedSignal sigTotalNumberOfDevices;
    sigTotalNumberOfDevices.name = "Total Number of Devices";
    sigTotalNumberOfDevices.value = QString::number(totalNumberOfDevices);
    sigTotalNumberOfDevices.isValid = true;
    decoded.signalList.append(sigTotalNumberOfDevices);

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

    if (msg.DataLen < 2) {
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

    // Field 1 - Program ID (0-255)
    uint8_t progId = msg.GetByte(index);
    DecodedSignal sigProgId;
    sigProgId.name = "Program ID";
    sigProgId.value = QString::number(progId);
    sigProgId.isValid = true;
    decoded.signalList.append(sigProgId);

    // Field 2 - Program Name (variable string)
    char programName[80];
    size_t programNameSize = sizeof(programName);
    msg.GetVarStr(programNameSize, programName, index);
    DecodedSignal sigProgramName;
    sigProgramName.name = "Name";
    sigProgramName.isValid = true;
    sigProgramName.value = QString::fromUtf8(programName);
    decoded.signalList.append(sigProgramName);

    // Field 3 - Description (variable string)
    char programDescription[80];
    size_t programDescriptionSize = sizeof(programDescription);
    msg.GetVarStr(programDescriptionSize, programDescription, index);
    DecodedSignal sigProgramDescription;
    sigProgramDescription.name = "Description";
    sigProgramDescription.isValid = true;
    sigProgramDescription.value = QString::fromUtf8(programDescription);
    decoded.signalList.append(sigProgramDescription);

    // Field 4 - Program Capabilities (4 bits)
    uint8_t byte = msg.GetByte(index);
    uint8_t programCapabilities = byte & 0x0F;
    DecodedSignal sigProgramCapabilities;
    sigProgramCapabilities.name = "Capabilities";
    sigProgramCapabilities.isValid = true;
    sigProgramCapabilities.value = QString("0x%1").arg(programCapabilities, 1, 16, QChar('0')).toUpper();
    decoded.signalList.append(sigProgramCapabilities);
    if (programCapabilities & 0x01) {
        DecodedSignal sig;
        sig.name = "Program Color Sequence";
        sig.value = "Supported";
        sig.isValid = true;
        decoded.signalList.append(sig);
    }
    if (programCapabilities & 0x02) {
        DecodedSignal sig;
        sig.name = "Program Intensity";
        sig.value = "Supported";
        sig.isValid = true;
        decoded.signalList.append(sig);
    }
    if (programCapabilities & 0x04) {
        DecodedSignal sig;
        sig.name = "Program Rate";
        sig.value = "Supported";
        sig.isValid = true;
        decoded.signalList.append(sig);
    }
    if (programCapabilities & 0x08) {
        DecodedSignal sig;
        sig.name = "Program Color Rate";
        sig.value = "Supported";
        sig.isValid = true;
        decoded.signalList.append(sig);
    }

    // Field 5 - NMEA Reserved (4 bits)
    uint8_t reserved = byte & 0xF0;
    DecodedSignal sigReserved;
    sigReserved.name = "NMEA Reserved";
    sigReserved.value = QString("0x%1").arg(reserved, 1, 16, QChar('0')).toUpper();
    sigReserved.isValid = true;
    decoded.signalList.append(sigReserved);

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

    // Field 1 - Global Enable (2 bits)
    uint8_t byte = msg.GetByte(index);
    uint8_t globalEnable = byte & 0x03;
    DecodedSignal sigGlobalEnable;
    sigGlobalEnable.name = "Global Enable";
    sigGlobalEnable.value = QString("0x%1").arg(QString::number(globalEnable, 16).toUpper());
    sigGlobalEnable.isValid = true;
    decoded.signalList.append(sigGlobalEnable);

    // Field 2 - Default Settings (3 bits)
    uint8_t defaultSettings = (byte >> 2) & 0x07;
    DecodedSignal sigDefaultSettings;
    sigDefaultSettings.name = "Default Settings";
    sigDefaultSettings.value = QString("0x%1").arg(QString::number(defaultSettings, 16).toUpper());
    sigDefaultSettings.isValid = true;
    decoded.signalList.append(sigDefaultSettings);

    // Field 3 - NMEA Reserved (3 bits)
    uint8_t reserved = (byte >> 5) & 0x07;
    DecodedSignal sigReserved;
    sigReserved.name = "NMEA Reserved";
    sigReserved.value = QString("0x%1").arg(reserved, 1, 16, QChar('0')).toUpper();
    sigReserved.isValid = true;
    decoded.signalList.append(sigReserved);

    // Field 4 - Controller Name (variable string)
    char controllerName[80];
    size_t controllerNameSize = sizeof(controllerName);
    msg.GetVarStr(controllerNameSize, controllerName, index);
    DecodedSignal sigControllerName;
    sigControllerName.name = "Controller Name";
    sigControllerName.isValid = true;
    sigControllerName.value = QString::fromUtf8(controllerName);
    decoded.signalList.append(sigControllerName);

    // Field 5 - Max Scenes (0-255)
    uint8_t maxScenes = msg.GetByte(index);
    DecodedSignal sigMaxScenes;
    sigMaxScenes.name = "Max Scenes";
    sigMaxScenes.value = QString::number(maxScenes);
    sigMaxScenes.isValid = true;
    decoded.signalList.append(sigMaxScenes);

    // Field 6 - Max Scene Config Count (0-255)
    uint8_t maxSceneConfigCount = msg.GetByte(index);
    DecodedSignal sigMaxSceneConfigCount;
    sigMaxSceneConfigCount.name = "Max Scene Config Count";
    sigMaxSceneConfigCount.value = QString::number(maxSceneConfigCount);
    sigMaxSceneConfigCount.isValid = true;
    decoded.signalList.append(sigMaxSceneConfigCount);

    // Field 7 - Max Zones (0-255)
    uint8_t maxZones = msg.GetByte(index);
    DecodedSignal sigMaxZones;
    sigMaxZones.name = "Max Zones";
    sigMaxZones.value = QString::number(maxZones);
    sigMaxZones.isValid = true;
    decoded.signalList.append(sigMaxZones);

    // Field 8 - Max Color Sequences (0-255)
    uint8_t maxColorSequences = msg.GetByte(index);
    DecodedSignal sigMaxColorSequences;
    sigMaxColorSequences.name = "Max Color Sequences";
    sigMaxColorSequences.value = QString::number(maxColorSequences);
    sigMaxColorSequences.isValid = true;
    decoded.signalList.append(sigMaxColorSequences);

    // Field 9 - Max Color Seq Color Count (0-255)
    uint8_t maxColorSeqColorCount = msg.GetByte(index);
    DecodedSignal sigMaxColorSeqColorCount;
    sigMaxColorSeqColorCount.name = "Max Color Seq Color Count";
    sigMaxColorSeqColorCount.value = QString::number(maxColorSeqColorCount);
    sigMaxColorSeqColorCount.isValid = true;
    decoded.signalList.append(sigMaxColorSeqColorCount);

    // Field 10 - Number of Programs (0-255)
    uint8_t numPrograms = msg.GetByte(index);
    DecodedSignal sigNumPrograms;
    sigNumPrograms.name = "Number of Programs";
    sigNumPrograms.value = QString::number(numPrograms);
    sigNumPrograms.isValid = true;
    decoded.signalList.append(sigNumPrograms);

    // Field 11 - Controller Capabilities (8 bits)
    uint8_t controllerCapabilities = msg.GetByte(index);
    DecodedSignal sigControllerCapabilities;
    sigControllerCapabilities.name = "Controller Capabilities";
    sigControllerCapabilities.value = QString("0x%1").arg(controllerCapabilities, 1, 16, QChar('0')).toUpper();
    sigControllerCapabilities.isValid = true;
    decoded.signalList.append(sigControllerCapabilities);

    // Field 12 - Identify Device (4 bytes)
    uint32_t identifyDevice = msg.Get4ByteUInt(index);
    DecodedSignal sigIdentifyDevice;
    sigIdentifyDevice.name = "Identify Device";
    sigIdentifyDevice.value = QString("0x%1").arg(identifyDevice, 1, 16, QChar('0')).toUpper();
    sigIdentifyDevice.isValid = true;
    decoded.signalList.append(sigIdentifyDevice);

    return decoded;
}

// Binary Switch Bank Status Decoder
DecodedMessage DBCDecoder::decodePGN127501(const tN2kMsg& msg)
{
    DecodedMessage decoded;
    decoded.messageName = "Binary Switch Bank Status (127501)";
    decoded.description = "NMEA2000 Binary Switch Bank Status Message";
    decoded.isDecoded = true;

    if (msg.DataLen < 2) {
        DecodedSignal sig;
        sig.name = "Error";
        sig.value = "Message too short for PGN 127501";
        sig.isValid = false;
        decoded.signalList.append(sig);
        return decoded;
    }

    int index = 0;

    // Field 1 - Device/Bank Instance (1 byte)
    uint8_t bankInstance = msg.GetByte(index);
    DecodedSignal sigBankInstance;
    sigBankInstance.name = "Bank Instance";
    sigBankInstance.value = QString::number(bankInstance);
    sigBankInstance.isValid = true;
    decoded.signalList.append(sigBankInstance);

    // The remaining bytes contain the binary status data for up to 28 switches
    // Each switch uses 2 bits (4 switches per byte)
    // Bits 0-1: Switch 1, Bits 2-3: Switch 2, etc.
    
    // Calculate how many switches we can decode based on available data
    int availableDataBytes = msg.DataLen - 1; // Subtract 1 for instance byte
    int maxSwitches = qMin(28, availableDataBytes * 4); // 4 switches per byte, max 28
    
    for (int switchNum = 1; switchNum <= maxSwitches; switchNum++) {
        // Calculate which byte and which bits within that byte
        int byteIndex = (switchNum - 1) / 4;
        int bitOffset = ((switchNum - 1) % 4) * 2;
        
        if (index + byteIndex >= msg.DataLen) {
            break; // Safety check
        }
        
        uint8_t dataByte = msg.Data[index + byteIndex];
        uint8_t switchValue = (dataByte >> bitOffset) & 0x03; // Extract 2 bits
        
        DecodedSignal sigSwitch;
        sigSwitch.name = QString("Switch %1").arg(switchNum);
        sigSwitch.isValid = true;
        
        // Decode the 2-bit status value according to tN2kOnOff enumeration
        switch (switchValue) {
            case 0:
                sigSwitch.value = "Off";
                break;
            case 1:
                sigSwitch.value = "On";
                break;
            case 2:
                sigSwitch.value = "Error";
                break;
            case 3:
                sigSwitch.value = "Unavailable";
                break;
            default:
                sigSwitch.value = QString("Unknown (%1)").arg(switchValue);
                break;
        }
        
        decoded.signalList.append(sigSwitch);
    }
    
    // Add summary information
    if (maxSwitches > 0) {
        // Count active switches
        int activeCount = 0, errorCount = 0, unavailableCount = 0;
        for (const DecodedSignal& sig : decoded.signalList) {
            if (sig.name.startsWith("Switch ")) {
                if (sig.value == "On") activeCount++;
                else if (sig.value == "Error") errorCount++;
                else if (sig.value == "Unavailable") unavailableCount++;
            }
        }
        
        DecodedSignal sigSummary;
        sigSummary.name = "Summary";
        sigSummary.value = QString("%1 switches decoded: %2 on, %3 off, %4 error, %5 unavailable")
                          .arg(maxSwitches)
                          .arg(activeCount)
                          .arg(maxSwitches - activeCount - errorCount - unavailableCount)
                          .arg(errorCount)
                          .arg(unavailableCount);
        sigSummary.isValid = true;
        decoded.signalList.append(sigSummary);
    }

    return decoded;
}

// Dedicated function for decoding PGN 126208 (Group Function)
DecodedMessage DBCDecoder::decodePGN126208(const tN2kMsg& msg)
{
    DecodedMessage decoded;
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
    QString pgnName = getPGNDescription(targetPGN);
    if (!pgnName.isEmpty() && pgnName != "Unknown Range") {
        sigPGN.value = QString("%1 (%2)").arg(targetPGN).arg(pgnName);
    } else {
        sigPGN.value = QString("%1").arg(targetPGN);
    }
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
        sigField.name = getFieldName(targetPGN, fieldNum);
        sigField.value = fieldVal;
        sigField.isValid = true;
        decoded.signalList.append(sigField);
    }
    
    return decoded;
}

// Dedicated function for decoding PGN 126464 (PGN List)
DecodedMessage DBCDecoder::decodePGN126464(const tN2kMsg& msg)
{
    DecodedMessage decoded;
    decoded.messageName = "PGN List (126464)";
    decoded.description = "NMEA2000 PGN List - Transmit/Receive PGNs";
    decoded.isDecoded = true;

    // Fast packet handling - PGN 126464 is typically transmitted as a fast packet
    // The actual data starts after the fast packet header
    int dataStart = 0;
    
    // Check if this looks like a fast packet (sequence counter in first byte)
    if (msg.DataLen >= 2) {
        uint8_t sequenceCounter = msg.Data[0] & 0x1F;  // Lower 5 bits
        if (sequenceCounter == 0) {
            // This is the first frame of a fast packet
            uint8_t totalLength = msg.Data[1];
            dataStart = 2;  // Skip sequence counter and length
            
            DecodedSignal sigSeq;
            sigSeq.name = "Fast Packet Sequence";
            sigSeq.value = QString("First frame (0), Total length: %1 bytes").arg(totalLength);
            sigSeq.isValid = true;
            decoded.signalList.append(sigSeq);
        } else {
            // This is a continuation frame
            DecodedSignal sigSeq;
            sigSeq.name = "Fast Packet Sequence";
            sigSeq.value = QString("Continuation frame (%1)").arg(sequenceCounter);
            sigSeq.isValid = true;
            decoded.signalList.append(sigSeq);
            
            dataStart = 1;  // Skip sequence counter only
        }
    }

    // Check minimum length for actual PGN list data
    if (msg.DataLen < dataStart + 1) {
        DecodedSignal sig;
        sig.name = "Error";
        sig.value = "Too short for PGN 126464 decode";
        sig.isValid = true;
        decoded.signalList.append(sig);
        return decoded;
    }

    // Parse the PGN list data
    int offset = dataStart;
    
    // First byte: Function Code (PGN List type)
    if (offset < msg.DataLen) {
        uint8_t functionCode = msg.Data[offset++];
        
        DecodedSignal sigFC;
        sigFC.name = "Function Code";
        QString functionDescription;
        switch (functionCode) {
            case 0: functionDescription = "Transmit PGN List"; break;
            case 1: functionDescription = "Receive PGN List"; break;
            default: functionDescription = QString("Unknown (%1)").arg(functionCode); break;
        }
        sigFC.value = QString("%1 (%2)").arg(functionCode).arg(functionDescription);
        sigFC.isValid = true;
        decoded.signalList.append(sigFC);
    }

    // Parse PGN entries (each PGN is 3 bytes, little endian)
    int pgnIndex = 1;
    while (offset + 2 < msg.DataLen) {
        uint32_t pgn = msg.Data[offset] | (msg.Data[offset + 1] << 8) | (msg.Data[offset + 2] << 16);
        offset += 3;
        
        // Skip invalid/reserved PGNs (0x000000 and 0xFFFFFF are often used as padding)
        if (pgn == 0 || pgn == 0xFFFFFF) {
            continue;
        }
        
        DecodedSignal sigPGN;
        sigPGN.name = QString("PGN %1").arg(pgnIndex++);
        sigPGN.value = QString("%1").arg(pgn);
        sigPGN.description = getPGNDescription(pgn);
        sigPGN.isValid = true;
        decoded.signalList.append(sigPGN);
    }
    
    // If we didn't find any PGNs, indicate that
    if (pgnIndex == 1) {
        DecodedSignal sig;
        sig.name = "PGN Count";
        sig.value = "No valid PGNs found";
        sig.isValid = true;
        decoded.signalList.append(sig);
    } else {
        DecodedSignal sig;
        sig.name = "PGN Count";
        sig.value = QString("%1 PGNs").arg(pgnIndex - 1);
        sig.isValid = true;
        decoded.signalList.append(sig);
    }
    
    return decoded;
}

// Helper function to extract NMEA2000 strings that may be padded with 0xFF
QString extractNMEA2000String(const uint8_t* data, int length)
{
    // Find the first 0xFF or null terminator
    int actualLength = 0;
    for (int i = 0; i < length; i++) {
        if (data[i] == 0xFF || data[i] == 0x00) {
            break;
        }
        actualLength++;
    }
    
    // Create string from the actual content only
    if (actualLength == 0) {
        return QString();
    }
    
    QByteArray stringData(reinterpret_cast<const char*>(data), actualLength);
    return QString::fromLatin1(stringData).trimmed();
}

// Dedicated function for decoding PGN 126996 (Product Information)
DecodedMessage DBCDecoder::decodePGN126996(const tN2kMsg& msg)
{
    DecodedMessage decoded;
    decoded.messageName = "Product Information (126996)";
    decoded.description = "NMEA2000 Device Product Information";
    decoded.isDecoded = true;

    // Check minimum length for PGN 126996
    if (msg.DataLen < 134) {
        DecodedSignal sig;
        sig.name = "Error";
        sig.value = QString("Too short for PGN 126996 decode (got %1, need 134 bytes)").arg(msg.DataLen);
        sig.isValid = true;
        decoded.signalList.append(sig);
        return decoded;
    }

    int offset = 0;

    // Field 1: NMEA 2000 Version (2 bytes)
    if (offset + 1 < msg.DataLen) {
        uint16_t nmea2000Version = msg.Data[offset] | (msg.Data[offset + 1] << 8);
        DecodedSignal sig;
        sig.name = "1 - NMEA 2000 Version";
        sig.value = QString("%1").arg(nmea2000Version);
        sig.isValid = true;
        decoded.signalList.append(sig);
        offset += 2;
    }

    // Field 2: Product Code (2 bytes)
    if (offset + 1 < msg.DataLen) {
        uint16_t productCode = msg.Data[offset] | (msg.Data[offset + 1] << 8);
        DecodedSignal sig;
        sig.name = "2 - Product Code";
        sig.value = QString("%1").arg(productCode);
        sig.isValid = true;
        decoded.signalList.append(sig);
        offset += 2;
    }

    // Field 3: Model ID (32 bytes string)
    if (offset + 31 < msg.DataLen) {
        QString modelId = extractNMEA2000String(&msg.Data[offset], 32);
        DecodedSignal sig;
        sig.name = "3 - Model ID";
        sig.value = modelId.isEmpty() ? "Not specified" : modelId;
        sig.isValid = true;
        decoded.signalList.append(sig);
        offset += 32;
    }

    // Field 4: Software Version Code (32 bytes string)
    if (offset + 31 < msg.DataLen) {
        QString swVersion = extractNMEA2000String(&msg.Data[offset], 32);
        DecodedSignal sig;
        sig.name = "4 - Software Version Code";
        sig.value = swVersion.isEmpty() ? "Not specified" : swVersion;
        sig.isValid = true;
        decoded.signalList.append(sig);
        offset += 32;
    }

    // Field 5: Model Version (32 bytes string)
    if (offset + 31 < msg.DataLen) {
        QString modelVersion = extractNMEA2000String(&msg.Data[offset], 32);
        DecodedSignal sig;
        sig.name = "5 - Model Version";
        sig.value = modelVersion.isEmpty() ? "Not specified" : modelVersion;
        sig.isValid = true;
        decoded.signalList.append(sig);
        offset += 32;
    }

    // Field 6: Model Serial Code (32 bytes string)
    if (offset + 31 < msg.DataLen) {
        QString serialCode = extractNMEA2000String(&msg.Data[offset], 32);
        DecodedSignal sig;
        sig.name = "6 - Model Serial Code";
        sig.value = serialCode.isEmpty() ? "Not specified" : serialCode;
        sig.isValid = true;
        decoded.signalList.append(sig);
        offset += 32;
    }

    // Field 7: Certification Level (1 byte)
    if (offset < msg.DataLen) {
        uint8_t certLevel = msg.Data[offset];
        DecodedSignal sig;
        sig.name = "7 - Certification Level";
        sig.value = QString("%1").arg(certLevel);
        sig.isValid = true;
        decoded.signalList.append(sig);
        offset += 1;
    }

    // Field 8: Load Equivalency (1 byte)
    if (offset < msg.DataLen) {
        uint8_t loadEquiv = msg.Data[offset];
        DecodedSignal sig;
        sig.name = "8 - Load Equivalency";
        sig.value = QString("%1").arg(loadEquiv);
        sig.isValid = true;
        decoded.signalList.append(sig);
        offset += 1;
    }

    return decoded;
}

// Dedicated function for decoding PGN 126998 (Configuration Information)
DecodedMessage DBCDecoder::decodePGN126998(const tN2kMsg& msg)
{
    DecodedMessage decoded;
    decoded.messageName = "Configuration Information (126998)";
    decoded.description = "NMEA2000 Device Configuration Information";
    decoded.isDecoded = true;

    // Parse using the official NMEA2000 library function
    char ManufacturerInformation[71] = {0};
    char InstallationDescription1[71] = {0};
    char InstallationDescription2[71] = {0};
    size_t ManufacturerInformationSize = sizeof(ManufacturerInformation);
    size_t InstallationDescription1Size = sizeof(InstallationDescription1);
    size_t InstallationDescription2Size = sizeof(InstallationDescription2);
    
    bool parseSuccess = ParseN2kPGN126998(msg, 
                          ManufacturerInformationSize, ManufacturerInformation,
                          InstallationDescription1Size, InstallationDescription1,
                          InstallationDescription2Size, InstallationDescription2);
    
    DecodedSignal parseResult;
    parseResult.name = "Parse Result";
    parseResult.value = parseSuccess ? "SUCCESS" : "FAILED";
    parseResult.isValid = true;
    decoded.signalList.append(parseResult);
    
    if (parseSuccess) {
        // Helper function to decode NMEA2000 strings (which can be ASCII or Unicode with SOH prefix)
        auto decodeN2kString = [](const char* rawStr) -> QString {
            if (!rawStr || rawStr[0] == '\0') {
                return QString("(empty)");
            }
            
            // Check if string starts with SOH (Start of Header, 0x01) indicating Unicode
            if (rawStr[0] == '\x01') {
                // Unicode string - skip SOH and interpret as UTF-8
                return QString::fromUtf8(rawStr + 1);
            } else {
                // ASCII string
                return QString::fromLatin1(rawStr);
            }
        };
        
        // Manufacturer Information
        DecodedSignal sigMfg;
        sigMfg.name = "Manufacturer Information";
        sigMfg.value = decodeN2kString(ManufacturerInformation);
        sigMfg.isValid = true;
        decoded.signalList.append(sigMfg);
        
        // Installation Description 1
        DecodedSignal sigDesc1;
        sigDesc1.name = "Installation Description 1";
        sigDesc1.value = decodeN2kString(InstallationDescription1);
        sigDesc1.isValid = true;
        decoded.signalList.append(sigDesc1);
        
        // Installation Description 2
        DecodedSignal sigDesc2;
        sigDesc2.name = "Installation Description 2";
        sigDesc2.value = decodeN2kString(InstallationDescription2);
        sigDesc2.isValid = true;
        decoded.signalList.append(sigDesc2);
        
    } else {
        DecodedSignal sig;
        sig.name = "Error";
        sig.value = "Failed to parse Configuration Information";
        sig.isValid = true;
        decoded.signalList.append(sig);
    }
    
    return decoded;
}

QString DBCDecoder::getFieldName(unsigned long pgn, uint8_t fieldNumber) const
{
    switch (pgn) {
        case 130561: // Zone Lighting Control
            switch (fieldNumber) {
                case 1: return "Field 1 - Zone ID";
                case 2: return "Field 2 - Zone Name";
                case 3: return "Field 3 - Red";
                case 4: return "Field 4 - Green";
                case 5: return "Field 5 - Blue";
                case 6: return "Field 6 - Color Temperature";
                case 7: return "Field 7 - Intensity";
                case 8: return "Field 8 - Program ID";
                case 9: return "Field 9 - Program Color Seq Index";
                case 10: return "Field 10 - Program Intensity";
                case 11: return "Field 11 - Program Rate";
                case 12: return "Field 12 - Program Color Sequence";
                case 13: return "Field 13 - Zone Enabled";
                default: return QString("Field %1").arg(fieldNumber);
            }
            
        case 130562: // Lighting Scene
            switch (fieldNumber) {
                case 1: return "Field 1 - Scene ID";
                case 2: return "Field 2 - Scene Name";
                case 3: return "Field 3 - Scene State";
                case 4: return "Field 4 - Scene Description";
                default: return QString("Field %1").arg(fieldNumber);
            }
            
        case 130563: // Lighting Device
            switch (fieldNumber) {
                case 1: return "Field 1 - Device Instance";
                case 2: return "Field 2 - Device Name";
                case 3: return "Field 3 - Device Type";
                case 4: return "Field 4 - Device Status";
                case 5: return "Field 5 - Firmware Version";
                case 6: return "Field 6 - Hardware Version";
                case 7: return "Field 7 - Serial Number";
                case 8: return "Field 8 - Manufacturer Code";
                case 9: return "Field 9 - Industry Code";
                case 10: return "Field 10 - Device Function";
                case 11: return "Field 11 - Device Class";
                case 12: return "Field 12 - System Instance";
                default: return QString("Field %1").arg(fieldNumber);
            }
            
        case 130564: // Lighting Device Enumeration
            switch (fieldNumber) {
                case 1: return "Field 1 - Device Index";
                case 2: return "Field 2 - Device Instance";
                case 3: return "Field 3 - Device Name";
                case 4: return "Field 4 - Device Type";
                default: return QString("Field %1").arg(fieldNumber);
            }
            
        case 130565: // Lighting Color Sequence
            switch (fieldNumber) {
                case 1: return "Field 1 - Sequence ID";
                case 2: return "Field 2 - Sequence Name";
                case 3: return "Field 3 - Color Count";
                case 4: return "Field 4 - Color Index";
                case 5: return "Field 5 - Red";
                case 6: return "Field 6 - Green";
                case 7: return "Field 7 - Blue";
                case 8: return "Field 8 - Duration";
                default: return QString("Field %1").arg(fieldNumber);
            }
            
        case 130566: // Lighting Program
            switch (fieldNumber) {
                case 1: return "Field 1 - Program ID";
                case 2: return "Field 2 - Program Name";
                case 3: return "Field 3 - Program Type";
                case 4: return "Field 4 - Program State";
                case 5: return "Field 5 - Program Description";
                default: return QString("Field %1").arg(fieldNumber);
            }
            
        case 130330: // Lighting System Settings
            switch (fieldNumber) {
                case 1: return "Field 1 - System Instance";
                case 2: return "Field 2 - System Name";
                case 3: return "Field 3 - Global Brightness";
                case 4: return "Field 4 - Power State";
                case 5: return "Field 5 - Default Scene";
                default: return QString("Field %1").arg(fieldNumber);
            }
            
        case 130567: // Lighting Zone Configuration
            switch (fieldNumber) {
                case 1: return "Field 1 - Zone ID";
                case 2: return "Field 2 - Zone Name";
                case 3: return "Field 3 - Zone Type";
                case 4: return "Field 4 - Zone Location";
                case 5: return "Field 5 - Default Program";
                case 6: return "Field 6 - Default Brightness";
                case 7: return "Field 7 - Default Color";
                case 8: return "Field 8 - Zone Group";
                default: return QString("Field %1").arg(fieldNumber);
            }
            
        case 130568: // Lighting Device Configuration
            switch (fieldNumber) {
                case 1: return "Field 1 - Device Instance";
                case 2: return "Field 2 - Device Address";
                case 3: return "Field 3 - Device Type";
                case 4: return "Field 4 - Max Zones";
                case 5: return "Field 5 - Supported Programs";
                case 6: return "Field 6 - Color Capability";
                case 7: return "Field 7 - Dimming Capability";
                case 8: return "Field 8 - Device Name";
                default: return QString("Field %1").arg(fieldNumber);
            }
            
        case 130847: // Lighting Control/Status (Proprietary)
            switch (fieldNumber) {
                case 1: return "Field 1 - Control Command";
                case 2: return "Field 2 - Zone ID";
                case 3: return "Field 3 - Brightness";
                case 4: return "Field 4 - Red Value";
                case 5: return "Field 5 - Green Value";
                case 6: return "Field 6 - Blue Value";
                case 7: return "Field 7 - White Value";
                case 8: return "Field 8 - Program ID";
                case 9: return "Field 9 - Speed/Rate";
                case 10: return "Field 10 - Direction";
                case 11: return "Field 11 - Status";
                default: return QString("Field %1").arg(fieldNumber);
            }
            
        case 131072: // Lighting System Command (Proprietary)
            switch (fieldNumber) {
                case 1: return "Field 1 - System Command";
                case 2: return "Field 2 - Zone Mask";
                case 3: return "Field 3 - Global Brightness";
                case 4: return "Field 4 - Scene ID";
                case 5: return "Field 5 - Program ID";
                case 6: return "Field 6 - Color Mode";
                case 7: return "Field 7 - Transition Time";
                default: return QString("Field %1").arg(fieldNumber);
            }
            
        // Add more PGNs as needed
        default:
            return QString("Field %1").arg(fieldNumber);
    }
}

// Helper function to get PGN descriptions
QString DBCDecoder::getPGNDescription(uint32_t pgn)
{
    // Common NMEA2000 PGN descriptions
    static QMap<uint32_t, QString> pgnDescriptions;
    
    // Initialize the map if it's empty
    if (pgnDescriptions.isEmpty()) {
        // System PGNs
        pgnDescriptions[59392] = "ISO Acknowledgement";
        pgnDescriptions[59904] = "ISO Request";
        pgnDescriptions[60416] = "ISO Transport Protocol - Data Transfer";
        pgnDescriptions[60160] = "ISO Transport Protocol - Connection Management";
        
        // Standard NMEA2000 PGNs
        pgnDescriptions[126208] = "Group Function";
        pgnDescriptions[126464] = "PGN List";
        pgnDescriptions[126992] = "System Time";
        pgnDescriptions[126993] = "Heartbeat";
        pgnDescriptions[126996] = "Product Information";
        pgnDescriptions[126998] = "Configuration Information";
        
        // Navigation PGNs
        pgnDescriptions[127245] = "Rudder";
        pgnDescriptions[127250] = "Vessel Heading";
        pgnDescriptions[127251] = "Rate of Turn";
        pgnDescriptions[127257] = "Attitude";
        pgnDescriptions[127258] = "Magnetic Variation";
        pgnDescriptions[129025] = "Position (Rapid Update)";
        pgnDescriptions[129026] = "COG & SOG (Rapid Update)";
        pgnDescriptions[129029] = "GNSS Position Data";
        pgnDescriptions[129033] = "Time & Date";
        pgnDescriptions[129283] = "Cross Track Error";
        pgnDescriptions[129284] = "Navigation Data";
        pgnDescriptions[129285] = "Navigation Route/WP Information";
        
        // Engine PGNs
        pgnDescriptions[127488] = "Engine Parameters (Rapid Update)";
        pgnDescriptions[127489] = "Engine Parameters (Dynamic)";
        pgnDescriptions[127493] = "Transmission Parameters (Dynamic)";
        pgnDescriptions[127497] = "Trip Parameters (Engine)";
        pgnDescriptions[127498] = "Trip Parameters (Vessel)";
        pgnDescriptions[127500] = "Load Controller Connection State/Control";
        pgnDescriptions[127501] = "Binary Switch Bank Status";
        pgnDescriptions[127502] = "Switch Bank Control";
        pgnDescriptions[127503] = "AC Input Status";
        pgnDescriptions[127504] = "AC Output Status";
        pgnDescriptions[127505] = "Fluid Level";
        pgnDescriptions[127506] = "DC Detailed Status";
        pgnDescriptions[127507] = "Charger Status";
        pgnDescriptions[127508] = "Battery Status";
        pgnDescriptions[127509] = "Inverter Status";
        
        // Environmental PGNs
        pgnDescriptions[128259] = "Speed (Water Referenced)";
        pgnDescriptions[128267] = "Water Depth";
        pgnDescriptions[128275] = "Distance Log";
        pgnDescriptions[130306] = "Wind Data";
        pgnDescriptions[130310] = "Environmental Parameters";
        pgnDescriptions[130311] = "Environmental Parameters";
        pgnDescriptions[130312] = "Temperature";
        pgnDescriptions[130313] = "Humidity";
        pgnDescriptions[130314] = "Actual Pressure";
        pgnDescriptions[130316] = "Temperature (Extended Range)";
        
        // Lighting PGNs (Lumitec specific)
        pgnDescriptions[130330] = "Lighting System Settings";
        pgnDescriptions[130561] = "Zone Lighting Control";
        pgnDescriptions[130562] = "Lighting Scene";
        pgnDescriptions[130563] = "Lighting Device";
        pgnDescriptions[130564] = "Lighting Device Enumeration";
        pgnDescriptions[130565] = "Lighting Color Sequence";
        pgnDescriptions[130566] = "Lighting Program";
        
        // AIS PGNs
        pgnDescriptions[129038] = "AIS Class A Position Report";
        pgnDescriptions[129039] = "AIS Class B Position Report";
        pgnDescriptions[129040] = "AIS Class B Extended Position Report";
        pgnDescriptions[129041] = "AIS Aids to Navigation (AtoN) Report";
        pgnDescriptions[129793] = "AIS UTC and Date Report";
        pgnDescriptions[129794] = "AIS Class A Static and Voyage Related Data";
        pgnDescriptions[129798] = "AIS SAR Aircraft Position Report";
        pgnDescriptions[129802] = "AIS Safety Related Broadcast Message";
        pgnDescriptions[129809] = "AIS Class B Static Data (Part A)";
        pgnDescriptions[129810] = "AIS Class B Static Data (Part B)";
    }
    
    // Look up the PGN description
    if (pgnDescriptions.contains(pgn)) {
        return pgnDescriptions[pgn];
    }
    
    // If not found, provide a generic description based on PGN range
    if (pgn >= 126208 && pgn <= 126463) {
        return "Network Management/Group Function";
    } else if (pgn >= 126464 && pgn <= 126719) {
        return "Proprietary Range A";
    } else if (pgn >= 126720 && pgn <= 126975) {
        return "Proprietary Range B";
    } else if (pgn >= 126976 && pgn <= 127231) {
        return "Network Management";
    } else if (pgn >= 127232 && pgn <= 127487) {
        return "Steering/Navigation";
    } else if (pgn >= 127488 && pgn <= 127743) {
        return "Propulsion";
    } else if (pgn >= 127744 && pgn <= 127999) {
        return "Navigation";
    } else if (pgn >= 128000 && pgn <= 128255) {
        return "Communication";
    } else if (pgn >= 128256 && pgn <= 128511) {
        return "Instrumentation/General";
    } else if (pgn >= 129024 && pgn <= 129279) {
        return "Navigation";
    } else if (pgn >= 129280 && pgn <= 129535) {
        return "Navigation";
    } else if (pgn >= 129536 && pgn <= 129791) {
        return "Communication";
    } else if (pgn >= 129792 && pgn <= 130047) {
        return "Communication/AIS";
    } else if (pgn >= 130048 && pgn <= 130303) {
        return "Instrumentation/General";
    } else if (pgn >= 130304 && pgn <= 130559) {
        return "Environmental";
    } else if (pgn >= 130560 && pgn <= 130815) {
        return "Proprietary/Lighting";
    } else {
        return "Unknown Range";
    }
}