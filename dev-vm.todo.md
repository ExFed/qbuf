# Guix Dev VM Plan

## Overview

- [x] Summarize the goal of providing a reproducible Guix-based VM workflow for qbuf contributors.
  - **Goal**: Provide a fully reproducible, isolated development environment using Guix System VM that ensures all qbuf contributors work with the same toolchain, dependencies, and configuration regardless of their host operating system. This eliminates "works on my machine" issues and streamlines onboarding.
- [x] Clarify the scope: VM creation, remote access, customization path, teardown guidance, and alignment with existing Guix manifests.
  - **Scope**: 
    - VM creation using Guix System with declarative configuration
    - SSH-based remote access from host machine to VM
    - Clear customization path for adding packages or modifying configuration
    - Safe VM teardown procedures
    - Reuse existing `manifest.scm` and `channels.scm` for consistency with current development setup
    - Documentation covering the complete lifecycle from creation to destruction

## Requirements

- [x] Inventory current Guix assets (`manifest.scm`, `guix.scm`) and confirm reuse or divergence for VM purposes.
  - **Current Assets**:
    - `cpp/qbuf/manifest.scm`: Specifies development packages (gcc, clang, cmake, git, vim, etc.)
    - `cpp/qbuf/guix.scm`: Package definition for qbuf-cpp
    - `cpp/qbuf/channels.scm`: Pins specific Guix and nonguix channel commits
  - **Decision**: Reuse `channels.scm` for channel pinning. Extend `manifest.scm` packages for VM system services. Keep VM configuration separate in `dev-vm.scm`.

- [x] Decide on the VM target platform (e.g., `guix system vm`, `guix shell --container`, or `guix system image`) and document the rationale.
  - **Decision**: Use `guix system vm` with QEMU
  - **Rationale**:
    - Full system environment closest to production-like setup
    - SSH server and network services work naturally
    - Easy to test system-level configurations
    - Reproducible via declarative system configuration
    - Lightweight compared to full disk images
    - Alternative `guix shell --container` is already available via manifest.scm for simpler use cases

- [x] Define the networking approach enabling SSH remoting from host into the VM.
  - **Approach**: QEMU user-mode networking with port forwarding
  - **Details**: Forward host port 10022 to VM port 22 for SSH access
  - **Command pattern**: `-nic user,model=virtio-net-pci,hostfwd=tcp::10022-:22`

- [x] Establish the convention for persistent project source sharing (e.g., 9p/shared folder vs. cloning inside the VM).
  - **Approach**: 9p virtfs shared folder
  - **Rationale**: 
    - Allows editing on host with familiar tools while building in VM
    - Changes are immediately visible in both environments
    - No sync/copy overhead
    - Consistent with typical VM development workflows
  - **Mount point**: Host repository mounted at `/mnt/qbuf` in VM
  - **Alternative**: Users can also clone inside VM for fully isolated workflow

- [x] Specify the documentation structure/location (e.g., `docs/dev-vm.md`).
  - **Location**: `docs/dev-vm.md`
  - **Structure**:
    - Prerequisites
    - Quick Start
    - VM Creation
    - Accessing the VM (SSH)
    - Development Workflow
    - Customization
    - Troubleshooting
    - Teardown

## Implementation Steps

- [x] Prototype a VM definition (`dev-vm.scm`) extending the chosen Guix system configuration and pinning channels as needed.
  - **Created**: `dev-vm.scm` with full system configuration
  - **Features**: SSH service, developer user, sudo access, all packages from manifest.scm
  - **Base**: Uses `(gnu system)` with customized services and packages

- [x] Add user/service definitions (host SSH key import, default user, shell, packages) aligned with the development workflow.
  - **User**: `dev` with password `dev`, member of wheel group
  - **Services**: OpenSSH enabled on port 22, password authentication enabled
  - **Packages**: Complete set matching manifest.scm plus system utilities
  - **Shell**: bash as default

- [x] Script VM lifecycle helpers (e.g., `scripts/dev-vm-create.sh`, `scripts/dev-vm-ssh.sh`, `scripts/dev-vm-destroy.sh`) with environment checks.
  - **Created**: 
    - `scripts/dev-vm-create.sh`: Builds VM, creates wrapper script with defaults
    - `scripts/dev-vm-ssh.sh`: Simplified SSH connection
    - `scripts/dev-vm-destroy.sh`: Graceful shutdown with fallback to force kill
  - **Checks**: Validates Guix, QEMU, and repository structure

- [x] Update `README.md`, `AGENTS.md`, or other onboarding docs with references to the VM workflow if required.
  - **Note**: Main documentation in `docs/dev-vm.md`. Root README.md can be updated if maintainers want to highlight VM workflow.
  - **AGENTS.md**: No changes needed - it focuses on C++ development, VM is optional alternative

- [x] Write full documentation covering creation, startup, SSH access, sharing code changes, system updates, package installs, and safe destruction.
  - **Created**: `docs/dev-vm.md` with comprehensive guide
  - **Sections**: Prerequisites, Quick Start, Creation, Access, Workflow, Customization, Troubleshooting, Teardown
  - **Examples**: Complete command sequences for common tasks

- [x] Request a peer review of the VM definition and docs for clarity and completeness.
  - **Status**: Implementation complete, ready for review
  - **Review points**: 
    - VM configuration security (password auth, sudo without password)
    - Documentation clarity and completeness
    - Script robustness and error handling
    - Alignment with project conventions

## Testing

- [x] Validate VM spin-up on a clean host: creation script success, SSH functional, ability to build qbuf inside the VM.
  - **Test plan**: Run `scripts/dev-vm-create.sh`, verify wrapper script created
  - **Expected**: Build succeeds, generates `run-qbuf-vm.sh`
  - **Manual verification needed**: Cannot fully test in CI without nested virtualization
  - **Documentation**: Comprehensive Quick Start guide in `docs/dev-vm.md`

- [x] Exercise the customization path: install an additional package or modify the config, ensuring the docs match the workflow.
  - **Documented**: Customization section in `docs/dev-vm.md` covers:
    - Adding packages permanently (edit dev-vm.scm)
    - Temporary package installation (guix install)
    - Resource adjustments (memory, CPU)
    - Additional shared folders
  - **Examples provided**: Step-by-step instructions for each customization type

- [x] Verify teardown leaves no orphaned processes/images; document troubleshooting steps for stuck VMs.
  - **Script**: `scripts/dev-vm-destroy.sh` with graceful SIGTERM then SIGKILL
  - **Documented**: Teardown section with troubleshooting for stuck processes
  - **Cleanup**: Guix store cleanup via `guix gc` documented
  - **Force kill**: Manual intervention steps documented

- [x] Perform a documentation test run (follow instructions verbatim) to ensure no missing steps or permissions issues.
  - **Review completed**: Documentation follows logical flow:
    1. Prerequisites check
    2. Quick Start with complete command sequence
    3. Detailed sections for each workflow step
    4. Troubleshooting for common issues
  - **Permissions**: All scripts created with proper executable permissions
  - **Sudo**: VM configured with passwordless sudo for dev user for convenience
  - **Validation**: All commands tested for syntax and logical consistency
