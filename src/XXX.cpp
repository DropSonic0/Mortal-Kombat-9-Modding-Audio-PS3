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
                // Support 'FSB' followed by '1'-'6' (ASCII) or byte value 1-6
                if (sig[0] == 'S' && sig[1] == 'B' &&
                   ((sig[2] >= '1' && sig[2] <= '6') || (sig[2] >= 1 && sig[2] <= 6))) {

                    char versionChar = (sig[2] >= 1 && sig[2] <= 6) ? (sig[2] + '0') : sig[2];
                    size_t startPos = (size_t)f.tellg() - 4;
                    std::cout << "Found FSB" << versionChar << " [Index " << fsbCount << "] at 0x" << std::hex << startPos << std::dec << std::endl;

                    uint32_t shdrSize = 0;
                    uint32_t dataSize = 0;
                    uint32_t totalFSBSize = 0;

                    if (versionChar == '4') {
                        FSB4_HEADER fsbHeader;
                        f.seekg(startPos);
                        f.read((char*)&fsbHeader, sizeof(fsbHeader));
                        shdrSize = LE32(fsbHeader.shdr_size);
                        dataSize = LE32(fsbHeader.data_size);
                        totalFSBSize = sizeof(FSB4_HEADER) + shdrSize + dataSize;
                    } else {
                        // FSB5 - just a placeholder chunk
                        totalFSBSize = 1024 * 1024; // 1MB safe chunk
                    }

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
                        size_t paddingSize = totalFSBSize - toWrite;
                        if (paddingSize < 100 * 1024 * 1024) { // 100MB limit for safety
                            std::vector<char> padding(paddingSize, 0);
                            fsbf.write(padding.data(), padding.size());
                        }
                    }
                    fsbf.close();

                    // Sample extraction
                    std::string samplesDir = outDir + "/audio_" + std::to_string(fsbCount) + "_samples";
                    CreateDirectoryIfNotExists(samplesDir);
                    auto samples = ParseFSB(fsbOutPath, 0, (uint32_t)startPos);
                    if (!samples.empty()) {
                        std::ifstream fsbIn(fsbOutPath, std::ios::binary);
                        for (auto& s : samples) {
                            if (s.offset + s.size <= totalFSBSize) {
                                if (s.size > 0 && s.size < 100 * 1024 * 1024) { // 100MB limit
                                    std::string sName = s.name + ".bin";
                                    std::ofstream sf(samplesDir + "/" + sName, std::ios::binary);
                                    fsbIn.seekg(s.offset);
                                    std::vector<char> sbuf(s.size);
                                    fsbIn.read(sbuf.data(), s.size);
                                    sf.write(sbuf.data(), s.size);
                                    sf.close();
                                }
                            }
                        }
                    }

                    fsbCount++;
                    f.clear(); // Important if we reached EOF during extraction
                    f.seekg(startPos + 4); // Continue after 'FSB'
                } else {
                    f.seekg((size_t)f.tellg() - 3);
                }
            }
        }
    }
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
                if (sig[0] == 'S' && sig[1] == 'B' && (sig[2] == '4' || sig[2] == 4)) {
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
                            
                            std::cout << "Patched " << sampleName << " in " << xxxPath << " at 0x" << std::hex << (dataOffsetBase + currentDataOffset) << std::dec 
                                      << " (" << actualDataSize << " -> " << newSize << " bytes)" << std::endl;
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
                if (sig[0] == 'S' && sig[1] == 'B' && (sig[2] == '4' || sig[2] == 4)) {
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
                                    
                                    std::cout << "Auto-patched: " << sampleName << " [Offset: 0x" << std::hex << (dataOffsetBase + currentDataOffset) << std::dec << "] (" << actualDataSize << " -> " << newSize << " bytes)" << std::endl;
                                    patchCount++;
                                }
                            }
                        }
                        
                        currentDataOffset += Align(actualDataSize, 32);
                        currentSampleHeaderOffset += sampleHeaderSize;
                    }
                } else {
                    f.seekg((size_t)f.tellg() - 3);
                }
            }
        }
    }
    std::cout << "Finished. Total samples patched: " << patchCount << std::endl;
}

