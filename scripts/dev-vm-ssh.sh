#!/usr/bin/env bash
# Script to SSH into the running qbuf development VM

set -euo pipefail

SSH_PORT=10022
SSH_USER=dev
SSH_HOST=localhost

# Color output
GREEN='\033[0;32m'
RED='\033[0;31m'
NC='\033[0m'

info() {
    echo -e "${GREEN}[INFO]${NC} $*"
}

error() {
    echo -e "${RED}[ERROR]${NC} $*" >&2
}

# Check if SSH is available
if ! command -v ssh &> /dev/null; then
    error "SSH client not found. Please install OpenSSH."
    exit 1
fi

# Check if VM is running by attempting to connect to the SSH port
# Try using netcat if available, otherwise use a timeout-based approach
if command -v nc &> /dev/null; then
    if ! nc -z "$SSH_HOST" "$SSH_PORT" 2>/dev/null; then
        error "Cannot connect to VM at $SSH_HOST:$SSH_PORT"
        error "Is the VM running? Start it with: ./run-qbuf-vm.sh"
        exit 1
    fi
else
    # Fallback: try SSH with short timeout
    if ! timeout 2 bash -c "echo > /dev/tcp/$SSH_HOST/$SSH_PORT" 2>/dev/null; then
        error "Cannot connect to VM at $SSH_HOST:$SSH_PORT"
        error "Is the VM running? Start it with: ./run-qbuf-vm.sh"
        exit 1
    fi
fi

info "Connecting to qbuf development VM..."
info "Default password: dev"
info ""

# SSH with options to reduce host key checking warnings for local VMs
exec ssh -p "$SSH_PORT" \
    -o "UserKnownHostsFile=/dev/null" \
    -o "StrictHostKeyChecking=no" \
    -o "LogLevel=ERROR" \
    "$SSH_USER@$SSH_HOST" "$@"
