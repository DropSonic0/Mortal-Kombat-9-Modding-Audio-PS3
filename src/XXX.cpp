#include "XXX.h"
#include <fstream>
#include <iostream>
#include <vector>
#include <iterator>

void ExtractXXX(const std::string& path) {
    std::ifstream f(path, std::ios::binary);
    if (!f.is_open()) {
        std::cout << "Failed to open " << path << std::endl;
        return;
    }

    uint32_t magic;
    f.read((char*)&magic, 4);
    if (SwapEndian(magic) != 0x9E2A83C1) {
        std::cout << "Warning: Invalid XXX magic" << std::endl;
    }

    f.seekg(8);
    uint32_t headerSize;
    f.read((char*)&headerSize, 4);
    headerSize = SwapEndian(headerSize);

    std::string outDir = GetFileNameWithoutExtension(path) + "_extracted";
    CreateDirectoryIfNotExists(outDir);

    f.seekg(0);
    std::vector<char> headerData(headerSize);
    f.read(headerData.data(), headerSize);

    std::ofstream hf(outDir + "/header.bin", std::ios::binary);
    hf.write(headerData.data(), headerSize);
    hf.close();

    std::ofstream df(outDir + "/data.bin", std::ios::binary);
    f.seekg(headerSize);
    df << f.rdbuf();
    df.close();

    std::cout << "Extracted header and data to " << outDir << std::endl;

    // Re-read file to scan for FSB
    f.clear();
    f.seekg(0, std::ios::beg);
    std::vector<char> allData((std::istreambuf_iterator<char>(f)), std::istreambuf_iterator<char>());

    int fsbCount = 0;
    if (allData.size() < 4) return;
    for (size_t i = 0; i < allData.size() - 3; ++i) {
        if (allData[i] == 'F' && allData[i+1] == 'S' && allData[i+2] == 'B') {
            std::cout << "Found FSB at 0x" << std::hex << i << std::dec << std::endl;

            FSB4_HEADER* fsbHeader = (FSB4_HEADER*)&allData[i];
            uint32_t shdrSize = fsbHeader->shdr_size;
            uint32_t dataSize = fsbHeader->data_size;

            // Based on memory: total size = sizeof(FSB4_HEADER) + shdr_size + data_size
            uint32_t totalFSBSize = sizeof(FSB4_HEADER) + shdrSize + dataSize;

            if (i + totalFSBSize > allData.size()) {
                totalFSBSize = (uint32_t)(allData.size() - i);
            }

            std::string fsbOutPath = outDir + "/audio_" + std::to_string(fsbCount) + ".fsb";
            std::ofstream fsbf(fsbOutPath, std::ios::binary);
            fsbf.write(&allData[i], totalFSBSize);
            fsbf.close();

            std::string samplesDir = outDir + "/audio_" + std::to_string(fsbCount) + "_samples";
            CreateDirectoryIfNotExists(samplesDir);
            auto samples = ParseFSB(fsbOutPath);
            for (auto& s : samples) {
                if (s.offset + s.size <= totalFSBSize) {
                    std::ofstream sf(samplesDir + "/" + s.name + ".bin", std::ios::binary);
                    sf.write(&allData[i + s.offset], s.size);
                    sf.close();
                }
            }

            fsbCount++;
            // We don't skip to stay "aggressive" as per memory, but usually skipping is fine.
            // i += 24 + shdrSize; // Skip header to avoid finding 'FSB' in names if any
        }
    }
}

void PackXXX(const std::string& headerPath, const std::string& dataPath, const std::string& outPath) {
    std::ofstream out(outPath, std::ios::binary);
    std::ifstream h(headerPath, std::ios::binary);
    std::ifstream d(dataPath, std::ios::binary);

    if (!h.is_open() || !d.is_open() || !out.is_open()) {
        std::cout << "Error opening files for packing" << std::endl;
        return;
    }

    out << h.rdbuf();
    out << d.rdbuf();
    std::cout << "Packed " << outPath << " successfully." << std::endl;
}

