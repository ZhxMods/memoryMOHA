# دعم معالجات ARM 32-bit و 64-bit
APP_ABI := armeabi-v7a arm64-v8a

# الحد الأدنى لإصدار Android (API 21 = Android 5.0)
APP_PLATFORM := android-21

# استخدام STL ثابتة لضمان التوافقية
APP_STL := c++_static

# وضع الإصدار النهائي
APP_OPTIM := release

# تفعيل معالجة الاستثناءات
APP_CPPFLAGS := -fexceptions -frtti

# تحسينات إضافية
APP_LDFLAGS := -Wl,--gc-sections
