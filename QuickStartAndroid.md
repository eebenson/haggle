**NOTE:** Haggle installs and runs fine on non-rooted Android devices. However, Haggle can currently only make use of WiFi in managed (basestation) mode. To set WiFi ad-hoc mode, or make use of Bluetooth, you need to root your device (see below).

## Why do I need an Android device with root access to make the most of Haggle? ##

There are two main reasons why Haggle requires root access on a device:
  1. Access to low level Bluetooth APIs requires root privileges. We are currently investigating possibilities to use Java APIs that do not have this requirement.
  1. There is currently no way to set ad-hoc mode on non-rooted devices.

Even if our ambition is to keep this guide up to date, you can also look at android/README in the source code tree for more information about compiling and running Haggle on Android.

# Building Haggle and Apps #

Haggle for Android consists of both native (C++) code and managed Java code (similarly, Haggle applications rely on Java bindings with native components). Building Haggle is therefore a two-step process that involves first compiling native code into libraries and then building the Java application. Below follows step-by-step instructions for this process.

## Prerequisites ##

  * Basic compilation environment on build machine
  * Android NDK version [r5](https://code.google.com/p/haggle/source/detail?r=5) or greater
  * Android SDK (installed and in PATH)
  * Eclipse with Android development plugin or Ant
  * Java

## Step-by-Step ##

Steps (all paths relative top source directory):

  1. Run configure in top-level directory
    * Make sure "Android applications = yes" in case you want to build Android applications with Ant rather than Eclipse. For this to  succeed, both Ant and the Android SDK need to be in the PATH.
```
$ ./autogen.sh (if building directly from the Mercurial repository)
$ ./configure --with-android-sdk=<path/to/android/SDK>
$ make
```
  1. First build the external library dependencies:
```
$ cd android/extlibs
$ ./build.sh
```
  1. Build native Haggle code:
```
$ cd android/Haggle
$ <path_to_NDK>/ndk-build
```
  1. Build native application code (e.g.):
```
$ cd android/PhotoShare
$ <path_to_NDK>/ndk-build
```

Repeat step 4 for all Android applications that require native code to be built (e.g., LuckyMe).

Once the native components are built, you need to compile the managed (Java) components:

  1. Build Haggle's Java component (in Eclipse or using Ant)
    * Eclipse:
      * Import the directory 'android/Haggle' as an Android project.
      * Build (should be automatic)
    * Ant:
      * Make sure Ant is installed
      * Run make in application directories (e.g., android/Haggle)
  1. Build Haggle applications (in Eclipse or using Ant)
    * Eclipse:
      * Import application code in Eclipse as an Android project (e.g., android/PhotoShare)
      * Build org.haggle.jar from top dir (if not already done in first step)
```
./configure  --with-android-sdk=<path/to/android/SDK> && make
```
      * Copy src/libhaggle/jni/org.haggle.jar to android/myapp/libs
      * Add src/libhaggle/jni/org.haggle.jar as a referenced library: Eclipse->Project->Properties->Java Build Path->Libraries->Add External Jar
    * Ant:
      * Follow instructions in the previous step.

For reference, here is an example command trace for compiling:
```
$ hg clone https://code.google.com/p/haggle/
$ cd haggle
$ ./autogen.sh
$ ./configure --with-android-sdk=~/android-sdk-macosx/
$ make -j4
$ cd android/extlibs
$ echo "NDK_PATH=~/android-ndk-r5c" > .ndkconfig
$ ./build.sh -j4
$ cd ../Haggle/
$ ~/android-ndk-r5c/ndk-build -j4
$ ant debug install
$ cd ../PhotoShare
$ ~/android-ndk-r5c/ndk-build -j4
$ cp ../../src/libhaggle/jni/org.haggle.jar libs/
$ ant debug install
```

Now launch PhotoShare on your device.

# Installing #

The apk packages that the compilation process produces (e.g., Haggle.apk or PhotoShare.apk) need to be installed using the Eclipse IDE or with the adb command line tool. Please refer to the Android documentation on how to use these tools. You also need to allow applications from "Unknown sources" and USB debugging under Application settings on your device in order to install Haggle. To install, issue the following command:

```
$ adb install Haggle.apk
```

To uninstall:
```
$ adb uninstall org.haggle.kernel
```

# Enabling Bluetooth Support on Rooted Devices #


Haggle can use bluetooth on rooted devices by modifying the privileges given to Android applications with the BLUETOOTH\_ADMIN permission. This can be done relatively easily by modifying the /system/etc/permissions/platform.xml file on a device. Change the following section:
```
<permission name="android.permission.BLUETOOTH_ADMIN" >
        <group gid="net_bt_admin" />
</permission>
```
to
```
<permission name="android.permission.BLUETOOTH_ADMIN" >
        <group gid="net_bt_admin" />
        <group gid="bluetooth" />
</permission>             
```

This change allows Haggle to detect the device's local bluetooth hardware. After the file has been updated, a reboot might be required for the permission changes to take effect.