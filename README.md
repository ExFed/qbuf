# QBuf

This project is intended to research thread-safe buffers and queues for different
programming languages.

## Project Layout

Implementation are each independent subprojects, grouped by programming
language. For instance, a Java implementation of a Michael & Scott queue named
`msqueue` would go in the `java/msqueue` directory. Each subproject should
provide a way to build the implementation.

## Development Environment

For a reproducible development environment using Guix System VM, see [docs/dev-vm.md](docs/dev-vm.md). The VM provides an isolated, consistent environment with all necessary development tools pre-configured.
