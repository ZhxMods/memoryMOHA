LOCAL_PATH := $(call my-dir)

include $(CLEAR_VARS)

# التغيير الجديد هنا ليطابق طلبك
LOCAL_MODULE    := memoryMOHA
LOCAL_SRC_FILES := main.cpp

# إضافة المكتبات اللازمة وتفعيل وضع السرعة القصوى O3
LOCAL_LDLIBS    := -llog
LOCAL_CPPFLAGS  := -std=c++11 -O3

# بناء كملف تنفيذي مستقل (لأنه يعمل مع الروت su)
include $(BUILD_EXECUTABLE)

