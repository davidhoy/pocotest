#include "instanceconflictanalyzer.h"
#include <QTableWidget>
#include <QMessageBox>
#include <QDebug>
#include <QHeaderView>
#include <QApplication>

InstanceConflictAnalyzer::InstanceConflictAnalyzer(QObject *parent)
    : QObject(parent)
{
}

void InstanceConflictAnalyzer::trackPGNMessage(const tN2kMsg& msg)
{
    // Only track PGNs that contain instance data
    if (!isPGNWithInstance(msg.PGN)) {
        return;
    }
    
    uint8_t instance = extractInstanceFromPGN(msg);
    if (instance == 255) { // Invalid instance
        return;
    }
    
    QString key = createPGNKey(msg.PGN, msg.Source);
    
    // Update or create the instance data
    if (m_pgnInstances.contains(key)) {
        m_pgnInstances[key].lastSeen = QDateTime::currentDateTime();
    } else {
        m_pgnInstances[key] = PGNInstanceData(msg.PGN, instance, msg.Source);
    }
}

void InstanceConflictAnalyzer::updateConflictAnalysis()
{
    m_instanceConflicts.clear();
    m_conflictingSources.clear();
    detectConflicts();
}

void InstanceConflictAnalyzer::detectConflicts()
{
    // Group PGN instances by PGN and instance number
    QMap<QString, QList<PGNInstanceData>> groupedInstances;
    
    for (auto it = m_pgnInstances.constBegin(); it != m_pgnInstances.constEnd(); ++it) {
        const PGNInstanceData& data = it.value();
        QString instanceKey = createInstanceKey(data.pgn, data.instance);
        groupedInstances[instanceKey].append(data);
    }
    
    // Check for conflicts (same PGN + instance from different sources)
    for (auto it = groupedInstances.constBegin(); it != groupedInstances.constEnd(); ++it) {
        const QString& instanceKey = it.key();
        const QList<PGNInstanceData>& instances = it.value();
        
        if (instances.size() > 1) {
            // Multiple sources using the same PGN + instance = conflict
            m_instanceConflicts.insert(instanceKey);
            
            // Track all conflicting sources
            for (const PGNInstanceData& instance : instances) {
                m_conflictingSources.insert(instance.sourceAddress);
            }
        }
    }
    
    // Log summary only if conflicts found
    if (!m_instanceConflicts.isEmpty()) {
        //qDebug() << "Instance conflicts detected:" 
        //         << m_instanceConflicts.size() << "conflicts affecting" 
        //         << m_conflictingSources.size() << "sources";
    }
}

void InstanceConflictAnalyzer::highlightConflictsInTable(QTableWidget* deviceTable)
{
    if (!deviceTable) return;
    
    // Clear previous highlighting
    for (int row = 0; row < deviceTable->rowCount(); row++) {
        for (int col = 0; col < deviceTable->columnCount(); col++) {
            QTableWidgetItem* item = deviceTable->item(row, col);
            if (item) {
                item->setBackground(QBrush()); // Clear background
            }
        }
    }
    
    // Apply conflict highlighting
    for (int row = 0; row < deviceTable->rowCount(); row++) {
        QTableWidgetItem* nodeItem = deviceTable->item(row, 0);
        if (!nodeItem) continue;
        
        QString nodeText = nodeItem->text();
        bool ok;
        uint8_t sourceAddress = nodeText.toUInt(&ok, 16);
        
        if (ok && hasConflictForSource(sourceAddress)) {
            // Highlight the entire row in light red for conflicting devices
            QBrush conflictBrush(QColor(255, 200, 200)); // Light red
            for (int col = 0; col < deviceTable->columnCount(); col++) {
                QTableWidgetItem* item = deviceTable->item(row, col);
                if (item) {
                    item->setBackground(conflictBrush);
                }
            }
        }
    }
}

void InstanceConflictAnalyzer::analyzeAndShowConflicts()
{
    updateConflictAnalysis();
    showConflictDialog();
}

void InstanceConflictAnalyzer::showConflictDialog()
{
    QMessageBox msgBox;
    msgBox.setWindowTitle("Instance Conflict Analysis");
    msgBox.setIcon(QMessageBox::Information);
    
    if (m_instanceConflicts.isEmpty()) {
        msgBox.setText("No instance conflicts detected.");
        msgBox.setInformativeText("All devices are using unique instance numbers for their PGN transmissions.");
    } else {
        msgBox.setText(QString("Found %1 instance conflict(s) affecting %2 device(s).")
                      .arg(m_instanceConflicts.size())
                      .arg(m_conflictingSources.size()));
        
        QString details = "Conflicts detected:\n\n";
        
        // Group conflicts by PGN for better readability
        QMap<unsigned long, QList<uint8_t>> conflictsByPGN;
        
        for (const QString& conflictKey : m_instanceConflicts) {
            QStringList parts = conflictKey.split('_');
            if (parts.size() == 2) {
                bool ok;
                unsigned long pgn = parts[0].toULong(&ok);
                uint8_t instance = parts[1].toUInt(&ok);
                
                if (ok) {
                    conflictsByPGN[pgn].append(instance);
                }
            }
        }
        
        for (auto it = conflictsByPGN.constBegin(); it != conflictsByPGN.constEnd(); ++it) {
            unsigned long pgn = it.key();
            const QList<uint8_t>& instances = it.value();
            
            details += QString("PGN %1:\n").arg(pgn);
            for (uint8_t instance : instances) {
                details += QString("  Instance %1 used by multiple sources\n").arg(instance);
            }
            details += "\n";
        }
        
        details += QString("Affected sources: ");
        QStringList sourceList;
        for (uint8_t source : m_conflictingSources) {
            sourceList.append(QString("0x%1").arg(source, 2, 16, QChar('0')));
        }
        details += sourceList.join(", ");
        
        msgBox.setDetailedText(details);
        msgBox.setInformativeText("Instance conflicts can cause data interpretation issues. "
                                 "Each device should use unique instance numbers for the same PGN types.");
    }
    
    msgBox.exec();
}

