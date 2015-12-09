# Prerequisites #

In order to compile the Haggle source code on Linux, you need a standard development environment installed. This includes GCC, standard development headers and tools. In addition you need to install Mercurial (in case you get the code from our source code repository), and also additional development packages for the libraries that Haggle depends on (OpenSSL, Sqlite, and LibXML2).

A typical set of packages for Ubuntu looks like follows (install with, e.g., apt):

  * build-essential (meta-package that includes libc6-dev, gcc, g++, make, etc.)
  * automake
  * autoconf
  * libtool
  * libsqlite3-dev
  * libxml2-dev
  * libbluetooth-dev
  * libdbus-1-3
  * libdbus-1-dev
  * libssl-dev
  * mercurial

Depending on your choice of Linux distribution this set of packages may vary, but it should be straightforward to figure out what packages the above one translate to for your distribution.

# Compilation #

## Checking out and compiling from the repository ##

  1. `hg clone https://code.google.com/p/haggle/`
  1. `cd haggle`
  1. `./autogen.sh`
  1. `./configure`
  1. `make`

## Compiling from official release source code package ##

  1. `tar zxf haggle-<version>.tar.gz`
  1. `cd haggle-<version>`
  1. `./configure`
  1. `make`

# Running Haggle #

Simply type `./bin/haggle` to run Haggle, assuming you are in the source code directory as instructed above.