#include "FSB.h"
#include <cstring>
#include <algorithm>

std::vector<FSBSample> ParseFSB(const std::string& fsbPath) {
    std::vector<FSBSample> samples;
    std::ifstream f(fsbPath, std::ios::binary);
    if (!f.is_open()) return samples;

    FSB4_HEADER header;
    f.read((char*)&header, sizeof(header));
    if (strncmp(header.magic, "FSB4", 4) != 0) {
        if (strncmp(header.magic, "FSB5", 4) == 0) {
            std::cout << "FSB5 detected in " << fsbPath << ". FSB5 parsing is not fully implemented yet." << std::endl;
        }
        return samples;
    }

    // FSB4 headers are Little-Endian in MK9 PS3
    uint32_t numSamples = LE32(header.numsamples);
    uint32_t shdrSize = LE32(header.shdr_size);
    uint32_t dataSize = LE32(header.data_size);

    uint32_t currentSampleHeaderOffset = sizeof(FSB4_HEADER);
    uint32_t dataOffsetBase = sizeof(FSB4_HEADER) + shdrSize;
    uint32_t currentDataOffset = 0;

    for (uint32_t i = 0; i < numSamples; ++i) {
        f.seekg(currentSampleHeaderOffset);
        uint16_t sampleHeaderSize;
        f.read((char*)&sampleHeaderSize, 2);
        sampleHeaderSize = LE16(sampleHeaderSize);

        char name[31];
        memset(name, 0, 31);
        f.read(name, 30);

        // This is a simplified parser. 
        // We need to find the size and offset of the audio data for this sample.
        // In some FSB4, it's in the sample header.
        
        // Skip some fields to get to compressed size
        // FSB4_SAMPLE_HEADER: size(2), name(30), numsamples(4), compressedsize(4)...
        f.seekg(currentSampleHeaderOffset + 2 + 30 + 4);
        uint32_t compressedSize;
        f.read((char*)&compressedSize, 4);
        compressedSize = LE32(compressedSize);

        FSBSample s;
        s.name = name;
        s.offset = dataOffsetBase + currentDataOffset;
        s.size = compressedSize;
        s.headerOffset = currentSampleHeaderOffset;
        s.headerSize = sampleHeaderSize;
        samples.push_back(s);

        currentDataOffset += compressedSize;
        currentSampleHeaderOffset += sampleHeaderSize;
    }

    return samples;
}

void ExtractFSB(const std::string& fsbPath) {
    auto samples = ParseFSB(fsbPath);
    if (samples.empty()) {
        std::cout << "No samples found or invalid FSB: " << fsbPath << std::endl;
        return;
    }

    std::string outDir = GetFileNameWithoutExtension(fsbPath) + "_samples";
    CreateDirectoryIfNotExists(outDir);

    std::ifstream f(fsbPath, std::ios::binary);
    for (auto& s : samples) {
        std::ofstream sf(outDir + "/" + s.name + ".bin", std::ios::binary);
        f.seekg(s.offset);
        std::vector<char> buf(s.size);
        f.read(buf.data(), s.size);
        sf.write(buf.data(), s.size);
        sf.close();
    }
    std::cout << "Extracted " << samples.size() << " samples to " << outDir << std::endl;
}

bool PatchFSBSample(const std::string& fsbPath, const std::string& sampleName, const std::string& newSampleDataPath) {
    std::vector<FSBSample> samples = ParseFSB(fsbPath);
    auto it = std::find_if(samples.begin(), samples.end(), [&](const FSBSample& s) {
        return s.name == sampleName;
    });

    if (it == samples.end()) {
        std::cout << "Sample " << sampleName << " not found in " << fsbPath << std::endl;
        return false;
    }

    std::ifstream newData(newSampleDataPath, std::ios::binary);
    if (!newData.is_open()) return false;

    newData.seekg(0, std::ios::end);
    uint32_t newSize = (uint32_t)newData.tellg();
    newData.seekg(0, std::ios::beg);

    if (newSize > it->size) {
        std::cout << "Error: New sample data is larger than original (" << newSize << " > " << it->size << "). In-place patching failed." << std::endl;
        return false;
    }

    std::fstream fsbFile(fsbPath, std::ios::binary | std::ios::in | std::ios::out);
    if (!fsbFile.is_open()) {
        std::cout << "Failed to open " << fsbPath << " for writing." << std::endl;
        return false;
    }
    fsbFile.seekp(it->offset);
    
    std::vector<char> buffer(newSize);
    newData.read(buffer.data(), newSize);
    fsbFile.write(buffer.data(), newSize);

    // Pad with zeros if smaller
    if (newSize < it->size) {
        std::vector<char> padding(it->size - newSize, 0);
        fsbFile.write(padding.data(), padding.size());
    }

    // Optional: Update compressed size in header if necessary, 
    // but many players rely on the original size or padding is fine for most formats.
    // For now, we keep the original size in header to avoid shifting data.

    std::cout << "Successfully patched sample " << sampleName << " in " << fsbPath << std::endl;
    return true;
}
