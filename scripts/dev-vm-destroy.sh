#!/usr/bin/env bash
# Script to stop and clean up the qbuf development VM

set -euo pipefail

# Color output
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
RED='\033[0;31m'
NC='\033[0m'

info() {
    echo -e "${GREEN}[INFO]${NC} $*"
}

warn() {
    echo -e "${YELLOW}[WARN]${NC} $*"
}

error() {
    echo -e "${RED}[ERROR]${NC} $*" >&2
}

info "Looking for running QEMU VM processes..."

# Find QEMU processes related to our VM
QEMU_PIDS=$(pgrep -f "qemu-system.*dev-vm" || true)

if [ -z "$QEMU_PIDS" ]; then
    info "No running qbuf VM found."
    exit 0
fi

info "Found VM process(es): $QEMU_PIDS"
info "Sending SIGTERM to gracefully shut down..."

# Send SIGTERM first for graceful shutdown
for PID in $QEMU_PIDS; do
    kill -TERM "$PID" 2>/dev/null || warn "Failed to send SIGTERM to PID $PID"
done

# Wait up to 10 seconds for graceful shutdown
TIMEOUT=10
ELAPSED=0
while [ $ELAPSED -lt $TIMEOUT ]; do
    REMAINING=$(pgrep -f "qemu-system.*dev-vm" || true)
    if [ -z "$REMAINING" ]; then
        info "VM shut down successfully."
        exit 0
    fi
    sleep 1
    ELAPSED=$((ELAPSED + 1))
done

# Force kill if still running
warn "VM did not shut down gracefully. Forcing termination..."
for PID in $QEMU_PIDS; do
    kill -KILL "$PID" 2>/dev/null || true
done

sleep 1

# Check if any processes remain
REMAINING=$(pgrep -f "qemu-system.*dev-vm" || true)
if [ -z "$REMAINING" ]; then
    info "VM terminated."
else
    error "Failed to terminate VM. Remaining processes: $REMAINING"
    error "You may need to manually kill these processes: sudo kill -9 $REMAINING"
    exit 1
fi
