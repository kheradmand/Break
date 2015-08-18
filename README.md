Break (Lockout)
=====
Break (later named Lockout) is a tool that increases the probability of deadlock occurrence in multithreaded programs, while preserving program semantics and requiring no perturbation to the runtime and test infrastructure. Lockout takes the source code of a program, and it produces a binary (similar to the native binary) that is more prone to deadlock at runtime. This way, more deadlocks can be detected, using the same test suite.


Dependencies
------------

Break uses LLVM build system and libraries. It works on versions 3.3 (preferable) and 3.1.


Making Break
------------

~~~ sh
$ ./configure --with-llvmsrc=/path/to/LLVM/source/root/ --with-llvmobj=/path/to/LLVM/object/root/
$ make ENABLE_OPTIMIZED=0
~~~

 
