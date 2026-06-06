LOCAL_PATH := $(call my-dir)

include $(CLEAR_VARS)
LOCAL_MODULE := libhwdb_sensor
LOCAL_SRC_FILES := \
    src/hwdb_parser.cpp \
    src/modalias.cpp
LOCAL_C_INCLUDES := $(LOCAL_PATH)/include
LOCAL_EXPORT_C_INCLUDE_DIRS := $(LOCAL_PATH)/include
LOCAL_STATIC_LIBRARIES := libsmbios_parser
LOCAL_CPPFLAGS := -std=c++14 -fexceptions
LOCAL_MODULE_TAGS := optional
include $(BUILD_STATIC_LIBRARY)
