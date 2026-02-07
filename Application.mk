# دعم معالجات 32 و 64 بت (اللعبة X32 والموبايل غالباً X64)
APP_ABI := armeabi-v7a arm64-v8a

# الحد الأدنى لنسخة أندرويد (API 21)
APP_PLATFORM := android-21

# استخدام مكتبة STL ثابتة لضمان استقرار الحقن
APP_STL := c++_static

# تحسين الأداء النهائي
APP_OPTIM := release

