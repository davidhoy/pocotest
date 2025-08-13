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
    // First try to load from a local NMEA2000 DBC file
    bool dbcLoaded = false;
    
    // Try to load from local file first
    if (QFile::exists("nmea2000.dbc")) {
        dbcLoaded = loadDBCFile("nmea2000.dbc");
        qDebug() << "Loaded local DBC file:" << dbcLoaded;
    }
    
    // If no local file, use fallback hardcoded definitions
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
                unsigned long pgn = match.captured(1).toULong();
                QString name = match.captured(2);
                int dlc = match.captured(3).toInt();
                
                DBCMessage message;
                message.pgn = pgn;
                message.name = name;
                message.dlc = dlc;
                message.description = name; // Use name as description for now
                
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
    // Initialize standard NMEA2000 message definitions
    defineEngineParametersRapid();
    defineEngineParametersDynamic();
    definePositionRapidUpdate();
    defineCOGSOGRapidUpdate();
    defineGNSSPositionData();
    defineWindData();
    defineTemperature();
    defineFluidLevel();
    defineBatteryStatus();
    defineProductInformation();
    defineConfigurationInformation();
    defineBinarySwitch();
    defineActualPressure();
}

void DBCDecoder::defineEngineParametersRapid()
{
    DBCMessage msg;
    msg.pgn = 127488;
    msg.name = "Engine Parameters, Rapid Update";
    msg.description = "High frequency engine data";
    msg.dlc = 8;
    
    DBCSignal signal;
    
    // Engine Instance
    signal.name = "Engine Instance";
    signal.startBit = 0;
    signal.bitLength = 8;
    signal.isSigned = false;
    signal.scale = 1.0;
    signal.offset = 0.0;
    signal.minimum = 0;
    signal.maximum = 251;
    signal.unit = "";
    signal.description = "Engine instance";
    msg.signalList.append(signal);
    
    // Engine Speed
    signal.name = "Engine Speed";
    signal.startBit = 8;
    signal.bitLength = 16;
    signal.isSigned = false;
    signal.scale = 0.25;
    signal.offset = 0.0;
    signal.minimum = 0;
    signal.maximum = 16383.75;
    signal.unit = "rpm";
    signal.description = "Engine rotational speed";
    msg.signalList.append(signal);
    
    // Engine Boost Pressure
    signal.name = "Engine Boost Pressure";
    signal.startBit = 24;
    signal.bitLength = 16;
    signal.isSigned = false;
    signal.scale = 100.0;
    signal.offset = 0.0;
    signal.minimum = 0;
    signal.maximum = 6553500;
    signal.unit = "Pa";
    signal.description = "Engine boost (inlet manifold) pressure";
    msg.signalList.append(signal);
    
    // Engine Tilt/Trim
    signal.name = "Engine Tilt/Trim";
    signal.startBit = 40;
    signal.bitLength = 8;
    signal.isSigned = true;
    signal.scale = 1.0;
    signal.offset = 0.0;
    signal.minimum = -125;
    signal.maximum = 125;
    signal.unit = "%";
    signal.description = "Current engine tilt/trim";
    msg.signalList.append(signal);
    
    addMessage(msg);
}

void DBCDecoder::defineWindData()
{
    DBCMessage msg;
    msg.pgn = 130306;
    msg.name = "Wind Data";
    msg.description = "Wind speed and direction";
    msg.dlc = 6;
    
    DBCSignal signal;
    
    // SID
    signal.name = "SID";
    signal.startBit = 0;
    signal.bitLength = 8;
    signal.isSigned = false;
    signal.scale = 1.0;
    signal.offset = 0.0;
    signal.minimum = 0;
    signal.maximum = 252;
    signal.unit = "";
    signal.description = "Sequence identifier";
    msg.signalList.append(signal);
    
    // Wind Speed
    signal.name = "Wind Speed";
    signal.startBit = 8;
    signal.bitLength = 16;
    signal.isSigned = false;
    signal.scale = 0.01;
    signal.offset = 0.0;
    signal.minimum = 0;
    signal.maximum = 655.34;
    signal.unit = "m/s";
    signal.description = "Wind speed";
    msg.signalList.append(signal);
    
    // Wind Direction
    signal.name = "Wind Direction";
    signal.startBit = 24;
    signal.bitLength = 16;
    signal.isSigned = false;
    signal.scale = 0.0001;
    signal.offset = 0.0;
    signal.minimum = 0;
    signal.maximum = 6.2831;
    signal.unit = "rad";
    signal.description = "Wind direction relative to vessel";
    msg.signalList.append(signal);
    
    // Wind Reference
    signal.name = "Wind Reference";
    signal.startBit = 40;
    signal.bitLength = 3;
    signal.isSigned = false;
    signal.scale = 1.0;
    signal.offset = 0.0;
    signal.minimum = 0;
    signal.maximum = 7;
    signal.unit = "";
    signal.description = "Wind reference";
    signal.valueDescriptions[0] = "True (ground referenced to North)";
    signal.valueDescriptions[1] = "Magnetic (ground referenced to Magnetic North)";
    signal.valueDescriptions[2] = "Apparent";
    signal.valueDescriptions[3] = "True (boat referenced)";
    signal.valueDescriptions[4] = "True (water referenced)";
    msg.signalList.append(signal);
    
    addMessage(msg);
}

