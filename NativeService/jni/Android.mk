LOCAL_PATH := $(call my-dir)

include $(CLEAR_VARS)

LOCAL_MODULE := ConnectaConsole

LOCAL_SRC_FILES := \
	main.cpp \

LOCAL_CPPFLAGS := -std=gnu++0x -Wall

LOCAL_LDLIBS := -llog
LOCAL_SHARED_LIBRARIES := libcutils

include $(BUILD_EXECUTABLE)
