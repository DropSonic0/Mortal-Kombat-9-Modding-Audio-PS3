#include "XXX.h"
#include <fstream>
#include <iostream>
#include <vector>
#include <iterator>
#include <cstring>

void ExtractXXX(const std::string& path) {
    std::ifstream f(path, std::ios::binary);
    if (!f.is_open()) {
        std::cout << "Failed to open " << path << std::endl;
        return;
    }

    uint32_t magic;
    f.read((char*)&magic, 4);
    if (BE32(magic) != 0x9E2A83C1) {
        std::cout << "Warning: Invalid XXX magic" << std::endl;
    }

    f.seekg(8);
    uint32_t headerSize;
    f.read((char*)&headerSize, 4);
    headerSize = BE32(headerSize);

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
    
    // Buffered copy for data.bin
    char buf[8192];
    while (f.read(buf, sizeof(buf))) df.write(buf, sizeof(buf));
    df.write(buf, f.gcount());
    df.close();

    std::cout << "Extracted header and data to " << outDir << std::endl;

    // Scanning for FSB without loading entire file
    f.clear();
    f.seekg(0, std::ios::beg);
    
    int fsbCount = 0;
    size_t pos = 0;
    while (true) {
        char c;
        if (!f.get(c)) break;
        if (c == 'F') {
            char sig[3];
            if (f.read(sig, 3)) {
                if (sig[0] == 'S' && sig[1] == 'B') {
                    size_t startPos = (size_t)f.tellg() - 4;
                    std::cout << "Found FSB" << sig[2] << " [Index " << fsbCount << "] at 0x" << std::hex << startPos << std::dec << std::endl;

                    uint32_t shdrSize = 0;
                    uint32_t dataSize = 0;
                    uint32_t totalFSBSize = 0;

                    if (sig[2] == '4') {
                        FSB4_HEADER fsbHeader;
                        f.seekg(startPos);
                        f.read((char*)&fsbHeader, sizeof(fsbHeader));
                        shdrSize = LE32(fsbHeader.shdr_size);
                        dataSize = LE32(fsbHeader.data_size);
                        totalFSBSize = sizeof(FSB4_HEADER) + shdrSize + dataSize;
                    } else if (sig[2] == '5') {
                        // FSB5 - just a placeholder chunk
                        totalFSBSize = 1024 * 1024; // 1MB safe chunk
                    } else {
                        // Generic FSB or other version - use a reasonable chunk if unknown
                        totalFSBSize = 1024;
                    }

                    f.clear();
                    f.seekg(0, std::ios::end);
                    size_t fileSize = (size_t)f.tellg();
                    f.seekg(startPos);

                    size_t available = fileSize - startPos;
                    size_t toWrite = totalFSBSize;
                    if (toWrite > available) {
                        toWrite = available;
                        std::cout << "  -> Detected Streaming Bank (truncated). Padding to match header size." << std::endl;
                    }

                    std::string fsbOutPath = outDir + "/audio_" + std::to_string(fsbCount) + ".fsb";
                    std::ofstream fsbf(fsbOutPath, std::ios::binary);
                    
                    size_t remaining = toWrite;
                    while (remaining > 0) {
                        size_t chunk = (remaining > sizeof(buf)) ? sizeof(buf) : remaining;
                        f.read(buf, chunk);
                        fsbf.write(buf, f.gcount());
                        remaining -= f.gcount();
                        if (f.gcount() == 0) break;
                    }

                    if (toWrite < totalFSBSize) {
                        std::vector<char> padding(totalFSBSize - toWrite, 0);
                        fsbf.write(padding.data(), padding.size());
                    }
                    fsbf.close();

                    // Sample extraction
                    std::string samplesDir = outDir + "/audio_" + std::to_string(fsbCount) + "_samples";
                    CreateDirectoryIfNotExists(samplesDir);
                    // Use path and startPos to read from original file
                    auto samples = ParseFSB(path, (uint32_t)startPos);
                    if (!samples.empty()) {
                        std::ifstream fsbIn(fsbOutPath, std::ios::binary);
                        int sIdx = 0;
                        for (auto& s : samples) {
                            if (s.offset + s.size <= totalFSBSize) {
                                std::string sName = std::to_string(sIdx) + "_" + s.name + ".bin";
                                std::ofstream sf(samplesDir + "/" + sName, std::ios::binary);
                                fsbIn.seekg(s.offset);
                                std::vector<char> sbuf(s.size);
                                fsbIn.read(sbuf.data(), s.size);
                                sf.write(sbuf.data(), s.size);
                                sf.close();
                            }
                            sIdx++;
                        }
                    }

                    fsbCount++;
                    f.clear();
                    f.seekg(startPos + 4); // Continue after 'FSB'
                } else {
                    f.seekg((size_t)f.tellg() - 3);
                }
            }
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
    std::ifstream f(xxxPath, std::ios::binary);
    if (!f.is_open()) return;

    // Scan for FSB4 without loading everything
    bool found = false;
    while (true) {
        char c;
        if (!f.get(c)) break;
        if (c == 'F') {
            char sig[3];
            if (f.read(sig, 3)) {
                if (sig[0] == 'S' && sig[1] == 'B') {
                    size_t startPos = (size_t)f.tellg() - 4;
                    
                    FSB4_HEADER fsbHeader;
                    f.seekg(startPos);
                    f.read((char*)&fsbHeader, sizeof(fsbHeader));
                    
                    uint32_t numSamples = LE32(fsbHeader.numsamples);
                    uint32_t shdrSize = LE32(fsbHeader.shdr_size);
                    
                    uint32_t currentSampleHeaderOffset = (uint32_t)startPos + sizeof(FSB4_HEADER);
                    uint32_t dataOffsetBase = (uint32_t)startPos + sizeof(FSB4_HEADER) + shdrSize;
                    uint32_t currentDataOffset = 0;

                    for (uint32_t j = 0; j < numSamples; ++j) {
                        f.seekg(currentSampleHeaderOffset);
                        uint16_t sampleHeaderSize;
                        f.read((char*)&sampleHeaderSize, 2);
                        sampleHeaderSize = LE16(sampleHeaderSize);
                        
                        char name[31];
                        memset(name, 0, 31);
                        f.read(name, 30);
                        
                        if (std::string(name) == sampleName) {
                            f.seekg(currentSampleHeaderOffset + 2 + 30 + 4);
                            uint32_t compressedSize;
                            f.read((char*)&compressedSize, 4);
                            compressedSize = LE32(compressedSize);
                            
                            uint32_t uncompressedSize;
                            f.read((char*)&uncompressedSize, 4);
                            uncompressedSize = LE32(uncompressedSize);

                            uint32_t actualDataSize = (compressedSize > uncompressedSize) ? compressedSize : uncompressedSize;

                            std::ifstream newData(newAudioPath, std::ios::binary);
                            if (!newData.is_open()) {
                                std::cout << "Failed to open new audio data" << std::endl;
                                return;
                            }
                            newData.seekg(0, std::ios::end);
                            uint32_t newSize = (uint32_t)newData.tellg();
                            newData.seekg(0, std::ios::beg);

                            if (newSize > actualDataSize) {
                                std::cout << "New audio too large for " << sampleName << " (" << newSize << " > " << actualDataSize << ")" << std::endl;
                                return;
                            }

                            if (newSize < actualDataSize / 1.5) {
                                std::cout << "Warning: New data is much smaller than the original slot. If the sound is corrupt, use 'patchfromfsb' with a source FSB to update metadata (channels/frequency)." << std::endl;
                            }

                            std::fstream xxxFile(xxxPath, std::ios::binary | std::ios::in | std::ios::out);
                            xxxFile.seekp(dataOffsetBase + currentDataOffset);
                            
                            char patchBuf[4096];
                            while (newData.read(patchBuf, sizeof(patchBuf))) xxxFile.write(patchBuf, sizeof(patchBuf));
                            xxxFile.write(patchBuf, newData.gcount());
                            
                            if (newSize < actualDataSize) {
                                std::vector<char> padding(actualDataSize - newSize, 0);
                                xxxFile.write(padding.data(), padding.size());
                            }
                            
                            std::cout << "Patched " << sampleName << " in " << xxxPath << " at 0x" << std::hex << (dataOffsetBase + currentDataOffset) << std::dec << std::endl;
                            found = true;
                            break;
                        }
                        
                        // Advance
                        f.seekg(currentSampleHeaderOffset + 2 + 30 + 4);
                        uint32_t sampleCompressedSize;
                        f.read((char*)&sampleCompressedSize, 4);
                        sampleCompressedSize = LE32(sampleCompressedSize);
                        
                        uint32_t sampleUncompressedSize;
                        f.read((char*)&sampleUncompressedSize, 4);
                        sampleUncompressedSize = LE32(sampleUncompressedSize);

                        uint32_t actualSampleDataSize = (sampleCompressedSize > sampleUncompressedSize) ? sampleCompressedSize : sampleUncompressedSize;

                        currentDataOffset += Align(actualSampleDataSize, 32);
                        currentSampleHeaderOffset += sampleHeaderSize;
                    }
                    f.clear();
                } else {
                    f.seekg((size_t)f.tellg() - 3);
                }
            }
        }
        if (found) break;
    }
    
    if (!found) {
        std::cout << "Sample " << sampleName << " not found in " << xxxPath << std::endl;
    }
}

void PatchXXXAudioByIndex(const std::string& xxxPath, int targetFsbIndex, int targetSampleIndex, const std::string& newAudioPath) {
    std::ifstream f(xxxPath, std::ios::binary);
    if (!f.is_open()) {
        std::cout << "Failed to open " << xxxPath << std::endl;
        return;
    }

    int fsbCount = 0;
    bool found = false;
    while (true) {
        char c;
        if (!f.get(c)) break;
        if (c == 'F') {
            char sig[3];
            if (f.read(sig, 3)) {
                if (sig[0] == 'S' && sig[1] == 'B') {
                    size_t startPos = (size_t)f.tellg() - 4;
                    if (fsbCount == targetFsbIndex) {
                        FSB4_HEADER fsbHeader;
                        f.seekg(startPos);
                        f.read((char*)&fsbHeader, sizeof(fsbHeader));
                        
                        uint32_t numSamples = LE32(fsbHeader.numsamples);
                        uint32_t shdrSize = LE32(fsbHeader.shdr_size);
                        
                        if (targetSampleIndex < 0 || targetSampleIndex >= (int)numSamples) {
                            std::cout << "Invalid sample index " << targetSampleIndex << " for FSB index " << targetFsbIndex << " (Total samples: " << numSamples << ")" << std::endl;
                            return;
                        }

                        uint32_t currentSampleHeaderOffset = (uint32_t)startPos + sizeof(FSB4_HEADER);
                        uint32_t dataOffsetBase = (uint32_t)startPos + sizeof(FSB4_HEADER) + shdrSize;
                        uint32_t currentDataOffset = 0;

                        for (uint32_t j = 0; j < numSamples; ++j) {
                            f.seekg(currentSampleHeaderOffset);
                            uint16_t sampleHeaderSize;
                            f.read((char*)&sampleHeaderSize, 2);
                            sampleHeaderSize = LE16(sampleHeaderSize);
                            
                            char name[31];
                            memset(name, 0, 31);
                            f.read(name, 30);

                            f.seekg(currentSampleHeaderOffset + 2 + 30 + 4);
                            uint32_t compressedSize;
                            f.read((char*)&compressedSize, 4);
                            compressedSize = LE32(compressedSize);
                            
                            uint32_t uncompressedSize;
                            f.read((char*)&uncompressedSize, 4);
                            uncompressedSize = LE32(uncompressedSize);

                            uint32_t actualDataSize = (compressedSize > uncompressedSize) ? compressedSize : uncompressedSize;

                            if ((int)j == targetSampleIndex) {
                                std::ifstream newData(newAudioPath, std::ios::binary);
                                if (!newData.is_open()) {
                                    std::cout << "Failed to open new audio data" << std::endl;
                                    return;
                                }
                                newData.seekg(0, std::ios::end);
                                uint32_t newSize = (uint32_t)newData.tellg();
                                newData.seekg(0, std::ios::beg);

                                if (newSize > actualDataSize) {
                                    std::cout << "New audio too large for " << name << " (" << newSize << " > " << actualDataSize << ")" << std::endl;
                                    return;
                                }

                                if (newSize < actualDataSize / 1.5) {
                                    std::cout << "Warning: New data is much smaller than the original slot. If the sound is corrupt, use 'patchfromfsb' with a source FSB to update metadata (channels/frequency)." << std::endl;
                                }

                                std::fstream xxxFile(xxxPath, std::ios::binary | std::ios::in | std::ios::out);
                                xxxFile.seekp(dataOffsetBase + currentDataOffset);
                                
                                char patchBuf[4096];
                                while (newData.read(patchBuf, sizeof(patchBuf))) xxxFile.write(patchBuf, sizeof(patchBuf));
                                xxxFile.write(patchBuf, newData.gcount());
                                
                                if (newSize < actualDataSize) {
                                    std::vector<char> padding(actualDataSize - newSize, 0);
                                    xxxFile.write(padding.data(), padding.size());
                                }
                                
                                std::cout << "Patched sample index " << j << " (" << name << ") in FSB " << targetFsbIndex << " at 0x" << std::hex << (dataOffsetBase + currentDataOffset) << std::dec << std::endl;
                                found = true;
                                break;
                            }
                            
                            currentDataOffset += Align(actualDataSize, 32);
                            currentSampleHeaderOffset += sampleHeaderSize;
                        }
                        if (found) break;
                    }
                    
                    // Skip this FSB
                    f.clear();
                    FSB4_HEADER fsbHeader;
                    f.seekg(startPos);
                    f.read((char*)&fsbHeader, sizeof(fsbHeader));
                    uint32_t size = sizeof(FSB4_HEADER) + LE32(fsbHeader.shdr_size) + LE32(fsbHeader.data_size);
                    f.seekg(startPos + size);
                    f.clear();
                    fsbCount++;
                } else {
                    f.seekg((size_t)f.tellg() - 3);
                }
            }
        }
        if (found) break;
    }
    
    if (!found) {
        std::cout << "FSB index " << targetFsbIndex << " not found in " << xxxPath << std::endl;
    }
}

void PatchAllXXXAudio(const std::string& xxxPath, const std::string& folderPath) {
    std::vector<std::string> files = GetFilesInDirectory(folderPath);
    if (files.empty()) {
        std::cout << "No files found in folder " << folderPath << std::endl;
        return;
    }

    std::ifstream f(xxxPath, std::ios::binary);
    if (!f.is_open()) {
        std::cout << "Failed to open " << xxxPath << std::endl;
        return;
    }

    int patchCount = 0;
    while (true) {
        char c;
        if (!f.get(c)) break;
        if (c == 'F') {
            char sig[3];
            if (f.read(sig, 3)) {
                if (sig[0] == 'S' && sig[1] == 'B') {
                    size_t startPos = (size_t)f.tellg() - 4;
                    
                    FSB4_HEADER fsbHeader;
                    f.seekg(startPos);
                    f.read((char*)&fsbHeader, sizeof(fsbHeader));
                    
                    uint32_t numSamples = LE32(fsbHeader.numsamples);
                    uint32_t shdrSize = LE32(fsbHeader.shdr_size);
                    
                    uint32_t currentSampleHeaderOffset = (uint32_t)startPos + sizeof(FSB4_HEADER);
                    uint32_t dataOffsetBase = (uint32_t)startPos + sizeof(FSB4_HEADER) + shdrSize;
                    uint32_t currentDataOffset = 0;

                    for (uint32_t j = 0; j < numSamples; ++j) {
                        f.seekg(currentSampleHeaderOffset);
                        uint16_t sampleHeaderSize;
                        f.read((char*)&sampleHeaderSize, 2);
                        sampleHeaderSize = LE16(sampleHeaderSize);
                        
                        char name[31];
                        memset(name, 0, 31);
                        f.read(name, 30);
                        std::string sampleName(name);

                        f.seekg(currentSampleHeaderOffset + 2 + 30 + 4);
                        uint32_t compressedSize;
                        f.read((char*)&compressedSize, 4);
                        compressedSize = LE32(compressedSize);
                        
                        uint32_t uncompressedSize;
                        f.read((char*)&uncompressedSize, 4);
                        uncompressedSize = LE32(uncompressedSize);

                        uint32_t actualDataSize = (compressedSize > uncompressedSize) ? compressedSize : uncompressedSize;

                        // Check if we have a matching file
                        std::string matchingFile = "";
                        for (const auto& file : files) {
                            if (file == sampleName || file == (sampleName + ".bin") ||
                                file == (std::to_string(j) + ".bin") ||
                                file.find(std::to_string(j) + "_") == 0) {
                                matchingFile = folderPath + "/" + file;
                                break;
                            }
                        }

                        if (!matchingFile.empty()) {
                            std::ifstream newData(matchingFile, std::ios::binary);
                            if (newData.is_open()) {
                                newData.seekg(0, std::ios::end);
                                uint32_t newSize = (uint32_t)newData.tellg();
                                newData.seekg(0, std::ios::beg);

                                if (newSize > actualDataSize) {
                                    std::cout << "Warning: " << matchingFile << " too large (" << newSize << " > " << actualDataSize << "). Skipping." << std::endl;
                                } else {
                                    if (newSize < actualDataSize / 1.5) {
                                        std::cout << "  Warning: New data is much smaller than original. Suggest using 'patchfromfsb'." << std::endl;
                                    }
                                    std::fstream xxxFile(xxxPath, std::ios::binary | std::ios::in | std::ios::out);
                                    xxxFile.seekp(dataOffsetBase + currentDataOffset);
                                    
                                    char patchBuf[4096];
                                    while (newData.read(patchBuf, sizeof(patchBuf))) xxxFile.write(patchBuf, sizeof(patchBuf));
                                    xxxFile.write(patchBuf, newData.gcount());
                                    
                                    if (newSize < actualDataSize) {
                                        std::vector<char> padding(actualDataSize - newSize, 0);
                                        xxxFile.write(padding.data(), padding.size());
                                    }
                                    
                                    std::cout << "Auto-patched: " << sampleName << " [Offset: 0x" << std::hex << (dataOffsetBase + currentDataOffset) << std::dec << "]" << std::endl;
                                    patchCount++;
                                }
                            }
                        }
                        
                        currentDataOffset += Align(actualDataSize, 32);
                        currentSampleHeaderOffset += sampleHeaderSize;
                    }
                    f.clear();
                } else {
                    f.seekg((size_t)f.tellg() - 3);
                }
            }
        }
    }
    std::cout << "Finished. Total samples patched: " << patchCount << std::endl;
}

void PatchXXXFromSourceFSB(const std::string& xxxPath, const std::string& sourceFsbPath) {
    auto sourceSamples = ParseFSB(sourceFsbPath);
    if (sourceSamples.empty()) return;

    std::ifstream f(xxxPath, std::ios::binary);
    if (!f.is_open()) {
        std::cout << "Failed to open " << xxxPath << std::endl;
        return;
    }

    int fsbCount = 0;
    while (true) {
        char c;
        if (!f.get(c)) break;
        if (c == 'F') {
            char sig[3];
            if (f.read(sig, 3)) {
                if (sig[0] == 'S' && sig[1] == 'B') {
                    size_t startPos = (size_t)f.tellg() - 4;
                    std::cout << "Patching internal FSB [Index " << fsbCount << "] from source..." << std::endl;

                    // We need a temporary file for the internal FSB to use PatchFSBFromSource
                    // but it's better to just do it in-place here to avoid copies.
                    
                    auto targetSamples = ParseFSB(xxxPath, (uint32_t)startPos);
                    // ParseFSB handles the seeks internally if we give it the XXX path.
                    // Wait, ParseFSB currently opens the file. If I give it the XXX path, 
                    // it starts from the beginning of the file. 
                    // I need a version of ParseFSB that starts at an offset.
                    
                    // Actually, I'll just reuse the logic from PatchFSBFromSource but adapted for XXX offset.
                    
                    std::fstream xxxF(xxxPath, std::ios::binary | std::ios::in | std::ios::out);
                    std::ifstream srcF(sourceFsbPath, std::ios::binary);

                    for (auto& t : targetSamples) {
                        for (auto& s : sourceSamples) {
                            if (t.name == s.name) {
                                // Patch Metadata (using startPos + relative offsets)
                                xxxF.seekp(startPos + t.headerOffset + 32);
                                uint32_t nsBE = LE32(s.numSamples);
                                xxxF.write((char*)&nsBE, 4);
                                
                                xxxF.seekp(startPos + t.headerOffset + 40);
                                uint32_t usBE = LE32(s.uncompressedSize);
                                uint32_t lsBE = LE32(s.loopStart);
                                uint32_t leBE = LE32(s.loopEnd);
                                uint32_t moBE = LE32(s.mode);
                                uint32_t frBE = (uint32_t)LE32((uint32_t)s.frequency);
                                
                                xxxF.write((char*)&usBE, 4);
                                xxxF.write((char*)&lsBE, 4);
                                xxxF.write((char*)&leBE, 4);
                                xxxF.write((char*)&moBE, 4);
                                xxxF.write((char*)&frBE, 4);
                                
                                xxxF.seekp(startPos + t.headerOffset + 66);
                                uint16_t chBE = LE16(s.channels);
                                xxxF.write((char*)&chBE, 2);

                                // Patch Data
                                uint32_t toCopy = (s.size > t.size) ? t.size : s.size;
                                xxxF.seekp(startPos + t.offset);
                                srcF.seekg(s.offset);
                                std::vector<char> buf(toCopy);
                                srcF.read(buf.data(), toCopy);
                                xxxF.write(buf.data(), toCopy);

                                if (toCopy < t.size) {
                                    std::vector<char> padding(t.size - toCopy, 0);
                                    xxxF.write(padding.data(), padding.size());
                                }
                                std::cout << "  Auto-patched " << t.name << " from source FSB." << std::endl;
                                break;
                            }
                        }
                    }

                    fsbCount++;
                    
                    // Skip this FSB in the outer scan
                    f.clear();
                    FSB4_HEADER fsbHeader;
                    f.seekg(startPos);
                    f.read((char*)&fsbHeader, sizeof(fsbHeader));
                    uint32_t size = sizeof(FSB4_HEADER) + LE32(fsbHeader.shdr_size) + LE32(fsbHeader.data_size);
                    f.seekg(startPos + size);
                    f.clear();
                } else {
                    f.seekg((size_t)f.tellg() - 3);
                }
            }
        }
    }
}

/**
 * Replaces an FSB bank within a XXX package at a given index.
 * Note: This is an in-place replacement. It does not update the XXX package's
 * internal object tables/metadata. It uses padding if the new FSB is smaller.
 */
void ReplaceXXXFSB(const std::string& xxxPath, int targetIndex, const std::string& newFsbPath) {
    std::ifstream f(xxxPath, std::ios::binary);
    if (!f.is_open()) {
        std::cout << "Failed to open " << xxxPath << std::endl;
        return;
    }

    int fsbCount = 0;
    bool replaced = false;

    while (true) {
        char c;
        if (!f.get(c)) break;
        if (c == 'F') {
            char sig[3];
            if (f.read(sig, 3)) {
                if (sig[0] == 'S' && sig[1] == 'B') {
                    size_t startPos = (size_t)f.tellg() - 4;

                    if (fsbCount == targetIndex) {
                        uint32_t totalFSBSize = 0;
                        if (sig[2] == '4') {
                            FSB4_HEADER fsbHeader;
                            f.seekg(startPos);
                            f.read((char*)&fsbHeader, sizeof(fsbHeader));
                            totalFSBSize = sizeof(FSB4_HEADER) + LE32(fsbHeader.shdr_size) + LE32(fsbHeader.data_size);
                        } else {
                            std::cout << "FSB5 replacement not fully supported (unknown original size)." << std::endl;
                            return;
                        }

                        f.seekg(0, std::ios::end);
                        size_t fileSize = (size_t)f.tellg();
                        size_t availableSpace = fileSize - startPos;
                        
                        // If it's a streaming bank, the actual space in XXX might be smaller than totalFSBSize
                        uint32_t maxAllowedSize = (totalFSBSize < availableSpace) ? totalFSBSize : (uint32_t)availableSpace;

                        std::ifstream newF(newFsbPath, std::ios::binary);
                        if (!newF.is_open()) {
                            std::cout << "Failed to open new FSB: " << newFsbPath << std::endl;
                            return;
                        }
                        newF.seekg(0, std::ios::end);
                        uint32_t newSize = (uint32_t)newF.tellg();
                        newF.seekg(0, std::ios::beg);

                        if (newSize > maxAllowedSize) {
                            std::cout << "Error: New FSB is larger than available space (" << newSize << " > " << maxAllowedSize << ")" << std::endl;
                            return;
                        }

                        std::fstream xxxF(xxxPath, std::ios::binary | std::ios::in | std::ios::out);
                        if (!xxxF.is_open()) {
                            std::cout << "Failed to open " << xxxPath << " for writing." << std::endl;
                            return;
                        }
                        xxxF.seekp(startPos);
                        
                        char copyBuf[8192];
                        uint32_t remaining = newSize;
                        while (remaining > 0) {
                            uint32_t chunk = (remaining > sizeof(copyBuf)) ? sizeof(copyBuf) : remaining;
                            newF.read(copyBuf, chunk);
                            xxxF.write(copyBuf, newF.gcount());
                            remaining -= (uint32_t)newF.gcount();
                            if (newF.gcount() == 0) break;
                        }

                        if (newSize < maxAllowedSize) {
                            std::vector<char> padding(maxAllowedSize - newSize, 0);
                            xxxF.write(padding.data(), padding.size());
                        }

                        // Update XXX header metadata (UE3 BulkData)
                        uint32_t startPosBE = BE32((uint32_t)startPos);
                        uint32_t oldSizeBE = BE32(totalFSBSize);
                        uint32_t newSizeBE = BE32(newSize);

                        xxxF.clear();
                        xxxF.seekg(0);
                        std::vector<char> headerData(startPos);
                        xxxF.read(headerData.data(), startPos);

                        for (size_t i = 8; i <= (size_t)startPos - 4; ++i) {
                            uint32_t val;
                            memcpy(&val, &headerData[i], 4);
                            if (val == startPosBE) {
                                // Potentially found the offset field. Check preceding size fields.
                                uint32_t s1, s2;
                                if (i >= 8) {
                                    memcpy(&s1, &headerData[i - 4], 4);
                                    memcpy(&s2, &headerData[i - 8], 4);
                                    if (s1 == oldSizeBE && s2 == oldSizeBE) {
                                        xxxF.seekp(i - 8);
                                        xxxF.write((char*)&newSizeBE, 4);
                                        xxxF.write((char*)&newSizeBE, 4);
                                        std::cout << "Updated XXX header metadata at 0x" << std::hex << (i - 8) << std::dec << std::endl;
                                    }
                                }
                            }
                        }

                        std::cout << "Successfully replaced FSB index " << targetIndex << " in " << xxxPath << " (Offset: 0x" << std::hex << startPos << std::dec << ")" << std::endl;
                        replaced = true;
                        break;
                    }

                    // Not the index we want, skip it
                    f.clear();
                    if (sig[2] == '4') {
                        FSB4_HEADER fsbHeader;
                        f.seekg(startPos);
                        f.read((char*)&fsbHeader, sizeof(fsbHeader));
                        uint32_t size = sizeof(FSB4_HEADER) + LE32(fsbHeader.shdr_size) + LE32(fsbHeader.data_size);
                        f.seekg(startPos + size);
                    } else {
                         f.seekg(startPos + 4);
                    }
                    f.clear();
                    fsbCount++;
                } else {
                    f.seekg((size_t)f.tellg() - 3);
                }
            }
        }
    }

    if (!replaced) {
        std::cout << "FSB with index " << targetIndex << " not found in " << xxxPath << std::endl;
    }
}
