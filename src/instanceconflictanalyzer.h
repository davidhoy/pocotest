#ifndef INSTANCECONFLICTANALYZER_H
#define INSTANCECONFLICTANALYZER_H

#include <QObject>
#include <QMap>
#include <QSet>
#include <QString>
#include <QList>
#include <QDateTime>
#include <N2kMsg.h>

// Forward declaration
class QTableWidget;

struct PGNInstanceData {
    unsigned long pgn;
    uint8_t instance;
    uint8_t sourceAddress;
    QDateTime lastSeen;
    
    PGNInstanceData() : pgn(0), instance(255), sourceAddress(255) {}
    PGNInstanceData(unsigned long p, uint8_t i, uint8_t s) 
        : pgn(p), instance(i), sourceAddress(s), lastSeen(QDateTime::currentDateTime()) {}
};

struct InstanceConflict {
    unsigned long pgn;
    uint8_t instance;
    QSet<uint8_t> conflictingSources;
    QDateTime firstDetected;
    
    InstanceConflict() : pgn(0), instance(255) {}
    InstanceConflict(unsigned long p, uint8_t i, const QSet<uint8_t>& sources)
        : pgn(p), instance(i), conflictingSources(sources), firstDetected(QDateTime::currentDateTime()) {}
};

class InstanceConflictAnalyzer : public QObject
{
    Q_OBJECT

public:
    explicit InstanceConflictAnalyzer(QObject *parent = nullptr);
    
    // Main interface methods
    void trackPGNMessage(const tN2kMsg& msg);
    void updateConflictAnalysis();
    void highlightConflictsInTable(QTableWidget* deviceTable);
    void analyzeAndShowConflicts();
    void clearHistory();
    
    // Query methods
    bool hasConflicts() const;
    int getConflictCount() const;
    QStringList getConflictSummary() const;
    bool hasConflictForSource(uint8_t sourceAddress) const;
    QString getConflictInfoForSource(uint8_t sourceAddress) const;
    QList<InstanceConflict> getConflictDetailsForSource(uint8_t sourceAddress) const;
    QSet<uint8_t> getUsedInstancesForPGN(unsigned long pgn, uint8_t excludeDeviceAddress = 255) const;
    
    // Static utility methods
    static bool isPGNWithInstance(unsigned long pgn);
    static uint8_t extractInstanceFromPGN(const tN2kMsg& msg);
    static QString getPGNName(unsigned long pgn);

private:
    // Internal data structures
    QMap<QString, PGNInstanceData> m_pgnInstances;  // Key: "PGN_Source"
    QSet<QString> m_instanceConflicts;              // Set of conflicting "PGN_Instance" keys
    QSet<uint8_t> m_conflictingSources;             // Source addresses with conflicts
    
    // Helper methods
    QString createPGNKey(unsigned long pgn, uint8_t source) const;
    QString createInstanceKey(unsigned long pgn, uint8_t instance) const;
    void detectConflicts();
    void showConflictDialog();
    
    // Static data for PGN classification
    static QSet<unsigned long> getInstancePGNSet();
};

#endif // INSTANCECONFLICTANALYZER_H