void InjectAsset(const std::string& targetPath, const std::string& assetPath, uint32_t offset) {
    std::fstream target(targetPath, std::ios::binary | std::ios::in | std::ios::out);
    std::ifstream asset(assetPath, std::ios::binary);

    if (!target.is_open() || !asset.is_open()) {
        std::cout << "Error opening files for injection" << std::endl;
        return;
    }

    target.seekp(offset);
    target << asset.rdbuf();
    std::cout << "Injected " << assetPath << " into " << targetPath << " at 0x" << std::hex << offset << std::dec << std::endl;
}

void PatchXXXAudio(const std::string& xxxPath, const std::string& sampleName, const std::string& newAudioPath) {
    // To patch XXX audio, we first need to find where the FSB is
    std::ifstream f(xxxPath, std::ios::binary);
    if (!f.is_open()) return;

    std::vector<char> allData((std::istreambuf_iterator<char>(f)), std::istreambuf_iterator<char>());
    f.close();

    bool found = false;
    for (size_t i = 0; i < allData.size() - 3; ++i) {
        if (allData[i] == 'F' && allData[i+1] == 'S' && allData[i+2] == 'B') {
            // Found an FSB, try to find the sample in it
            // We can temporarily save it to a file or parse from memory
            // For simplicity, let's reuse PatchFSBSample but we need it to work on the XXX directly or on a temp file

            // Let's implement a memory-based PatchFSBSample-like logic
            FSB4_HEADER* fsbHeader = (FSB4_HEADER*)&allData[i];
            uint32_t numSamples = fsbHeader->numsamples;
            uint32_t shdrSize = fsbHeader->shdr_size;

            uint32_t currentSampleHeaderOffset = (uint32_t)i + sizeof(FSB4_HEADER);
            uint32_t dataOffsetBase = (uint32_t)i + sizeof(FSB4_HEADER) + shdrSize;
            uint32_t currentDataOffset = 0;

            for (uint32_t j = 0; j < numSamples; ++j) {
                uint16_t sampleHeaderSize = *(uint16_t*)&allData[currentSampleHeaderOffset];
                char* name = &allData[currentSampleHeaderOffset + 2];

                if (std::string(name) == sampleName) {
                    uint32_t compressedSize = *(uint32_t*)&allData[currentSampleHeaderOffset + 2 + 30 + 4];

                    std::ifstream newData(newAudioPath, std::ios::binary);
                    if (!newData.is_open()) return;
                    newData.seekg(0, std::ios::end);
                    uint32_t newSize = (uint32_t)newData.tellg();
                    newData.seekg(0, std::ios::beg);

                    if (newSize > compressedSize) {
                        std::cout << "New audio too large for " << sampleName << std::endl;
                        return;
                    }

                    std::fstream xxxFile(xxxPath, std::ios::binary | std::ios::in | std::ios::out);
                    xxxFile.seekp(dataOffsetBase + currentDataOffset);
                    std::vector<char> buffer(newSize);
                    newData.read(buffer.data(), newSize);
                    xxxFile.write(buffer.data(), newSize);

                    // Padding
                    if (newSize < compressedSize) {
                        std::vector<char> padding(compressedSize - newSize, 0);
                        xxxFile.write(padding.data(), padding.size());
                    }

                    std::cout << "Patched " << sampleName << " in " << xxxPath << " at 0x" << std::hex << (dataOffsetBase + currentDataOffset) << std::dec << std::endl;
                    found = true;
                    break;
                }

                // Advance currentDataOffset
                uint32_t sampleCompressedSize = *(uint32_t*)&allData[currentSampleHeaderOffset + 2 + 30 + 4];
                currentDataOffset += sampleCompressedSize;
                currentSampleHeaderOffset += sampleHeaderSize;
            }
        }
        if (found) break;
    }

    if (!found) {
        std::cout << "Sample " << sampleName << " not found in " << xxxPath << std::endl;
    }
}
