# Introduction #

The Linux [wireless tools](http://www.hpl.hp.com/personal/Jean_Tourrilhes/Linux/Tools.html) are not installed on Android devices by default, but can easily be compiled using the NDK. The wireless tools are needed to, e.g., set WiFi ad-hoc mode.

# Prerequisites #

  * The source for the wireless tools, which can be downloaded from [here](http://www.hpl.hp.com/personal/Jean_Tourrilhes/Linux/Tools.html).
  * The Android NDK with the `net/ethernet.h` header from a Linux system, for example:


`cp /usr/include/net/ethernet.h /path/to/android-ndk-r6/platforms/android-9/arch-arm/usr/include/net/`

# Compiling #

Put the following Android.mk in the wireless tools directory:

```
LOCAL_PATH:= $(call my-dir)
################## build iwlib ###################
include $(CLEAR_VARS)
LOCAL_SRC_FILES := iwlib.c
LOCAL_CFLAGS += -Wstrict-prototypes -Wmissing-prototypes -Wshadow -Wpointer-arith -Wcast-qual -Winline -MMD -fPIC
LOCAL_MODULE:= libiw
LOCAL_STATIC_LIBRARIES := libcutils libc libm
include $(BUILD_STATIC_LIBRARY)
################## build iwconfig ###################
include $(CLEAR_VARS)
LOCAL_SRC_FILES := iwconfig.c
LOCAL_CFLAGS += -Wstrict-prototypes -Wmissing-prototypes -Wshadow -Wpointer-arith -Wcast-qual -Winline -MMD -fPIC
LOCAL_MODULE:= iwconfig
LOCAL_STATIC_LIBRARIES := libcutils libc libm libiw
LOCAL_MODULE_PATH := $(TARGET_OUT_OPTIONAL_EXECUTABLES) # install to system/xbin
include $(BUILD_EXECUTABLE)
################## build iwlist ###################
include $(CLEAR_VARS)
LOCAL_SRC_FILES := iwlist.c iwlib.h
LOCAL_CFLAGS += -Wstrict-prototypes -Wmissing-prototypes -Wshadow -Wpointer-arith -Wcast-qual -Winline -MMD -fPIC
LOCAL_MODULE:= iwlist
LOCAL_STATIC_LIBRARIES := libcutils libc libm libiw
LOCAL_MODULE_PATH := $(TARGET_OUT_OPTIONAL_EXECUTABLES) # install to system/xbin
include $(BUILD_EXECUTABLE)
```

Then, positioned in the wireless tools directory, compile with:

`/path/to/android-ndk-r6/ndk-build NDK_PROJECT_PATH=/path/to/wireless_tools.29/ APP_BUILD_SCRIPT=/path/to/wireless_tools.29/Android.mk SYSROOT=~/path/to/android-ndk-r6/platforms/android-9/arch-arm`

The binaries are found in the `libs` directory.