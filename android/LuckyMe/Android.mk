LOCAL_PATH := $(call my-dir)
include $(CLEAR_VARS)

# Build all java files in the src subdirectory
LOCAL_SRC_FILES := $(call all-java-files-under,src)

LOCAL_JAVA_LIBRARIES += org.haggle
#LOCAL_JNI_SHARED_LIBRARIES += libhaggle_jni

# Name of the APK to build
LOCAL_PACKAGE_NAME := LuckyMe

# Tell it to build an APK
include $(BUILD_PACKAGE)

