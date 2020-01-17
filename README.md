
# Overview
This is a solution to the Stanford CS107 Programming Paradignms 
assignments 4 and 6 that are still available at Stanford Engineering Everywhere. 
https://see.stanford.edu/Course/CS107

The class was taught in 2008 and the march of progress has broken several aspects of the assignment starter files, as provided.

I was developing this solution on ubuntu 18.04.3 64-bit

# Issues and fixes
## 32-bit vs 64-bit architecture
The starter files include compiled libraries (.o files inside .a archive) that were compiled for a 32-bit environment.  Most of us will be on 64-bit computers today.  This is actually 2 separate problems.  The first: to use the 32-bit libraries provided, the code you write needs to be compiled in a 32-bit format.  This means you need all the supporting libraries for gcc to be able to build 32-bit.  The second: once you have compiled your 32-bit program, you likely need to do some setup to be able to run a compiled 32-bit binary executable, ie. the example files that show you what your program should do.  

Solution: 
I took the approach of installing the necessary 32-bit (i-386) runtime support and gcc libraries, as well as the i-386 version of the curl library (below.)  I could have simply proceeded with all my own libraries or substitues for those provided, compiling to 64-bit, but I decided to use at least the hashset and vector libraries provided. 

To install the gcc libraries, here are the commands I used: 
$ sudo apt-get install multiarch-support

runtime support for 32-bit executables: 
$ sudo dpkg --add-architecture i386

gcc libraries for building 32-bit apps (because )

## http vs https
The libraries provided that handle the network traffic  (urlconnection.h / urlconneciton.o) do not support https, only http.  It is now difficult to find an online news outlet serving articles in plain old http. 

Solution: Curl / Libcurl to the rescue! 
libcurl had everything I needed to get this assignment done and so much more! 
Documentation: https://curl.haxx.se/libcurl/c/libcurl.html

Installing libcurl 32-bit package on ubuntu: 
$ sudo apt install libcurl4-openssl-dev:i386

finally, you need to add  -lcurl to the LDFLAGS in the Makefile to tell the linker to look at the libcurl library. 

## no threading library
The lectures assume you will use a course-specific multi-threading library that does not appear to be available for download. 

Solution: use POSIX threading libraries
I used the POSIX packages by including <pthread.h> and <semaphore.h>, then adding -lpthread after LDFLAGS in the Makefile.  Some of the syntax is different from what is presented in the lecture videos, but I found it similar enough to use fairly painlessly. 

------------------------
