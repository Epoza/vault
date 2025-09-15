#!/bin/bash
# Installer script

set -e

# If running as root, no need for sudo
if [ "$(id -u)" -eq 0 ]; then
    SUDO=""
else
    SUDO="sudo"
fi

# Check dependencies
echo "Checking dependencies..."
for dep in g++ cmake gocryptfs; do
    if ! command -v $dep &>/dev/null; then
        echo "Error: $dep is not installed. Please install it first."
        exit 1
    fi
done

# Build the project
echo "Building the project..."
mkdir -p build
cd build
cmake ..
make

# Install the binary system-wide
echo "Installing vault..."
$SUDO cp vault /usr/local/bin/
$SUDO chmod 755 /usr/local/bin/vault

# Setup encrypted vault folder
ENCRYPTED_DIR="$HOME/.vault.crypt"
MOUNT_DIR="$HOME/vault-plain"

mkdir -p "$ENCRYPTED_DIR" "$MOUNT_DIR"

if [ ! -f "$ENCRYPTED_DIR/gocryptfs.conf" ]; then
    echo "Initializing encrypted vault..."
    gocryptfs -init "$ENCRYPTED_DIR"
    gocryptfs "$ENCRYPTED_DIR" "$MOUNT_DIR"
fi

echo "Installation complete!"
echo "Run 'vault' to start managing your passwords."