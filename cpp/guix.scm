(use-modules (guix packages)
             (guix build-system cmake)
             (guix gexp)
             ((guix licenses) #:prefix license:)
             (gnu packages cmake)
             (gnu packages gcc))

(package
  (name "qbuf-cpp")
  (version "0.1.0")
  (source (local-file "." "qbuf-cpp-checkout" #:recursive? #t))
  (build-system cmake-build-system)
  (arguments `(#:tests? #t
               #:build-type "Debug"))
  (native-inputs (list gcc-11 cmake))
  (synopsis "Thread-safe buffers and queues in C++")
  (description
   "Research implementation of thread-safe buffers and queues using modern C++.")
  (home-page "https://github.com/ExFed/qbuf")
  (license license:unlicense))
