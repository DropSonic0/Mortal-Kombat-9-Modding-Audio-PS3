#include "FSB.h"
#include <cstring>
#include <algorithm>

std::vector<FSBSample> ParseFSB(const std::string& fsbPath, uint32_t baseOffset) {
    std::vector<FSBSample> samples;
    std::ifstream f(fsbPath, std::ios::binary);
    if (!f.is_open()) return samples;

    f.seekg(baseOffset);

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
        f.seekg(baseOffset + currentSampleHeaderOffset);
        uint16_t sampleHeaderSize;
        f.read((char*)&sampleHeaderSize, 2);
        sampleHeaderSize = LE16(sampleHeaderSize);

        char name[31];
        memset(name, 0, 31);
        f.read(name, 30);

        // This is a simplified parser. 
        // We need to find the size and offset of the audio data for this sample.
        // In some FSB4, it's in the sample header.
        
        // FSB4_SAMPLE_HEADER:
        // 0: size(2), 2: name(30), 32: numsamples(4), 36: compressedsize(4), 40: uncompressedsize(4),
        // 44: loopstart(4), 48: loopend(4), 52: mode(4), 56: def_freq(4), 60: def_vol(2),
        // 62: def_pan(2), 64: def_pri(2), 66: num_channels(2)

        f.seekg(baseOffset + currentSampleHeaderOffset + 32);
        uint32_t numSamples;
        f.read((char*)&numSamples, 4);
        numSamples = LE32(numSamples);

        uint32_t compressedSize;
        f.read((char*)&compressedSize, 4);
        compressedSize = LE32(compressedSize);

        uint32_t uncompressedSize;
        f.read((char*)&uncompressedSize, 4);
        uncompressedSize = LE32(uncompressedSize);

        uint32_t loopStart, loopEnd, mode;
        f.read((char*)&loopStart, 4);
        f.read((char*)&loopEnd, 4);
        f.read((char*)&mode, 4);
        loopStart = LE32(loopStart);
        loopEnd = LE32(loopEnd);
        mode = LE32(mode);

        int32_t frequency;
        f.read((char*)&frequency, 4);
        frequency = (int32_t)LE32((uint32_t)frequency);

        f.seekg(baseOffset + currentSampleHeaderOffset + 66);
        uint16_t channels;
        f.read((char*)&channels, 2);
        channels = LE16(channels);

        // Heuristic: Use the larger of compressed and uncompressed size for data offset calculation.
        uint32_t actualDataSize = (compressedSize > uncompressedSize) ? compressedSize : uncompressedSize;

        FSBSample s;
        s.name = name;
        s.offset = dataOffsetBase + currentDataOffset;
        s.size = actualDataSize;
        s.headerOffset = currentSampleHeaderOffset;
        s.headerSize = sampleHeaderSize;
        s.numSamples = numSamples;
        s.uncompressedSize = uncompressedSize;
        s.loopStart = loopStart;
        s.loopEnd = loopEnd;
        s.mode = mode;
        s.frequency = frequency;
        s.channels = channels;
        samples.push_back(s);

        std::cout << "  [Sample " << i << "] " << s.name << " | Offset: 0x" << std::hex << (baseOffset + s.offset)
                  << " | Size: " << std::dec << s.size << " bytes | End: 0x" << std::hex << (baseOffset + s.offset + s.size) << std::dec << std::endl;

        currentDataOffset += Align(actualDataSize, 32);
        currentSampleHeaderOffset += sampleHeaderSize;
    }

    return samples;
}

