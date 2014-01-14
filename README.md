Break
=====
A compiler that breaks your code, or sort of!


Dependencies
------------

Break uses LLVM build system and libraries. It works on versions 3.3 (preferable) and 3.1.


Making Break
------------

~~~ sh
$ ./configure --with-llvmsrc=/path/to/LLVM/source/root/ --with-llvmobj=/path/to/LLVM/object/root/
$ make ENABLE_OPTIMIZED=0
~~~

 
