# Copyright 2008 Uppsala University

LOCAL_PATH:= $(call my-dir)
include $(CLEAR_VARS)

# Do not build for simulator
ifneq ($(TARGET_SIMULATOR),true)
# Copyright 2008 Uppsala University

subdirs := $(addprefix $(LOCAL_PATH)/,$(addsuffix /Android.mk, \
                extlibs/libxml2-2.6.31 \
                src/libcpphaggle \
                src/hagglekernel \
                src/libhaggle \
        ))

include $(subdirs)

endif
