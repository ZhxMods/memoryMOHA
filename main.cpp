#include <iostream>
#include <fstream>
#include <string>
#include <vector>
#include <unistd.h>
#include <sys/uio.h>
#include <cstring>
#include <stdlib.h>

using namespace std;

// دالة تحويل الهكس النصي إلى بايتات حقيقية
vector<unsigned char> hexToBytes(string hex) {
    vector<unsigned char> bytes;
    for (unsigned int i = 0; i < hex.length(); i++) {
        if (hex[i] == ' ' || hex[i] == '\n' || hex[i] == '\r') continue;
        if (i + 1 < hex.length()) {
            string byteString = hex.substr(i, 2);
            unsigned char byte = (unsigned char) strtol(byteString.c_str(), NULL, 16);
            bytes.push_back(byte);
            i++;
        }
    }
    return bytes;
}

void patch_memory(int pid, string sH, string rH) {
    if (sH.empty() || rH.empty()) return;
    
    vector<unsigned char> sBytes = hexToBytes(sH);
    vector<unsigned char> rBytes = hexToBytes(rH);

    char path[256];
    sprintf(path, "/proc/%d/maps", pid);
    ifstream maps(path);
    string line;

    while (getline(maps, line)) {
        if (line.find("rw-p") != string::npos) {
            unsigned long long start, end;
            if (sscanf(line.c_str(), "%llx-%llx", &start, &end) != 2) continue;
            
            size_t size = end - start;
            vector<unsigned char> buffer(size);

            struct iovec local[1];
            local[0].iov_base = buffer.data();
            local[0].iov_len = size;
            struct iovec remote[1];
            remote[0].iov_base = reinterpret_cast<void*>(start);
            remote[0].iov_len = size;

            // السطر 55 المصلح: مقارنة ssize_t مع size_t باستخدام Cast صحيح
            if (process_vm_readv(pid, local, 1, remote, 1, 0) == (ssize_t)size) {
                for (size_t i = 0; i <= size - sBytes.size(); ++i) {
                    if (memcmp(buffer.data() + i, sBytes.data(), sBytes.size()) == 0) {
                        
                        struct iovec local_w[1];
                        local_w[0].iov_base = rBytes.data();
                        local_w[0].iov_len = rBytes.size();
                        
                        struct iovec remote_w[1];
                        remote_w[0].iov_base = reinterpret_cast<void*>(start + i);
                        remote_w[0].iov_len = rBytes.size();

                        // السطر 62 المصلح: تمرير بارامترات الكتابة بشكل سليم
                        process_vm_writev(pid, local_w, 1, remote_w, 1, 0);
                    }
                }
            }
        }
    }
}

int main() {
    // الحصول على PID اللعبة (com.dts.freefireth)
    system("pidof com.dts.freefireth > /data/local/tmp/pid");
    ifstream pidFile("/data/local/tmp/pid");
    int pid = 0;
    pidFile >> pid;
    if (pid <= 0) return 1;

    // قراءة التعليمات من الملف المؤقت الذي أنشأه Sketchware
    ifstream hexFile("/data/local/tmp/hex");
    string line, sH, rH;
    while (getline(hexFile, line)) {
        if (line.find("Search:") != string::npos) getline(hexFile, sH);
        if (line.find("Replace:") != string::npos) getline(hexFile, rH);
    }
    
    patch_memory(pid, sH, rH);
    return 0;
}

