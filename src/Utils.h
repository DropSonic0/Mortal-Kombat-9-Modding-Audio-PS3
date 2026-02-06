#ifndef UTILS_H
#define UTILS_H

#include <iostream>
#include <vector>
#include <string>
#include <fstream>
#include <cstdint>

#ifdef _WIN32
#include <direct.h>
#define MKDIR(x) _mkdir(x)
#else
#include <sys/stat.h>
#include <sys/types.h>
#define MKDIR(x) mkdir(x, 0777)
#endif

uint32_t SwapEndian(uint32_t val);
uint16_t SwapEndian(uint16_t val);

inline uint32_t LE32(uint32_t v) {
#if defined(__BYTE_ORDER__) && __BYTE_ORDER__ == __ORDER_BIG_ENDIAN__
    return SwapEndian(v);
#else
    return v;
#endif
}

inline uint16_t LE16(uint16_t v) {
#if defined(__BYTE_ORDER__) && __BYTE_ORDER__ == __ORDER_BIG_ENDIAN__
    return SwapEndian(v);
#else
    return v;
#endif
}

inline uint32_t BE32(uint32_t v) {
#if defined(__BYTE_ORDER__) && __BYTE_ORDER__ == __ORDER_BIG_ENDIAN__
    return v;
#else
    return SwapEndian(v);
#endif
}

bool FileExists(const std::string& name);
std::string GetFileNameWithoutExtension(const std::string& path);
void CreateDirectoryIfNotExists(const std::string& path);
std::vector<std::string> GetFilesInDirectory(const std::string& path);

inline uint32_t Align(uint32_t val, uint32_t alignment) {
    if (alignment == 0) return val;
    return (val + alignment - 1) & ~(alignment - 1);
}

#endif
