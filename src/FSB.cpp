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
    uint32_t flags = LE32(header.flags);

    uint32_t headerSize = DetectFSB4HeaderSize(f, 0, shdrSize);
    uint32_t currentSampleHeaderOffset = headerSize;
    uint32_t dataOffsetBase = headerSize + shdrSize;
    uint32_t currentDataOffset = 0;

    for (uint32_t i = 0; i < numSamples; ++i) {
        f.seekg(currentSampleHeaderOffset);
        uint16_t sampleHeaderSize;
        f.read((char*)&sampleHeaderSize, 2);
        sampleHeaderSize = LE16(sampleHeaderSize);

        char name[31];
        memset(name, 0, 31);
        f.read(name, 30);

        uint32_t offset = 0xFFFFFFFF;
        uint32_t compressedSize = 0;

        if (flags & 0x100) { // FSB_FLAGS_HAS_OFFSETS
            f.seekg(currentSampleHeaderOffset + 32);
            f.read((char*)&offset, 4);
            offset = LE32(offset);
            f.read((char*)&compressedSize, 4);
            compressedSize = LE32(compressedSize);
        } else {
            // Heuristic for MK9 vs Standard FMOD
            uint32_t val1, val2;
            f.seekg(currentSampleHeaderOffset + 32);
            f.read((char*)&val1, 4); val1 = LE32(val1);
            f.read((char*)&val2, 4); val2 = LE32(val2);

            // In MK9: 32 is uncompressed, 36 is compressed. Usually compressed < uncompressed.
            // In Standard: 32 is compressed, 36 is uncompressed.
            if (val1 > val2 && val2 > 0) {
                compressedSize = val2;
            } else {
                compressedSize = val1;
            }
        }

        FSBSample s;
        s.name = name;
        if (offset != 0xFFFFFFFF) {
            s.offset = dataOffsetBase + offset;
            currentDataOffset = offset + compressedSize;
        } else {
            s.offset = dataOffsetBase + currentDataOffset;
            currentDataOffset += compressedSize;
        }

        // Alignment (usually 32 bytes in FMOD)
        if (currentDataOffset % 32 != 0) {
            currentDataOffset += (32 - (currentDataOffset % 32));
        }

        s.size = compressedSize;
        s.headerOffset = currentSampleHeaderOffset;
        s.headerSize = sampleHeaderSize;
        samples.push_back(s);

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
    f.seekg(0, std::ios::end);
    size_t actualFileSize = (size_t)f.tellg();
    f.seekg(0, std::ios::beg);

    for (auto& s : samples) {
        if (s.offset >= actualFileSize) continue;
        uint32_t toRead = s.size;
        if (s.offset + toRead > actualFileSize) toRead = (uint32_t)(actualFileSize - s.offset);
        if (toRead == 0) continue;

        std::ofstream sf(outDir + "/" + s.name + ".bin", std::ios::binary);
        f.seekg(s.offset);
        std::vector<char> buf(toRead);
        f.read(buf.data(), toRead);
        sf.write(buf.data(), toRead);
        sf.close();
    }
    std::cout << "Extracted " << samples.size() << " samples to " << outDir << std::endl;
}

uint32_t DetectFSB4HeaderSize(std::istream& f, size_t startPos, uint32_t shdrSize) {
    auto currentPos = f.tellg();
    f.seekg(startPos + 24);
    uint16_t testSize;
    if (!f.read((char*)&testSize, 2)) {
        f.clear();
        f.seekg(currentPos);
        return 48;
    }
    testSize = LE16(testSize);
    f.seekg(currentPos);
    // If the size at offset 24 looks like a valid sample header size (32-512 bytes)
    // and is less than or equal to total shdrSize, it's likely a 24-byte main header.
    if (testSize >= 32 && testSize <= 512 && testSize <= shdrSize) return 24;
    return 48;
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
        std::cout << "ERROR: New audio is TOO LARGE for this slot!" << std::endl;
        std::cout << "  Original size: " << it->size << " bytes" << std::endl;
        std::cout << "  Your file:     " << newSize << " bytes" << std::endl;
        std::cout << "  Please re-encode with lower quality to fit the original size." << std::endl;
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
