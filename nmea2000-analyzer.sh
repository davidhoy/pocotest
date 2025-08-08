#!/bin/bash

# NMEA 2000 Analyzer Launcher Script
# This script sets environment variables for better desktop integration

export QT_QPA_PLATFORMTHEME="gtk3"
export QT_SCALE_FACTOR_ROUNDING_POLICY="RoundPreferFloor"

# Run the application from the installation directory
cd /opt/nmea2000-analyzer
exec ./pocotest "$@"
