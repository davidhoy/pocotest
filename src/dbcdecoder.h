#ifndef DBCDECODER_H
#define DBCDECODER_H

#include <QObject>
#include <QString>
#include <QList>
#include <QMap>
#include <QVariant>
#include <functional>
#include <N2kMsg.h>

struct DBCSignal {
    QString name;
    int startBit;
    int bitLength;
    bool isSigned;
    double scale;
    double offset;
    double minimum;
    double maximum;
    QString unit;
    QString description;
    QMap<int, QString> valueDescriptions; // For enumerated values
};

struct DBCMessage {
    unsigned long pgn;
    QString name;
    QString description;
    int dlc;
    QList<DBCSignal> signalList;
};

struct DecodedSignal {
    QString name;
    QString unit;
    QString description;
    QVariant value;
    bool isValid;
};

struct DecodedMessage {
    QString messageName;
    QString description;
    QList<DecodedSignal> signalList;
    bool isDecoded;
};

class DBCDecoder : public QObject
{
    Q_OBJECT

public:
    explicit DBCDecoder(QObject *parent = nullptr);
    ~DBCDecoder();

    // DBC file loading
    bool loadDBCFile(const QString& filePath);
    bool loadDBCFromUrl(const QString& url);
    
    // Main decode function
    DecodedMessage decodeMessage(const tN2kMsg& msg);
    QString decodePGN(const tN2kMsg& msg);
    
    // Helper functions
    bool canDecode(unsigned long pgn) const;
    QString getMessageName(unsigned long pgn) const;
    QString getCleanMessageName(unsigned long pgn) const;  // Enhanced message name formatting
    QString getFormattedDecoded(const tN2kMsg& msg);
    QString getFormattedDecodedForSave(const tN2kMsg& msg);  // Format without reserved fields for saving
    QString formatSignalValue(const DecodedSignal& signal);
    
    // Status functions
    int getLoadedMessageCount() const;
    QStringList getAvailablePGNs() const;
    bool isInitialized() const;  // Status check for compatibility
    QString getDecoderInfo() const;  // Enhanced decoder status info
    QList<unsigned long> getCustomDecoderPGNs() const;  // Get list of PGNs with custom decoders
    bool hasCustomDecoder(unsigned long pgn) const;     // Check if PGN has custom decoder

private:
    void initializeStandardNMEA2000();
    void addMessage(const DBCMessage& message);
    
    // DBC file parsing
    bool parseDBCFile(const QString& content);
    DBCMessage parseDBCMessage(const QString& messageLines);
    DBCSignal parseDBCSignal(const QString& signalLine);
    void parseValueTable(const QString& line, DBCSignal& signal);
    
    // Signal extraction and validation
    double extractSignalValue(const uint8_t* data, const DBCSignal& signal);
    bool isSignalValid(double rawValue, const DBCSignal& signal);
    
    // Field name mapping for group functions
    QString getFieldName(unsigned long pgn, uint8_t fieldNumber) const;
    
    // Field size mapping for group functions
    int getFieldSize(unsigned long pgn, uint8_t fieldNumber) const;
    
    // Special group function decoders
    void decodePGN130565GroupFunction(DecodedMessage& decoded, const tN2kMsg& msg, int& paramPos, uint8_t numParams);
    
    // Helper functions for PGN descriptions
    QString getPGNDescription(uint32_t pgn);
    
    // Lighting PGN decoders
    DecodedMessage decodePGN130330(const tN2kMsg& msg);  // Lighting System Settings
    DecodedMessage decodePGN130561(const tN2kMsg& msg);  // Zone Lighting Control
    DecodedMessage decodePGN130562(const tN2kMsg& msg);  // Lighting Scene
    DecodedMessage decodePGN130563(const tN2kMsg& msg);  // Lighting Device
    DecodedMessage decodePGN130564(const tN2kMsg& msg);  // Lighting Device Enumeration
    DecodedMessage decodePGN130565(const tN2kMsg& msg);  // Lighting Color Sequence
    DecodedMessage decodePGN130566(const tN2kMsg& msg);  // Lighting Program
    
    // NMEA2000 Standard PGN decoders
    DecodedMessage decodePGN126208(const tN2kMsg& msg);  // Group Function
    DecodedMessage decodePGN126464(const tN2kMsg& msg);  // PGN List
    DecodedMessage decodePGN126996(const tN2kMsg& msg);  // Product Information
    DecodedMessage decodePGN126998(const tN2kMsg& msg);  // Configuration Information
    DecodedMessage decodePGN127501(const tN2kMsg& msg);  // Binary Switch Bank Status

private:
    // Custom decoder function pointer type
    using CustomDecoderFunction = std::function<DecodedMessage(DBCDecoder*, const tN2kMsg&)>;
    
    // Custom decoder entry structure
    struct CustomDecoderEntry {
        unsigned long pgn;
        QString name;
        CustomDecoderFunction decoder;
    };
    
    QMap<unsigned long, DBCMessage> m_messages;
    QMap<unsigned long, CustomDecoderEntry> m_customDecoders;
    
    // Initialize custom decoder lookup table
    void initializeCustomDecoders();
};

#endif // DBCDECODER_H
