# Prerequisites #

In order to compile the Haggle source code on Mac OS X, you need a standard development environment installed, e.g., XCode. This includes GCC, standard development headers and tools. In addition you need to install [Mercurial](http://mercurial.selenic.com/wiki/Mercurial) (in case you get the code from our source code repository). Haggle also depends on the following libraries (including development headers):

  * libxml2
  * sqlite3
  * openssl (libcrypto)

You have to option of compiling Haggle via XCode or the command line. The command line gives the most flexibility, but XCode can be easier for many users.

# Compilation #

## Checking out and compiling from the repository ##

  1. `hg clone https://haggle.googlecode.com/hg/ haggle`
  1. `cd haggle`
  1. `./autogen.sh`
  1. `./configure`
  1. `make`

## Compiling from official release source code package ##

  1. `tar zxf haggle-<version>.tar.gz`
  1. `cd haggle-<version>`
  1. `./configure`
  1. `make`

## Compiling using XCode ##

In the macosx directory of the source code tree, you will find XCode project files that can be used to compile and run Haggle in XCode.

# Running Haggle #

Simply type `./bin/haggle` to run Haggle, assuming you are in the source code directory as instructed above.