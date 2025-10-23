;; Guix System VM configuration for qbuf development
;; This provides a reproducible development environment with all necessary tools

(use-modules (gnu)
             (gnu system)
             (gnu services)
             (gnu services ssh)
             (gnu services networking)
             (gnu packages)
             (gnu packages bash)
             (gnu packages gcc)
             (gnu packages cmake)
             (gnu packages version-control)
             (gnu packages text-editors)
             (gnu packages vim)
             (gnu packages tmux)
             (gnu packages linux)
             (gnu packages admin)
             (gnu packages compression)
             (gnu packages curl)
             (gnu packages less)
             (gnu packages ncurses)
             (gnu packages man)
             (gnu packages ssh)
             (gnu packages base)
             (guix gexp))

(operating-system
  (host-name "qbuf-dev")
  (timezone "UTC")
  (locale "en_US.utf8")

  ;; Use a minimal bootloader configuration for VM
  (bootloader (bootloader-configuration
                (bootloader grub-bootloader)
                (targets '("/dev/vda"))))

  ;; Single root file system for VM
  (file-systems (cons (file-system
                        (device (file-system-label "my-root"))
                        (mount-point "/")
                        (type "ext4"))
                      %base-file-systems))

  ;; Define a developer user with sudo access
  (users (cons (user-account
                 (name "dev")
                 (comment "Developer")
                 (group "users")
                 (supplementary-groups '("wheel" "audio" "video"))
                 (home-directory "/home/dev")
                 (shell (file-append bash "/bin/bash")))
               %base-user-accounts))

  ;; Allow wheel group to use sudo without password for convenience
  (sudoers-file (plain-file "sudoers" "\
root ALL=(ALL) ALL
%wheel ALL=(ALL) NOPASSWD:ALL\n"))

  ;; System packages available to all users
  (packages (append (list
                     ;; Development tools matching manifest.scm
                     bash
                     gcc-toolchain
                     clang
                     cmake
                     git
                     ;; Editors
                     vim
                     neovim
                     ;; Terminal multiplexer
                     tmux
                     ;; Utilities
                     coreutils
                     findutils
                     grep
                     sed
                     gawk
                     diffutils
                     which
                     tree
                     ripgrep
                     curl
                     wget
                     tar
                     gzip
                     less
                     man-db
                     ncurses
                     procps
                     util-linux
                     inetutils
                     openssh
                     nss-certs)
                    %base-packages))

  ;; Enable SSH for remote access
  (services (cons* (service openssh-service-type
                           (openssh-configuration
                            (port-number 22)
                            (permit-root-login #t)
                            (password-authentication? #t)
                            (authorized-keys
                             `(("dev" ,(local-file "~/.ssh/id_rsa.pub"
                                                  #:optional? #t))))))
                   %base-services)))
