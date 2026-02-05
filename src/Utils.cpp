#include "Utils.h"

uint32_t SwapEndian(uint32_t val) {
    return ((val >> 24) & 0xff) |
           ((val << 8) & 0xff0000) |
           ((val >> 8) & 0xff00) |
           ((val << 24) & 0xff000000);
}

uint16_t SwapEndian(uint16_t val) {
    return ((val >> 8) & 0xff) |
           ((val << 8) & 0xff00);
}

bool FileExists(const std::string& name) {
    std::ifstream f(name.c_str());
    return f.good();
}

std::string GetFileNameWithoutExtension(const std::string& path) {
    size_t lastdot = path.find_last_of(".");
    size_t lastslash = path.find_last_of("/\\");
    if (lastslash == std::string::npos) {
        if (lastdot == std::string::npos) return path;
        return path.substr(0, lastdot);
    }
    if (lastdot == std::string::npos || lastdot < lastslash) return path.substr(lastslash + 1);
    return path.substr(lastslash + 1, lastdot - lastslash - 1);
}

void CreateDirectoryIfNotExists(const std::string& path) {
    MKDIR(path.c_str());
}
