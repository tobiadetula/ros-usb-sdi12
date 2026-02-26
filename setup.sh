#!/usr/bin/env bash

# Exit immediately if a command fails
set -e

# Function to print section headers
    log() {
        echo
        echo "=================================================="
        echo "[INFO] $1"
        echo "=================================================="
    }


# Function to check last command
check_status() {
    if [ $? -ne 0 ]; then
        echo "[ERROR] $1 failed. Exiting..."
        exit 1
    fi
}

    error_exit() {
        echo "[ERROR] $1 failed. Exiting."
        exit 1
    }


setup_libserial() {
    log "Installing LibSerial dependencies"
    sudo apt update
    sudo apt install libserial-dev
    log "LibSerial installed successfully"
    log "Adding user to dialout group for serial port access"
    sudo usermod -a -G dialout $USER
    }