void DBCDecoder::defineTemperature()
{
    DBCMessage msg;
    msg.pgn = 130312;
    msg.name = "Temperature";
    msg.description = "Temperature measurement";
    msg.dlc = 8;
    
    DBCSignal signal;
    
    // SID
    signal.name = "SID";
    signal.startBit = 0;
    signal.bitLength = 8;
    signal.isSigned = false;
    signal.scale = 1.0;
    signal.offset = 0.0;
    signal.minimum = 0;
    signal.maximum = 252;
    signal.unit = "";
    signal.description = "Sequence identifier";
    msg.signalList.append(signal);
    
    // Temperature Instance
    signal.name = "Temperature Instance";
    signal.startBit = 8;
    signal.bitLength = 8;
    signal.isSigned = false;
    signal.scale = 1.0;
    signal.offset = 0.0;
    signal.minimum = 0;
    signal.maximum = 252;
    signal.unit = "";
    signal.description = "Temperature instance";
    msg.signalList.append(signal);
    
    // Temperature Source
    signal.name = "Temperature Source";
    signal.startBit = 16;
    signal.bitLength = 8;
    signal.isSigned = false;
    signal.scale = 1.0;
    signal.offset = 0.0;
    signal.minimum = 0;
    signal.maximum = 252;
    signal.unit = "";
    signal.description = "Temperature source";
    signal.valueDescriptions[0] = "Sea Temperature";
    signal.valueDescriptions[1] = "Outside Temperature";
    signal.valueDescriptions[2] = "Inside Temperature";
    signal.valueDescriptions[3] = "Engine Room Temperature";
    signal.valueDescriptions[4] = "Main Cabin Temperature";
    signal.valueDescriptions[5] = "Live Well Temperature";
    signal.valueDescriptions[6] = "Bait Well Temperature";
    signal.valueDescriptions[7] = "Refrigeration Temperature";
    signal.valueDescriptions[8] = "Heating System Temperature";
    signal.valueDescriptions[9] = "Dewpoint Temperature";
    signal.valueDescriptions[10] = "Apparent Wind Chill Temperature";
    signal.valueDescriptions[11] = "Theoretical Wind Chill Temperature";
    signal.valueDescriptions[12] = "Heat Index Temperature";
    signal.valueDescriptions[13] = "Freezer Temperature";
    signal.valueDescriptions[14] = "Exhaust Gas Temperature";
    msg.signalList.append(signal);
    
    // Actual Temperature
    signal.name = "Actual Temperature";
    signal.startBit = 24;
    signal.bitLength = 16;
    signal.isSigned = false;
    signal.scale = 0.01;
    signal.offset = -273.15;
    signal.minimum = -273.15;
    signal.maximum = 381.85;
    signal.unit = "K";
    signal.description = "Actual temperature";
    msg.signalList.append(signal);
    
    // Set Temperature
    signal.name = "Set Temperature";
    signal.startBit = 40;
    signal.bitLength = 16;
    signal.isSigned = false;
    signal.scale = 0.01;
    signal.offset = -273.15;
    signal.minimum = -273.15;
    signal.maximum = 381.85;
    signal.unit = "K";
    signal.description = "Set temperature";
    msg.signalList.append(signal);
    
    addMessage(msg);
}

