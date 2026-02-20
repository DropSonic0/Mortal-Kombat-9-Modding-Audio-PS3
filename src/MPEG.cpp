#include "MPEG.h"
#include "Utils.h"
#include <cstring>
#include <fstream>
#include <iostream>
#include <vector>

bool IsValidMPEGHeader(const unsigned char* h) {
    if (h[0] != 0xFF) return false;
    if ((h[1] & 0xE0) != 0xE0) return false;

    int version = (h[1] >> 3) & 0x03;
    if (version == 1) return false;

    int layer = (h[1] >> 1) & 0x03;
    if (layer == 0) return false;

    int bitrateIdx = (h[2] >> 4) & 0x0F;
    if (bitrateIdx == 0 || bitrateIdx == 15) return false;

    int freqIdx = (h[2] >> 2) & 0x03;
    if (freqIdx == 3) return false;

    return true;
}

int GetMPEGFrameSize(const unsigned char* h) {
    static const int bitrates[3][3][16] = {
        {{0, 32, 64, 96, 128, 160, 192, 224, 256, 288, 320, 352, 384, 416, 448, 0},
         {0, 32, 48, 56, 64, 80, 96, 112, 128, 160, 192, 224, 256, 320, 384, 0},
         {0, 32, 40, 48, 56, 64, 80, 96, 112, 128, 160, 192, 224, 256, 320, 0}},
        {{0, 32, 48, 56, 64, 80, 96, 112, 128, 144, 160, 176, 192, 224, 256, 0},
         {0, 8, 16, 24, 32, 40, 48, 56, 64, 80, 96, 112, 128, 144, 160, 0},
         {0, 8, 16, 24, 32, 40, 48, 56, 64, 80, 96, 112, 128, 144, 160, 0}},
        {{0, 32, 48, 56, 64, 80, 96, 112, 128, 144, 160, 176, 192, 224, 256, 0},
         {0, 8, 16, 24, 32, 40, 48, 56, 64, 80, 96, 112, 128, 144, 160, 0},
         {0, 8, 16, 24, 32, 40, 48, 56, 64, 80, 96, 112, 128, 144, 160, 0}}
    };
    static const int samplerates[3][3] = {
        {44100, 48000, 32000},
        {22050, 24000, 16000},
        {11025, 12000, 8000}
    };

    int versionIdx = (h[1] >> 3) & 0x03;
    int versionMapping[] = {2, 0, 1, 0};
    int v = versionMapping[versionIdx];

    int layerIdx = (h[1] >> 1) & 0x03;
    int layerMapping[] = {0, 2, 1, 0};
    int l = layerMapping[layerIdx];

    int bitrate = bitrates[v][l][(h[2] >> 4) & 0x0F] * 1000;
    int samplerate = samplerates[v][(h[2] >> 2) & 0x03];
    int padding = (h[2] >> 1) & 0x01;

    if (samplerate == 0) return 0;
    if (l == 0) return (12 * bitrate / samplerate + padding) * 4;
    return 144 * bitrate / samplerate + padding;
}

void ExtractMPEG(const std::string& path) {
    std::ifstream f(path, std::ios::binary | std::ios::ate);
    if (!f.is_open()) return;
    size_t fileSize = (size_t)f.tellg();
    f.seekg(0, std::ios::beg);

    std::string outDir = GetFileNameWithoutExtension(path) + "_mpeg";
    CreateDirectoryIfNotExists(outDir);

    int mpegCount = 0;
    const size_t bufSize = 1024 * 1024;
    std::vector<unsigned char> buffer(bufSize + 4096);

    for (size_t offset = 0; offset < fileSize; ) {
        f.seekg(offset);
        f.read((char*)buffer.data(), bufSize + 4096);
        size_t bytesRead = (size_t)f.gcount();
        if (bytesRead < 4) break;

        bool foundInBlock = false;
        for (size_t i = 0; i < bytesRead - 4; ++i) {
            if (buffer[i] == 0xFF && (buffer[i+1] & 0xE0) == 0xE0) {
                if (IsValidMPEGHeader(&buffer[i])) {
                    size_t streamStart = offset + i;
                    size_t currentStreamOffset = streamStart;
                    int frames = 0;

                    while (true) {
                        unsigned char header[4];
                        f.seekg(currentStreamOffset);
                        if (!f.read((char*)header, 4)) break;

                        if (IsValidMPEGHeader(header)) {
                            int size = GetMPEGFrameSize(header);
                            if (size < 4) break;
                            currentStreamOffset += size;
                            frames++;

                            // Check next
                            unsigned char nextH[4];
                            if (!f.read((char*)nextH, 4)) break;
                            if (!IsValidMPEGHeader(nextH)) {
                                // Gap scan
                                bool foundNext = false;
                                for (int gap = 1; gap <= 1024; ++gap) {
                                    f.seekg(currentStreamOffset + gap);
                                    if (!f.read((char*)nextH, 4)) break;
                                    if (IsValidMPEGHeader(nextH)) {
                                        currentStreamOffset += gap;
                                        foundNext = true;
                                        break;
                                    }
                                }
                                if (!foundNext) break;
                            }
                        } else break;
                    }

                    if (frames >= 10) { // Require more frames to be sure it's audio
                        std::string outPath = outDir + "/audio_" + std::to_string(mpegCount++) + ".mp3";
                        std::ofstream outf(outPath, std::ios::binary);
                        f.seekg(streamStart);
                        size_t totalStreamSize = currentStreamOffset - streamStart;
                        std::vector<char> streamBuf(8192);
                        size_t remaining = totalStreamSize;
                        while (remaining > 0) {
                            size_t chunk = (remaining > 8192) ? 8192 : remaining;
                            f.read(streamBuf.data(), chunk);
                            outf.write(streamBuf.data(), f.gcount());
                            remaining -= (size_t)f.gcount();
                            if (f.gcount() == 0) break;
                        }
                        outf.close();
                        std::cout << "Extracted MPEG [Index " << (mpegCount-1) << "] at 0x" << std::hex << streamStart << std::dec << " (" << totalStreamSize << " bytes)" << std::endl;
                        offset = currentStreamOffset;
                        foundInBlock = true;
                        break;
                    }
                }
            }
        }
        if (!foundInBlock) offset += bufSize;
    }
}
