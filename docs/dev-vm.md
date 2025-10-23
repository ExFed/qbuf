# Guix Development VM Guide

This guide describes how to use the Guix System-based development VM for qbuf. The VM provides a fully reproducible, isolated development environment with all necessary tools pre-configured.

## Table of Contents

- [Overview](#overview)
- [Prerequisites](#prerequisites)
- [Quick Start](#quick-start)
- [VM Creation](#vm-creation)
- [Accessing the VM](#accessing-the-vm)
- [Development Workflow](#development-workflow)
- [Customization](#customization)
- [Troubleshooting](#troubleshooting)
- [Teardown](#teardown)

## Overview

The qbuf development VM is built using Guix System and QEMU. It provides:

- **Reproducibility**: All contributors use identical toolchains and configurations
- **Isolation**: Development happens in a clean environment without affecting your host
- **Consistency**: Matches the pinned channels and packages from `manifest.scm`
- **Convenience**: SSH access and shared folders for seamless host-VM interaction

## Prerequisites

Before creating the VM, ensure you have:

1. **Guix package manager** or **Guix System** installed
   - Install from: https://guix.gnu.org/download/
   - Or use your distribution's package manager if available

2. **QEMU** installed
   - On Guix: `guix install qemu`
   - On other systems: Use your package manager (e.g., `apt install qemu-system-x86`)

3. **Sufficient disk space**: At least 5GB free for the VM image and store

4. **SSH client**: Usually pre-installed on Linux/macOS, or use PuTTY on Windows

## Quick Start

The fastest way to get started:

```bash
# 1. Create the VM (first time only, ~10-15 minutes)
./scripts/dev-vm-create.sh

# 2. Start the VM
./run-qbuf-vm.sh

# 3. In another terminal, SSH into the VM
./scripts/dev-vm-ssh.sh

# 4. Inside the VM, mount the shared folder and build qbuf
sudo mkdir -p /mnt/qbuf
sudo mount -t 9p -o trans=virtio qbuf_share /mnt/qbuf
cd /mnt/qbuf/cpp/qbuf
cmake -S . -B build --fresh
cmake --build build
ctest --output-on-failure --test-dir build

# 5. When done, stop the VM
./scripts/dev-vm-destroy.sh
```

## VM Creation

The `dev-vm-create.sh` script handles VM creation:

```bash
./scripts/dev-vm-create.sh
```

This script:
1. Verifies Guix and QEMU are installed
2. Pulls the pinned Guix channels from `cpp/qbuf/channels.scm`
3. Builds the VM using `dev-vm.scm` configuration
4. Creates a wrapper script `run-qbuf-vm.sh` with sensible defaults

**Note**: The first build can take 10-30 minutes depending on your system and network speed. Guix will download and cache all necessary packages. Subsequent builds reuse the cache and are much faster.

### What Gets Built

The VM includes all packages from `cpp/qbuf/manifest.scm`:
- Development tools: gcc, clang, cmake, git
- Editors: vim, neovim
- Utilities: tmux, ripgrep, tree, curl, wget
- Shell environment: bash with standard Unix tools

### Configuration Details

The VM configuration (`dev-vm.scm`):
- **Hostname**: qbuf-dev
- **Default user**: `dev` (password: `dev`)
- **Sudo**: Enabled for `wheel` group (no password required)
- **SSH**: Enabled on port 22 (forwarded to host port 10022)
- **Resources**: 2GB RAM, 2 CPUs (configurable in `run-qbuf-vm.sh`)

## Accessing the VM

### Starting the VM

Run the generated wrapper script:

```bash
./run-qbuf-vm.sh
```

The VM will:
- Start in the background with QEMU
- Forward port 10022 on your host to port 22 in the VM
- Share your repository directory as a 9p virtfs mount
- Display connection information

### SSH Connection

Connect from another terminal:

```bash
./scripts/dev-vm-ssh.sh
```

Or manually:

```bash
ssh -p 10022 dev@localhost
# Password: dev
```

### First Login Steps

After logging in for the first time:

1. **Mount the shared folder**:
   ```bash
   sudo mkdir -p /mnt/qbuf
   sudo mount -t 9p -o trans=virtio qbuf_share /mnt/qbuf
   ```

2. **Verify the mount**:
   ```bash
   ls -la /mnt/qbuf
   # You should see the repository files
   ```

3. **Optional: Add mount to fstab for persistence** (requires VM rebuild):
   Edit `dev-vm.scm` and add to `file-systems`.

## Development Workflow

### Building qbuf Inside the VM

Once you've mounted the shared folder:

```bash
cd /mnt/qbuf/cpp/qbuf

# Configure (first time or after cleaning)
cmake -S . -B build --fresh

# Build
cmake --build build

# Run tests
ctest --output-on-failure --test-dir build

# Run benchmarks
./build/benchmark
```

### Editing Files

You have two options:

1. **Edit on host, build in VM** (recommended):
   - Use your favorite editor on the host machine
   - Changes are immediately visible in the VM via the shared folder
   - Build and test inside the VM

2. **Edit in VM**:
   - Use `vim` or `neovim` inside the VM
   - All changes are still reflected on the host via the shared folder

### Using Guix Shell in VM

For development with the exact manifest environment:

```bash
cd /mnt/qbuf/cpp/qbuf
guix shell -m manifest.scm
# Now you're in a pure environment matching the manifest
```

### Updating Packages in VM

To update or install additional packages:

```bash
# Install a package temporarily (lost on reboot)
guix install <package-name>

# Or use guix shell
guix shell <package-name> -- <command>
```

For permanent changes, see [Customization](#customization).

## Customization

### Adding Packages Permanently

To add packages to the VM system:

1. Edit `dev-vm.scm`
2. Add package to the `packages` list:
   ```scheme
   (packages (append (list
                      ...
                      your-new-package
                      ...)
                     %base-packages))
   ```

3. Rebuild the VM:
   ```bash
   ./scripts/dev-vm-create.sh
   ```

### Changing VM Resources

Edit the generated `run-qbuf-vm.sh` script:

```bash
# Adjust these variables:
SSH_PORT=10022    # Host SSH port
MEMORY=4G         # Increase to 4GB RAM
CPUS=4            # Use 4 CPU cores
```

### Adding More Shared Folders

Edit `run-qbuf-vm.sh` and add additional `-virtfs` options:

```bash
-virtfs local,path="/host/path",mount_tag=my_tag,security_model=mapped-xattr,id=my_id
```

Then mount inside the VM:

```bash
sudo mkdir -p /mnt/my_folder
sudo mount -t 9p -o trans=virtio my_tag /mnt/my_folder
```

### Customizing User Configuration

The default user `dev` has minimal configuration. To customize:

1. In the VM, create dotfiles in `/home/dev/`
2. Or, modify `dev-vm.scm` to include pre-configured dotfiles

### Using Different Guix Channels

To use different channel versions:

1. Edit `cpp/qbuf/channels.scm` to pin different commits
2. Rebuild the VM: `./scripts/dev-vm-create.sh`

## Troubleshooting

### VM Won't Start

**Problem**: `run-qbuf-vm.sh` fails or hangs

**Solutions**:
- Check if port 10022 is already in use: `lsof -i :10022`
- Verify QEMU is installed: `which qemu-system-x86_64`
- Check if another VM instance is running: `pgrep qemu`
- Review QEMU output for error messages

### Cannot SSH Into VM

**Problem**: `scripts/dev-vm-ssh.sh` fails to connect

**Solutions**:
- Verify VM is running: `pgrep qemu`
- Check port forwarding: `nc -zv localhost 10022`
- Wait 30-60 seconds after VM start for SSH to be ready
- Try manual SSH: `ssh -p 10022 -v dev@localhost` to see detailed errors

### Shared Folder Not Working

**Problem**: Cannot see repository files in `/mnt/qbuf`

**Solutions**:
- Ensure you mounted the 9p filesystem:
  ```bash
  sudo mount -t 9p -o trans=virtio qbuf_share /mnt/qbuf
  ```
- Check if the mount tag matches: `dmesg | grep 9p`
- Verify the share path in `run-qbuf-vm.sh`

### Build Errors in VM

**Problem**: CMake or compilation fails

**Solutions**:
- Ensure you're building from the shared mount: `cd /mnt/qbuf/cpp/qbuf`
- Check if packages are available: `which gcc`, `which cmake`
- Try in a clean Guix shell: `guix shell -m manifest.scm`
- Verify file permissions on shared folder

### VM Performance Issues

**Problem**: VM is slow or unresponsive

**Solutions**:
- Increase memory in `run-qbuf-vm.sh`: `MEMORY=4G`
- Increase CPU cores: `CPUS=4`
- Check host system resources: `top` or `htop`
- Consider using KVM acceleration if available (QEMU will auto-detect)

### Stuck VM Processes

**Problem**: VM won't stop with `dev-vm-destroy.sh`

**Solutions**:
- Find the process: `pgrep -a qemu`
- Force kill: `pkill -9 -f "qemu-system.*dev-vm"`
- Or kill by PID: `kill -9 <pid>`

### Rebuilding After Changes

**Problem**: Changes to `dev-vm.scm` not reflected

**Solution**:
- Always run `./scripts/dev-vm-create.sh` after editing `dev-vm.scm`
- Stop the old VM first: `./scripts/dev-vm-destroy.sh`
- Guix caches builds, so rebuilds are usually fast

## Teardown

### Stopping the VM

To stop a running VM:

```bash
./scripts/dev-vm-destroy.sh
```

This script:
1. Finds running QEMU processes for the qbuf VM
2. Sends SIGTERM for graceful shutdown
3. Waits up to 10 seconds
4. Forces termination if needed (SIGKILL)

### Removing VM Files

The VM image is stored in the Guix store and managed automatically. To free up space:

```bash
# Remove old VM generations
guix gc

# Force garbage collection (removes all unreferenced store items)
guix gc --delete-generations
```

### Complete Cleanup

To completely remove all VM artifacts:

```bash
# 1. Stop the VM
./scripts/dev-vm-destroy.sh

# 2. Remove the wrapper script
rm -f run-qbuf-vm.sh

# 3. Garbage collect Guix store
guix gc

# 4. Remove any temporary files
rm -f /tmp/guix-* 2>/dev/null || true
```

The VM can be recreated anytime by running `./scripts/dev-vm-create.sh` again.

## Advanced Topics

### Using Time Machine with Specific Channels

To build the VM with exact channel versions:

```bash
guix time-machine -C cpp/qbuf/channels.scm -- system vm dev-vm.scm
```

### Headless VM Operation

The VM already runs headless (no GUI). All interaction happens via SSH. This is configured with the `--no-graphic` flag in the build script.

### Integration with CI/CD

The same `dev-vm.scm` configuration can be used to build container images or run tests in CI:

```bash
# Build a container image instead of VM
guix system container dev-vm.scm
```

### Debugging VM Boot Issues

To see detailed boot output:

1. Remove `--no-graphic` from `dev-vm-create.sh`
2. Rebuild the VM
3. Run with graphical console to see boot messages

## Additional Resources

- [Guix System Documentation](https://guix.gnu.org/manual/en/html_node/System-Configuration.html)
- [Guix Cookbook: QEMU](https://guix.gnu.org/cookbook/en/html_node/Guix-System-in-a-VM.html)
- [QEMU Documentation](https://www.qemu.org/docs/master/)
- qbuf project: `README.md`, `cpp/qbuf/README.md`, `cpp/qbuf/AGENTS.md`

## Getting Help

If you encounter issues not covered here:

1. Check QEMU logs: `dmesg` or VM console output
2. Review Guix build logs: `guix build --log-file dev-vm.scm`
3. Search existing issues: https://github.com/ExFed/qbuf/issues
4. Open a new issue with:
   - Steps to reproduce
   - Error messages
   - Guix version: `guix describe`
   - QEMU version: `qemu-system-x86_64 --version`
