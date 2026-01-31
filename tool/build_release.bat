@echo off
:: Windows Build Script for FOPDT Tuner
:: Requirements: pip install pyinstaller matplotlib Pillow

echo Starting Windows build process...

:: Set APP_NAME
set APP_NAME=fopdt_tuner

:: Run PyInstaller
:: --onefile: Create a single executable
:: --windowed: Do not show console window
:: --name: Specific name for the binary
:: --clean: Clean cache before build

pyinstaller --onefile --windowed --name "%APP_NAME%" --clean main.py --paths ..

if %ERRORLEVEL% EQU 0 (
    echo ------------------------------------------------
    echo Windows Build Successful!
    echo Binary location: tool\dist\%APP_NAME%.exe
    echo ------------------------------------------------
) else (
    echo Build failed!
    pause
)
