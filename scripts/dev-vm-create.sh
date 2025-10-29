#!/usr/bin/env bash
# Script to create and launch the qbuf development VM

set -euo pipefail

# Color output
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
NC='\033[0m' # No Color

info() {
    echo -e "${GREEN}[INFO]${NC} $*"
}

warn() {
    echo -e "${YELLOW}[WARN]${NC} $*"
}

error() {
    echo -e "${RED}[ERROR]${NC} $*" >&2
}

# Check for Guix
if ! command -v guix &> /dev/null; then
    error "Guix is not installed. Please install Guix System or the Guix package manager."
    exit 1
fi

# Check for QEMU
if ! command -v qemu-system-x86_64 &> /dev/null; then
    error "QEMU is not installed. Please install QEMU or use 'guix install qemu'."
    exit 1
fi

# Verify we're in the qbuf repository
if [ ! -f "dev-vm.scm" ]; then
    error "dev-vm.scm not found. Please run this script from the qbuf repository root."
    exit 1
fi

info "Building qbuf development VM..."
info "This may take several minutes on first run..."

# Pull channels if channels file exists
if [ -f "cpp/channels.scm" ]; then
    info "Pulling Guix channels..."
    guix time-machine -C cpp/channels.scm -- describe || warn "Channel pull failed, using current Guix version"
fi

# Build the VM
info "Building VM system configuration..."
VM_SCRIPT=$(guix system vm dev-vm.scm --no-graphic 2>&1 | tee /dev/stderr | tail -1)

if [ ! -f "$VM_SCRIPT" ] || [ ! -x "$VM_SCRIPT" ]; then
    error "Failed to build VM. Expected executable script but got: $VM_SCRIPT"
    exit 1
fi

info "VM built successfully: $VM_SCRIPT"

# Create a wrapper script with better defaults
WRAPPER_SCRIPT="./run-dev-vm.sh"
cat > "$WRAPPER_SCRIPT" << EOF
#!/usr/bin/env bash
# Auto-generated wrapper for qbuf development VM

set -euo pipefail

# Port forwarding: Host 10022 -> VM 22 (SSH)
# Shared folder: Current directory -> VM /mnt/workspace
# Memory: 2GB
# CPUs: 2

SSH_PORT=10022
MEMORY=2G
CPUS=2
SHARE_PATH="\$(pwd)"

echo "Starting qbuf development VM..."
echo "  SSH: localhost:\$SSH_PORT (user: dev, password: dev)"
echo "  Shared folder: \$SHARE_PATH -> /mnt/workspace"
echo "  Memory: \$MEMORY, CPUs: \$CPUS"
echo ""
echo "To connect: ssh -p \$SSH_PORT dev@localhost"
echo "To stop: Press Ctrl+C or run 'scripts/dev-vm-destroy.sh'"
echo ""

exec "$VM_SCRIPT" \\
  -m "\$MEMORY" \\
  -smp "\$CPUS" \\
  -nic user,model=virtio-net-pci,hostfwd=tcp::\$SSH_PORT-:22 \\
  -virtfs local,path="\$SHARE_PATH",mount_tag=qbuf_share,security_model=mapped-xattr,id=qbuf_share
EOF

chmod +x "$WRAPPER_SCRIPT"

info "========================================"
info "VM setup complete!"
info "========================================"
info ""
info "To start the VM:"
info "  $WRAPPER_SCRIPT"
info ""
info "The VM will be accessible via SSH at:"
info "  ssh -p 10022 dev@localhost"
info "  (default password: 'dev')"
info ""
info "Your repository will be available in the VM at: /mnt/workspace"
info ""
info "See docs/dev-vm.md for more details."
