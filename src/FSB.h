#ifndef FSB_H
#define FSB_H

#include "Utils.h"

#pragma pack(push, 1)
struct FSB4_HEADER {
    char magic[4]; // "FSB4"
    uint32_t numsamples;
    uint32_t shdr_size;
    uint32_t data_size;
    uint32_t version;
    uint32_t flags;
    char hash[16];
    char zero[8];
};
#pragma pack(pop)

struct FSBSample {
    std::string name;
    uint32_t offset;
    uint32_t size;
    uint32_t headerOffset;
    uint32_t headerSize;
};

std::vector<FSBSample> ParseFSB(const std::string& fsbPath);
void ExtractFSB(const std::string& fsbPath);
bool PatchFSBSample(const std::string& fsbPath, const std::string& sampleName, const std::string& newSampleDataPath);
uint32_t DetectFSB4HeaderSize(std::istream& f, size_t startPos, uint32_t shdrSize);

#endif