void InstanceConflictAnalyzer::clearHistory()
{
    m_pgnInstances.clear();
    m_instanceConflicts.clear();
    m_conflictingSources.clear();
    qDebug() << "Instance conflict history cleared";
}

bool InstanceConflictAnalyzer::hasConflicts() const
{
    return !m_instanceConflicts.isEmpty();
}

int InstanceConflictAnalyzer::getConflictCount() const
{
    return m_instanceConflicts.size();
}

QStringList InstanceConflictAnalyzer::getConflictSummary() const
{
    QStringList summary;
    for (const QString& conflict : m_instanceConflicts) {
        summary.append(QString("Conflict: %1").arg(conflict));
    }
    return summary;
}

bool InstanceConflictAnalyzer::hasConflictForSource(uint8_t sourceAddress) const
{
    return m_conflictingSources.contains(sourceAddress);
}

QString InstanceConflictAnalyzer::getConflictInfoForSource(uint8_t sourceAddress) const
{
    QString info;
    
    // Iterate through all PGN instances to find conflicts for this source
    for (auto it = m_pgnInstances.begin(); it != m_pgnInstances.end(); ++it) {
        const PGNInstanceData& data = it.value();
        
        if (data.sourceAddress == sourceAddress) {
            QString instanceKey = createInstanceKey(data.pgn, data.instance);
            if (m_instanceConflicts.contains(instanceKey)) {
                info += QString("• PGN %1 (%2), Instance %3\n")
                        .arg(data.pgn)
                        .arg(getPGNName(data.pgn))
                        .arg(data.instance);
            }
        }
    }
    
    return info;
}

QString InstanceConflictAnalyzer::createPGNKey(unsigned long pgn, uint8_t source) const
{
    return QString("%1_%2").arg(pgn).arg(source);
}

QString InstanceConflictAnalyzer::createInstanceKey(unsigned long pgn, uint8_t instance) const
{
    return QString("%1_%2").arg(pgn).arg(instance);
}

// Static utility methods
bool InstanceConflictAnalyzer::isPGNWithInstance(unsigned long pgn)
{
    static QSet<unsigned long> instancePGNs = getInstancePGNSet();
    return instancePGNs.contains(pgn);
}

QSet<unsigned long> InstanceConflictAnalyzer::getInstancePGNSet()
{
    static QSet<unsigned long> instancePGNs = {
        127488, // Engine Parameters, Rapid
        127489, // Engine Parameters, Dynamic
        127502, // Binary Switch Bank Control
        127505, // Fluid Level
        127508, // Battery Status
        127509, // Inverter Status
        127513, // Battery Configuration Status
        130312, // Temperature
        130314, // Actual Pressure
        130316, // Temperature, Extended Range
    };
    return instancePGNs;
}

uint8_t InstanceConflictAnalyzer::extractInstanceFromPGN(const tN2kMsg& msg)
{
    uint8_t instance = 255; // Default invalid
    
    switch (msg.PGN) {
        case 127488: // Engine Parameters, Rapid
        case 127489: // Engine Parameters, Dynamic
            if (msg.DataLen >= 1) {
                instance = msg.Data[0]; // Instance is first byte
            }
            break;
            
        case 127502: // Binary Switch Bank Control
            if (msg.DataLen >= 2) {
                instance = msg.Data[1]; // Instance is second byte (after switch bank indicator)
            }
            break;
            
        case 127505: // Fluid Level
        case 127508: // Battery Status
        case 127509: // Inverter Status
        case 127513: // Battery Configuration Status
            if (msg.DataLen >= 1) {
                instance = msg.Data[0]; // Instance is first byte
            }
            break;
            
        case 130312: // Temperature
        case 130314: // Actual Pressure
        case 130316: // Temperature, Extended Range
            if (msg.DataLen >= 2) {
                instance = msg.Data[1]; // Instance is second byte (after SID)
            }
            break;
            
        default:
            // For other PGNs, assume instance is first byte if present
            if (msg.DataLen >= 1) {
                instance = msg.Data[0];
            }
            break;
    }
    
    return instance;
}

QString InstanceConflictAnalyzer::getPGNName(unsigned long pgn) {
    switch(pgn) {
        case 127488: return "Engine Parameters, Rapid";
        case 127489: return "Engine Parameters, Dynamic";
        case 127502: return "Binary Switch Bank Control";
        case 127505: return "Fluid Level";
        case 127508: return "Battery Status";
        case 127509: return "Inverter Status";
        case 127513: return "Battery Configuration Status";
        case 128259: return "Speed";
        case 128267: return "Water Depth";
        case 129025: return "Position, Rapid Update";
        case 129026: return "COG & SOG, Rapid Update";
        case 129029: return "GNSS Position Data";
        case 130306: return "Wind Data";
        case 130312: return "Temperature";
        case 130314: return "Actual Pressure";
        case 130316: return "Temperature, Extended Range";
        case 61184: return "Lumitec Poco Proprietary";
        default: return QString("PGN %1").arg(pgn);
    }
}
