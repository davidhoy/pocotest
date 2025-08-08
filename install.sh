# Installation script for NMEA2000 Network Analyzer
#!/bin/bash

echo "Installing NMEA2000 Network Analyzer..."

# Create application directory
sudo mkdir -p /opt/nmea2000-analyzer

# Copy executable and launcher
sudo cp pocotest /opt/nmea2000-analyzer/
sudo cp nmea2000-analyzer.sh /opt/nmea2000-analyzer/
sudo chmod +x /opt/nmea2000-analyzer/nmea2000-analyzer.sh

# Copy desktop file
sudo cp resources/nmea2000-analyzer.desktop /usr/share/applications/

# Create icon directory and copy icon
sudo mkdir -p /usr/share/pixmaps
sudo cp resources/app_icon.png /usr/share/pixmaps/nmea2000-analyzer.png

# Update desktop database
sudo update-desktop-database

echo "Installation complete! You can now find 'NMEA2000 Network Analyzer' in your applications menu."
echo "Or run directly with: /opt/nmea2000-analyzer/pocotest"
