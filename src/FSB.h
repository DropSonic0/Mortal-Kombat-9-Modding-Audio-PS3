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

    // Metadata
    uint32_t numSamples;
    uint32_t uncompressedSize;
    uint32_t loopStart;
    uint32_t loopEnd;
    uint32_t mode;
    int32_t frequency;
    uint16_t channels;
};

std::vector<FSBSample> ParseFSB(const std::string& fsbPath, uint32_t baseOffset = 0);
void ExtractFSB(const std::string& fsbPath);

#endif
