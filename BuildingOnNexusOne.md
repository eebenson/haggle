**NOTE:** These instructions refer to Haggle v0.3 or older. Please follow the general instructions under QuickStartAndroid for building Haggle v0.4 or newer.

## Prerequisites ##

You need a rooted Nexus One phone. Google for "Nexus One root" for instructions. We recommend using superboot which does not require a re-flash of your phone.

Please see [QuickStartAndroid](QuickStartAndroid.md) for general instructions on how to build Haggle on Android.

## Steps-by-step ##

  1. Download and compile the Eclair (android 2.1) or Froyo (android 2.2) source code.
  1. Put the haggle source code in external/haggle of the android source tree (or make a symlink).
  1. `source ./build/envsetup.sh` (setup build environment)
  1. `lunch generic_passion-eng` (select Nexus One as target device)
  1. If not already built, build the Android source: `make` (wait for a long time)
  1. `cd external/haggle; mm`
  1. Install compiled binaries with the `android/install_haggle.sh` script in the android directory in the Haggle source tree. You need to run it in the shell in which you executed step 3.