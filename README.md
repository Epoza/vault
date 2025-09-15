# Vault - Password Manager :lock:
Vault is a lightweight, simple-to-use command-line password manager for Ubuntu/Debian-based systems.

## Features
- Add new entries (service, username, password) :pencil2:
- View saved entries :page_facing_up:
- Edit existing entries :pencil:
- Remove entries :scissors:
- Change the master password :key:

## Dependencies
Vault requires the following packages:
- g++
- cmake
- gocryptfs

## Installation

1. Clone or download the project files.
2. Navigate into the project directory.
3. Make the installation script executable.
```bash
chmod +x install.sh
```
4. Run the install script
```bash
./install.sh
```
## Usage

Once installed, start Vault by running:
```bash
vault
```
You will then be able to select from the menu to add, view, edit, remove entries, or change the master password.

## Running with Docker

If you have Docker installed, you can test Vault without installing dependencies on your system:
1. Navigate to the project directory
2. Build the Docker image:
```bash
docker build -t vault-test .
```
3. Run the container:
```bash
docker run --privileged -it vault-test
```
4. Run the install script
```bash
./install.sh
```
## Notes
- Vault stores your entries securely inside an encrypted directory managed by gocryptfs.
- Works out of the box for Ubuntu/Debian-based distributions, but install.sh can be modified to work on other linux distributions.
