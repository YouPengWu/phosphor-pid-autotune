#!/bin/bash
# Linux Build Script for FOPDT Tuner
# Requirements: pip install pyinstaller matplotlib Pillow

# Ensure we are in the tool directory
cd "$(dirname "$0")"

echo "Starting Linux build process..."

# Define the entry point
ENTRY_POINT="main.py"
APP_NAME="fopdt_tuner"

# Run PyInstaller
# --onefile: Create a single executable
# --windowed: Do not show console window (useful for GUI)
# --name: Specific name for the binary
# --add-data: Include the 'app' package as a folder if needed (pyinstaller usually handles imports)
# --clean: Clean cache before build

./venv/bin/pyinstaller --onefile --windowed --name "$APP_NAME" --clean "$ENTRY_POINT" --paths ..

if [ $? -eq 0 ]; then
    echo "------------------------------------------------"
    echo "Linux Build Successful!"
    echo "Binary location: tool/dist/$APP_NAME"
    echo "------------------------------------------------"
else
    echo "Build failed!"
    exit 1
fi
