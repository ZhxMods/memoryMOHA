#include <iostream>
#include <fstream>
#include <string>
#include <vector>
#include <sstream>
#include <unistd.h>
#include <sys/uio.h>
#include <sys/syscall.h>
#include <cstring>
#include <cstdlib>
#include <android/log.h>
#include <errno.h>

#define LOG_TAG "MemoryMOHA"
#define LOGI(...) __android_log_print(ANDROID_LOG_INFO, LOG_TAG, __VA_ARGS__)
#define LOGE(...) __android_log_print(ANDROID_LOG_ERROR, LOG_TAG, __VA_ARGS__)

using namespace std;

// Syscall numbers for process_vm_readv and process_vm_writev
#ifndef __NR_process_vm_readv
#if defined(__arm__)
#define __NR_process_vm_readv 376
#define __NR_process_vm_writev 377
#elif defined(__aarch64__)
#define __NR_process_vm_readv 270
#define __NR_process_vm_writev 271
#elif defined(__i386__)
#define __NR_process_vm_readv 347
#define __NR_process_vm_writev 348
#elif defined(__x86_64__)
#define __NR_process_vm_readv 310
#define __NR_process_vm_writev 311
#endif
#endif

// Wrapper functions for process_vm_readv and process_vm_writev
static ssize_t process_vm_readv_wrapper(pid_t pid,
                                        const struct iovec *local_iov,
                                        unsigned long liovcnt,
                                        const struct iovec *remote_iov,
                                        unsigned long riovcnt,
                                        unsigned long flags) {
    return syscall(__NR_process_vm_readv, pid, local_iov, liovcnt, remote_iov, riovcnt, flags);
}

static ssize_t process_vm_writev_wrapper(pid_t pid,
                                         const struct iovec *local_iov,
                                         unsigned long liovcnt,
                                         const struct iovec *remote_iov,
                                         unsigned long riovcnt,
                                         unsigned long flags) {
    return syscall(__NR_process_vm_writev, pid, local_iov, liovcnt, remote_iov, riovcnt, flags);
}

// دالة تحويل الهكس النصي إلى بايتات حقيقية
vector<unsigned char> hexToBytes(const string& hex) {
    vector<unsigned char> bytes;
    string cleanHex;
    
    // إزالة المسافات والأسطر الجديدة
    for (char c : hex) {
        if (c != ' ' && c != '\n' && c != '\r' && c != '\t') {
            cleanHex += c;
        }
    }
    
    // التحويل من هكس إلى بايتات
    for (size_t i = 0; i + 1 < cleanHex.length(); i += 2) {
        string byteString = cleanHex.substr(i, 2);
        unsigned char byte = static_cast<unsigned char>(strtol(byteString.c_str(), nullptr, 16));
        bytes.push_back(byte);
    }
    
    return bytes;
}

// دالة الحصول على PID من اسم الحزمة
int getPidByPackageName(const string& packageName) {
    char command[256];
    snprintf(command, sizeof(command), "pidof %s 2>/dev/null", packageName.c_str());
    
    FILE* pipe = popen(command, "r");
    if (!pipe) {
        LOGE("Failed to run pidof command");
        return -1;
    }
    
    int pid = -1;
    if (fscanf(pipe, "%d", &pid) != 1) {
        LOGE("Failed to read PID");
        pclose(pipe);
        return -1;
    }
    
    pclose(pipe);
    return pid;
}

