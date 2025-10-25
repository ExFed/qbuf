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
             (guix gexp)
             (gnu services guix))

(define (platform-random x)
  (random x (random-state-from-platform)))

(define (random-salt)
  (let* ([max-salt (string->number "zzzzzzzzzzzzzzzz" 36)]
         [salt (platform-random max-salt)])
    (string-append "$6$" (number->string salt 36) "$")))

(operating-system
  (host-name "qbuf-dev")
  (timezone "UTC")
  (locale "en_US.utf8")

  ;; Use a minimal bootloader configuration for VM
  (bootloader (bootloader-configuration
                (bootloader grub-bootloader)
                (targets '("/dev/vda"))))

  ;; Root and shared workspace file systems
  ;; The qbuf_share 9p mount is automatically configured to mount the host
  ;; project directory at /mnt/workspace when the VM boots
  (file-systems (cons* (file-system
                         (device (file-system-label "my-root"))
                         (mount-point "/")
                         (type "ext4"))
                       (file-system
                         (device "qbuf_share")
                         (mount-point "/mnt/workspace")
                         (type "9p")
                         (options "trans=virtio,cache=loose")
                         (create-mount-point? #t)
                         (check? #f))
                       %base-file-systems))

  ;; Define a developer user with sudo access
  (users (cons (user-account
                 (name "dev")
                 (comment "Developer")
                 (group "users")
                 (supplementary-groups '("wheel" "audio" "video"))
                 (home-directory "/home/dev")
                 (shell (file-append bash "/bin/bash"))
                 (password (crypt "dev" (random-salt))))
               %base-user-accounts))

  ;; Allow wheel group to use sudo without password for convenience
  (sudoers-file (plain-file "sudoers" "\
root ALL=(ALL) ALL
%wheel ALL=(ALL) NOPASSWD:ALL\n"))

  ;; System packages available to all users
  (packages %base-packages)

  ;; Enable networking and SSH for remote access
  (services (cons* (service openssh-service-type
                           (openssh-configuration
                            (port-number 22)
                            (permit-root-login #t)
                            (password-authentication? #t)))
                   (simple-service 'dhcp
                                   dhcpcd-service-type
                                   #t)
                   %base-services)))
