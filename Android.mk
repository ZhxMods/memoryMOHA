LOCAL_PATH := $(call my-dir)

include $(CLEAR_VARS)

# اسم الملف التنفيذي
LOCAL_MODULE    := memoryMOHA

# الملفات المصدرية
LOCAL_SRC_FILES := main.cpp

# المكتبات المطلوبة
LOCAL_LDLIBS    := -llog

# إعدادات المترجم
LOCAL_CPPFLAGS  := -std=c++11 -O3 -Wall -Wextra -Wno-unused-parameter
LOCAL_CFLAGS    := -O3 -fvisibility=hidden

# تحديد نوع البناء كملف تنفيذي
include $(BUILD_EXECUTABLE)
