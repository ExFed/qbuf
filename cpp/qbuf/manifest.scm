;; This manifest can be used with `guix shell` to set up a development environment
(specifications->manifest
 '("gcc-toolchain"
   "clang"
   "cmake"
   "make"))