void ExtractFSB(const std::string& fsbPath, bool swapEndian) {
    auto samples = ParseFSB(fsbPath);
    if (samples.empty()) {
        std::cout << "No samples found or invalid FSB: " << fsbPath << std::endl;
        return;
    }

    std::string outDir = GetFileNameWithoutExtension(fsbPath) + "_samples";
    CreateDirectoryIfNotExists(outDir);

    std::ifstream f(fsbPath, std::ios::binary);
    int sIdx = 0;
    for (auto& s : samples) {
        std::string sName = std::to_string(sIdx) + "_" + s.name + ".bin";
        std::ofstream sf(outDir + "/" + sName, std::ios::binary);
        f.seekg(s.offset);
        std::vector<char> buf(s.size);
        f.read(buf.data(), s.size);
        
        if (swapEndian) {
            for (size_t i = 0; i < buf.size() - 1; i += 2) {
                char tmp = buf[i];
                buf[i] = buf[i + 1];
                buf[i + 1] = tmp;
            }
        }

        sf.write(buf.data(), s.size);
        sf.close();
        sIdx++;
    }
    std::cout << "Extracted " << samples.size() << " samples to " << outDir << (swapEndian ? " (with Endian Swap)" : "") << std::endl;
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

    if (newSize < it->size / 1.5) {
        std::cout << "Warning: New data is much smaller than the original slot. If the sound is corrupt, use 'patchfromfsb' with a source FSB to update metadata (channels/frequency)." << std::endl;
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

bool PatchFSBSampleByIndex(const std::string& fsbPath, int sampleIndex, const std::string& newSampleDataPath) {
    std::vector<FSBSample> samples = ParseFSB(fsbPath);
    if (sampleIndex < 0 || sampleIndex >= (int)samples.size()) {
        std::cout << "Invalid sample index: " << sampleIndex << " (Total samples: " << samples.size() << ")" << std::endl;
        return false;
    }

    FSBSample& s = samples[sampleIndex];

    std::ifstream newData(newSampleDataPath, std::ios::binary);
    if (!newData.is_open()) {
        std::cout << "Failed to open new audio data: " << newSampleDataPath << std::endl;
        return false;
    }

    newData.seekg(0, std::ios::end);
    uint32_t newSize = (uint32_t)newData.tellg();
    newData.seekg(0, std::ios::beg);

    if (newSize > s.size) {
        std::cout << "Error: New sample data is larger than original (" << newSize << " > " << s.size << "). In-place patching failed." << std::endl;
        return false;
    }

    if (newSize < s.size / 1.5) {
        std::cout << "Warning: New data is much smaller than the original slot. If the sound is corrupt, use 'patchfromfsb' with a source FSB to update metadata (channels/frequency)." << std::endl;
    }

    std::fstream fsbFile(fsbPath, std::ios::binary | std::ios::in | std::ios::out);
    if (!fsbFile.is_open()) {
        std::cout << "Failed to open " << fsbPath << " for writing." << std::endl;
        return false;
    }
    fsbFile.seekp(s.offset);

    std::vector<char> buffer(newSize);
    newData.read(buffer.data(), newSize);
    fsbFile.write(buffer.data(), newSize);

    if (newSize < s.size) {
        std::vector<char> padding(s.size - newSize, 0);
        fsbFile.write(padding.data(), padding.size());
    }

    std::cout << "Successfully patched sample index " << sampleIndex << " (" << s.name << ") in " << fsbPath << std::endl;
    return true;
}

bool PatchFSBFromSource(const std::string& targetPath, const std::string& sourcePath) {
    auto targetSamples = ParseFSB(targetPath);
    auto sourceSamples = ParseFSB(sourcePath);

    if (targetSamples.empty() || sourceSamples.empty()) return false;

    std::fstream targetF(targetPath, std::ios::binary | std::ios::in | std::ios::out);
    std::ifstream sourceF(sourcePath, std::ios::binary);

    if (!targetF.is_open() || !sourceF.is_open()) return false;

    int matchCount = 0;
    for (auto& t : targetSamples) {
        for (auto& s : sourceSamples) {
            if (t.name == s.name) {
                // Found a match. Patch metadata and data.

                // Patch Metadata (skip size and compressedSize)
                targetF.seekp(t.headerOffset + 32);
                uint32_t numSamplesBE = LE32(s.numSamples);
                targetF.write((char*)&numSamplesBE, 4);

                targetF.seekp(t.headerOffset + 40); // Skip compressedSize at 36
                uint32_t uncompSizeBE = LE32(s.uncompressedSize);
                uint32_t loopStartBE = LE32(s.loopStart);
                uint32_t loopEndBE = LE32(s.loopEnd);
                uint32_t modeBE = LE32(s.mode);
                uint32_t freqBE = (uint32_t)LE32((uint32_t)s.frequency);

                targetF.write((char*)&uncompSizeBE, 4);
                targetF.write((char*)&loopStartBE, 4);
                targetF.write((char*)&loopEndBE, 4);
                targetF.write((char*)&modeBE, 4);
                targetF.write((char*)&freqBE, 4);

                targetF.seekp(t.headerOffset + 66);
                uint16_t chanBE = LE16(s.channels);
                targetF.write((char*)&chanBE, 2);

                // Patch Data
                if (s.size > t.size) {
                    std::cout << "  Warning: Source sample " << s.name << " is larger than target slot (" << s.size << " > " << t.size << "). It will be truncated." << std::endl;
                }

                targetF.seekp(t.offset);
                sourceF.seekg(s.offset);

                uint32_t toCopy = (s.size > t.size) ? t.size : s.size;
                std::vector<char> buf(toCopy);
                sourceF.read(buf.data(), toCopy);
                targetF.write(buf.data(), toCopy);

                if (toCopy < t.size) {
                    std::vector<char> padding(t.size - toCopy, 0);
                    targetF.write(padding.data(), padding.size());
                }

                std::cout << "  Patched " << t.name << " (Metadata & Data)" << std::endl;
                matchCount++;
                break;
            }
        }
    }

    std::cout << "Finished patching. Total matches: " << matchCount << std::endl;
    return true;
}
