LOCAL_PATH := $(my-dir)
include $(CLEAR_VARS)

# Do not build for simulator
ifneq ($(TARGET_SIMULATOR),true)

LOCAL_SRC_FILES := \
	String.cpp \
	Heap.cpp \
	Thread.cpp \
	Timeval.cpp \
	Watch.cpp \
	Mutex.cpp \
	Condition.cpp \
	Signal.cpp

LOCAL_C_INCLUDES += $(LOCAL_PATH)/include $(LOCAL_PATH)/../utils

LOCAL_MODULE := libcpphaggle

LOCAL_CFLAGS :=-O2 -g
LOCAL_CFLAGS +=-DHAVE_CONFIG -DOS_ANDROID -DHAVE_EXCEPTION=0 -DDEBUG -DDEBUG_LEAKS

include $(BUILD_STATIC_LIBRARY)

endif