// دالة التصحيح الرئيسية
bool patch_memory(int pid, const string& searchHex, const string& replaceHex) {
    if (searchHex.empty() || replaceHex.empty()) {
        LOGE("Search or replace hex is empty");
        return false;
    }
    
    if (pid <= 0) {
        LOGE("Invalid PID: %d", pid);
        return false;
    }
    
    vector<unsigned char> searchBytes = hexToBytes(searchHex);
    vector<unsigned char> replaceBytes = hexToBytes(replaceHex);
    
    if (searchBytes.empty() || replaceBytes.empty()) {
        LOGE("Failed to convert hex to bytes");
        return false;
    }
    
    LOGI("Searching for %zu bytes, replacing with %zu bytes", searchBytes.size(), replaceBytes.size());
    
    char mapsPath[256];
    snprintf(mapsPath, sizeof(mapsPath), "/proc/%d/maps", pid);
    
    ifstream mapsFile(mapsPath);
    if (!mapsFile.is_open()) {
        LOGE("Failed to open maps file: %s", mapsPath);
        return false;
    }
    
    string line;
    int patchCount = 0;
    
    while (getline(mapsFile, line)) {
        // البحث في المناطق القابلة للكتابة فقط
        if (line.find("rw-p") == string::npos) {
            continue;
        }
        
        // تجنب المناطق الحساسة
        if (line.find("[vdso]") != string::npos || 
            line.find("[vectors]") != string::npos ||
            line.find("[stack") != string::npos) {
            continue;
        }
        
        unsigned long long start, end;
        if (sscanf(line.c_str(), "%llx-%llx", &start, &end) != 2) {
            continue;
        }
        
        size_t regionSize = end - start;
        
        // تجاهل المناطق الكبيرة جداً أو الصغيرة جداً
        if (regionSize < searchBytes.size() || regionSize > 100 * 1024 * 1024) {
            continue;
        }
        
        vector<unsigned char> buffer(regionSize);
        
        struct iovec localVec[1];
        localVec[0].iov_base = buffer.data();
        localVec[0].iov_len = regionSize;
        
        struct iovec remoteVec[1];
        remoteVec[0].iov_base = reinterpret_cast<void*>(start);
        remoteVec[0].iov_len = regionSize;
        
        // قراءة الذاكرة
        ssize_t bytesRead = process_vm_readv_wrapper(pid, localVec, 1, remoteVec, 1, 0);
        if (bytesRead != static_cast<ssize_t>(regionSize)) {
            if (bytesRead < 0) {
                // LOGE("Failed to read memory region: %s", strerror(errno));
            }
            continue;
        }
        
        // البحث عن النمط
        for (size_t i = 0; i + searchBytes.size() <= regionSize; ++i) {
            if (memcmp(buffer.data() + i, searchBytes.data(), searchBytes.size()) == 0) {
                LOGI("Pattern found at address: 0x%llx + 0x%zx", start, i);
                
                // كتابة البايتات الجديدة
                struct iovec localWrite[1];
                localWrite[0].iov_base = const_cast<unsigned char*>(replaceBytes.data());
                localWrite[0].iov_len = replaceBytes.size();
                
                struct iovec remoteWrite[1];
                remoteWrite[0].iov_base = reinterpret_cast<void*>(start + i);
                remoteWrite[0].iov_len = replaceBytes.size();
                
                ssize_t bytesWritten = process_vm_writev_wrapper(pid, localWrite, 1, remoteWrite, 1, 0);
                
                if (bytesWritten == static_cast<ssize_t>(replaceBytes.size())) {
                    LOGI("Successfully patched at 0x%llx", start + i);
                    patchCount++;
                } else {
                    LOGE("Failed to write at 0x%llx: %s", start + i, strerror(errno));
                }
            }
        }
    }
    
    mapsFile.close();
    
    if (patchCount > 0) {
        LOGI("Total patches applied: %d", patchCount);
        return true;
    } else {
        LOGE("Pattern not found in memory");
        return false;
    }
}

int main(int argc, char* argv[]) {
    LOGI("MemoryMOHA started");
    
    // الحصول على PID
    int pid = getPidByPackageName("com.dts.freefireth");
    
    if (pid <= 0) {
        LOGE("Game process not found. Make sure Free Fire is running.");
        cout << "Error: Game not running" << endl;
        return 1;
    }
    
    LOGI("Found game PID: %d", pid);
    
    // قراءة التعليمات من الملف
    ifstream hexFile("/data/local/tmp/hex");
    if (!hexFile.is_open()) {
        LOGE("Failed to open hex file: /data/local/tmp/hex");
        cout << "Error: Hex file not found" << endl;
        return 1;
    }
    
    string line, searchHex, replaceHex;
    bool foundSearch = false, foundReplace = false;
    
    while (getline(hexFile, line)) {
        // إزالة المسافات البيضاء من البداية والنهاية
        line.erase(0, line.find_first_not_of(" \t\r\n"));
        line.erase(line.find_last_not_of(" \t\r\n") + 1);
        
        if (line.find("Search:") != string::npos) {
            if (getline(hexFile, searchHex)) {
                foundSearch = true;
                LOGI("Search hex loaded");
            }
        } else if (line.find("Replace:") != string::npos) {
            if (getline(hexFile, replaceHex)) {
                foundReplace = true;
                LOGI("Replace hex loaded");
            }
        }
    }
    
    hexFile.close();
    
    if (!foundSearch || !foundReplace) {
        LOGE("Invalid hex file format");
        cout << "Error: Invalid hex file format" << endl;
        return 1;
    }
    
    // تنفيذ التصحيح
    bool success = patch_memory(pid, searchHex, replaceHex);
    
    if (success) {
        cout << "Success: Memory patched successfully" << endl;
        LOGI("Memory patching completed successfully");
        return 0;
    } else {
        cout << "Error: Failed to patch memory" << endl;
        LOGE("Memory patching failed");
        return 1;
    }
}