void DBCDecoder::defineBatteryStatus()
{
    DBCMessage msg;
    msg.pgn = 127508;
    msg.name = "Battery Status";
    msg.description = "DC battery status information";
    msg.dlc = 8;
    
    DBCSignal signal;
    
    // Battery Instance
    signal.name = "Battery Instance";
    signal.startBit = 0;
    signal.bitLength = 8;
    signal.isSigned = false;
    signal.scale = 1.0;
    signal.offset = 0.0;
    signal.minimum = 0;
    signal.maximum = 252;
    signal.unit = "";
    signal.description = "Battery instance";
    msg.signalList.append(signal);
    
    // Battery Voltage
    signal.name = "Battery Voltage";
    signal.startBit = 8;
    signal.bitLength = 16;
    signal.isSigned = false;
    signal.scale = 0.01;
    signal.offset = 0.0;
    signal.minimum = 0;
    signal.maximum = 655.34;
    signal.unit = "V";
    signal.description = "Battery terminal voltage";
    msg.signalList.append(signal);
    
    // Battery Current
    signal.name = "Battery Current";
    signal.startBit = 24;
    signal.bitLength = 16;
    signal.isSigned = true;
    signal.scale = 0.1;
    signal.offset = 0.0;
    signal.minimum = -3276.7;
    signal.maximum = 3276.6;
    signal.unit = "A";
    signal.description = "Battery current (+ve = charging, -ve = discharging)";
    msg.signalList.append(signal);
    
    // Battery Temperature
    signal.name = "Battery Temperature";
    signal.startBit = 40;
    signal.bitLength = 16;
    signal.isSigned = false;
    signal.scale = 0.01;
    signal.offset = -273.15;
    signal.minimum = -273.15;
    signal.maximum = 381.85;
    signal.unit = "K";
    signal.description = "Battery temperature";
    msg.signalList.append(signal);
    
    addMessage(msg);
}

void DBCDecoder::definePositionRapidUpdate()
{
    DBCMessage msg;
    msg.pgn = 129025;
    msg.name = "Position, Rapid Update";
    msg.description = "GPS position data at high frequency";
    msg.dlc = 8;
    
    DBCSignal signal;
    
    // Latitude
    signal.name = "Latitude";
    signal.startBit = 0;
    signal.bitLength = 32;
    signal.isSigned = true;
    signal.scale = 1e-7;
    signal.offset = 0.0;
    signal.minimum = -214.7483647;
    signal.maximum = 214.7483646;
    signal.unit = "deg";
    signal.description = "Vessel latitude";
    msg.signalList.append(signal);
    
    // Longitude
    signal.name = "Longitude";
    signal.startBit = 32;
    signal.bitLength = 32;
    signal.isSigned = true;
    signal.scale = 1e-7;
    signal.offset = 0.0;
    signal.minimum = -214.7483647;
    signal.maximum = 214.7483646;
    signal.unit = "deg";
    signal.description = "Vessel longitude";
    msg.signalList.append(signal);
    
    addMessage(msg);
}

// Placeholder implementations for other messages
void DBCDecoder::defineEngineParametersDynamic() { /* TODO */ }
void DBCDecoder::defineCOGSOGRapidUpdate() { /* TODO */ }
void DBCDecoder::defineGNSSPositionData() { /* TODO */ }
void DBCDecoder::defineFluidLevel() { /* TODO */ }
void DBCDecoder::defineProductInformation() { /* TODO */ }
void DBCDecoder::defineConfigurationInformation() { /* TODO */ }
void DBCDecoder::defineBinarySwitch() { /* TODO */ }
void DBCDecoder::defineActualPressure() { /* TODO */ }

void DBCDecoder::addMessage(const DBCMessage& message)
{
    m_messages[message.pgn] = message;
}

DecodedMessage DBCDecoder::decodeMessage(const tN2kMsg& msg)
{
    DecodedMessage decoded;
    decoded.isDecoded = false;
    
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
    int startByte = signal.startBit / 8;
    int startBitInByte = signal.startBit % 8;
    int remainingBits = signal.bitLength;
    
    for (int i = startByte; i < 8 && remainingBits > 0; i++) {
        int bitsToRead = qMin(8 - (i == startByte ? startBitInByte : 0), remainingBits);
        int shift = i == startByte ? startBitInByte : 0;
        uint8_t mask = ((1 << bitsToRead) - 1) << shift;
        uint8_t maskedByte = (data[i] & mask) >> shift;
        
        rawValue |= ((uint64_t)maskedByte) << ((i - startByte) * 8 - (i == startByte ? 0 : startBitInByte));
        remainingBits -= bitsToRead;
    }
    
    // Handle signed values
    if (signal.isSigned && signal.bitLength < 64) {
        uint64_t signBit = 1ULL << (signal.bitLength - 1);
        if (rawValue & signBit) {
            rawValue |= ~((1ULL << signal.bitLength) - 1);
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
    return m_messages.contains(pgn);
}

QString DBCDecoder::getMessageName(unsigned long pgn) const
{
    if (m_messages.contains(pgn)) {
        return m_messages[pgn].name;
    }
    return QString("PGN %1").arg(pgn);
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
                part += QString("%1").arg(signal.value.toDouble(), 0, 'f', 2);
            } else {
                part += signal.value.toString();
            }
            
            if (!signal.unit.isEmpty()) {
                part += " " + signal.unit;
            }
            
            parts.append(part);
        }
    }
    
    return parts.join(", ");
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
