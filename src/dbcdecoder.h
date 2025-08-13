#ifndef DBCDECODER_H
#define DBCDECODER_H

#include <QObject>
#include <QString>
#include <QList>
#include <QMap>
#include <QVariant>
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
    QString formatSignalValue(const DecodedSignal& signal);
    
    // Status functions
    int getLoadedMessageCount() const;
    QStringList getAvailablePGNs() const;
    bool isInitialized() const;  // Status check for compatibility
    QString getDecoderInfo() const;  // Enhanced decoder status info

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

private:
    QMap<unsigned long, DBCMessage> m_messages;
};

#endif // DBCDECODER_H
