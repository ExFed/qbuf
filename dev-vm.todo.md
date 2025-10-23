# Guix Dev VM Plan

## Overview

- [ ] Summarize the goal of providing a reproducible Guix-based VM workflow for qbuf contributors.
- [ ] Clarify the scope: VM creation, remote access, customization path, teardown guidance, and alignment with existing Guix manifests.

## Requirements

- [ ] Inventory current Guix assets (`manifest.scm`, `guix.scm`) and confirm reuse or divergence for VM purposes.
- [ ] Decide on the VM target platform (e.g., `guix system vm`, `guix shell --container`, or `guix system image`) and document the rationale.
- [ ] Define the networking approach enabling SSH remoting from host into the VM.
- [ ] Establish the convention for persistent project source sharing (e.g., 9p/shared folder vs. cloning inside the VM).
- [ ] Specify the documentation structure/location (e.g., `docs/dev-vm.md`).

## Implementation Steps

- [ ] Prototype a VM definition (`dev-vm.scm`) extending the chosen Guix system configuration and pinning channels as needed.
- [ ] Add user/service definitions (host SSH key import, default user, shell, packages) aligned with the development workflow.
- [ ] Script VM lifecycle helpers (e.g., `scripts/dev-vm-create.sh`, `scripts/dev-vm-ssh.sh`, `scripts/dev-vm-destroy.sh`) with environment checks.
- [ ] Update `README.md`, `AGENTS.md`, or other onboarding docs with references to the VM workflow if required.
- [ ] Write full documentation covering creation, startup, SSH access, sharing code changes, system updates, package installs, and safe destruction.
- [ ] Request a peer review of the VM definition and docs for clarity and completeness.

## Testing

- [ ] Validate VM spin-up on a clean host: creation script success, SSH functional, ability to build qbuf inside the VM.
- [ ] Exercise the customization path: install an additional package or modify the config, ensuring the docs match the workflow.
- [ ] Verify teardown leaves no orphaned processes/images; document troubleshooting steps for stuck VMs.
- [ ] Perform a documentation test run (follow instructions verbatim) to ensure no missing steps or permissions issues.
