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

bool FileExists(const std::string& name);
std::string GetFileNameWithoutExtension(const std::string& path);
void CreateDirectoryIfNotExists(const std::string& path);

#endif
