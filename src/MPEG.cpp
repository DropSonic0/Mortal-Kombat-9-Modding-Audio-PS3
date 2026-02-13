#include "MPEG.h"
#include "Utils.h"
#include <cstring>
#include <fstream>
#include <iostream>
#include <vector>

// MPEG Frame Header Analysis
bool IsValidMPEGHeader(const unsigned char* h) {
    if (h[0] != 0xFF) return false;
    if ((h[1] & 0xE0) != 0xE0) return false; // Sync bits (first 11 bits must be 1)

    int version = (h[1] >> 3) & 0x03;
    if (version == 1) return false; // Reserved version

    int layer = (h[1] >> 1) & 0x03;
    if (layer == 0) return false; // Reserved layer

    int bitrateIdx = (h[2] >> 4) & 0x0F;
    if (bitrateIdx == 0 || bitrateIdx == 15) return false; // forbidden

    int freqIdx = (h[2] >> 2) & 0x03;
    if (freqIdx == 3) return false; // forbidden

    // MK9 PS3 usually uses MPEG 1 Layer 3 (0xFB) or Layer 2 (0xF3 / 0xF2)
    // Common: FF FB (V1 L3), FF F3 (V2 L3), FF F2 (V2 L2)

    return true;
}

int GetMPEGFrameSize(const unsigned char* h) {
    static const int bitrates[3][3][16] = {
        {{0, 32, 64, 96, 128, 160, 192, 224, 256, 288, 320, 352, 384, 416, 448, 0}, // V1, L1
         {0, 32, 48, 56, 64, 80, 96, 112, 128, 160, 192, 224, 256, 320, 384, 0},    // V1, L2
         {0, 32, 40, 48, 56, 64, 80, 96, 112, 128, 160, 192, 224, 256, 320, 0}},   // V1, L3
        {{0, 32, 48, 56, 64, 80, 96, 112, 128, 144, 160, 176, 192, 224, 256, 0}, // V2, L1
         {0, 8, 16, 24, 32, 40, 48, 56, 64, 80, 96, 112, 128, 144, 160, 0},      // V2, L2
         {0, 8, 16, 24, 32, 40, 48, 56, 64, 80, 96, 112, 128, 144, 160, 0}},     // V2, L3
        {{0, 32, 48, 56, 64, 80, 96, 112, 128, 144, 160, 176, 192, 224, 256, 0}, // V2.5, L1
         {0, 8, 16, 24, 32, 40, 48, 56, 64, 80, 96, 112, 128, 144, 160, 0},      // V2.5, L2
         {0, 8, 16, 24, 32, 40, 48, 56, 64, 80, 96, 112, 128, 144, 160, 0}}      // V2.5, L3
    };
    static const int samplerates[3][3] = {
        {44100, 48000, 32000}, // V1
        {22050, 24000, 16000}, // V2
        {11025, 12000, 8000}   // V2.5
    };

    int versionIdx = (h[1] >> 3) & 0x03; // 00:2.5, 01:reserved, 10:2, 11:1
    int versionMapping[] = {2, 0, 1, 0}; // map to 0:V1, 1:V2, 2:V2.5
    int v = versionMapping[versionIdx];

    int layerIdx = (h[1] >> 1) & 0x03; // 00:res, 01:L3, 10:L2, 11:L1
    int layerMapping[] = {0, 2, 1, 0}; // map to 0:L1, 1:L2, 2:L3
    int l = layerMapping[layerIdx];

    int bitrate = bitrates[v][l][(h[2] >> 4) & 0x0F] * 1000;
    int samplerate = samplerates[v][(h[2] >> 2) & 0x03];
    int padding = (h[2] >> 1) & 0x01;

    if (samplerate == 0) return 0;
    if (l == 0) { // Layer 1
        return (12 * bitrate / samplerate + padding) * 4;
    } else { // Layer 2 & 3
        return 144 * bitrate / samplerate + padding;
    }
}

void ExtractMPEG(const std::string& path) {
    std::ifstream f(path, std::ios::binary | std::ios::ate);
    if (!f.is_open()) {
        std::cout << "Failed to open " << path << std::endl;
        return;
    }

    size_t fileSize = (size_t)f.tellg();
    f.seekg(0, std::ios::beg);

    if (fileSize > 500 * 1024 * 1024) { // 500MB limit for safety
        std::cout << "File too large for mass MPEG scanning: " << path << std::endl;
        return;
    }

    std::vector<unsigned char> buffer(fileSize);
    f.read((char*)buffer.data(), fileSize);
    f.close();

    std::string outDir = GetFileNameWithoutExtension(path) + "_mpeg";
    CreateDirectoryIfNotExists(outDir);

    int mpegCount = 0;
    for (size_t i = 0; i < fileSize - 4; ) {
        if (buffer[i] == 0xFF && (buffer[i+1] & 0xE0) == 0xE0) {
            if (IsValidMPEGHeader(&buffer[i])) {
                size_t startPos = i;
                size_t currentPos = i;
                int frames = 0;

                while (currentPos < fileSize - 4) {
                    if (IsValidMPEGHeader(&buffer[currentPos])) {
                        int size = GetMPEGFrameSize(&buffer[currentPos]);
                        if (size < 4 || (currentPos + size) > fileSize) break;
                        currentPos += size;
                        frames++;
                    } else {
                        break;
                    }
                }

                if (frames >= 1) { // Extremely lenient
                    std::string outPath = outDir + "/audio_" + std::to_string(mpegCount++) + ".mp3";
                    std::ofstream outf(outPath, std::ios::binary);
                    outf.write((char*)&buffer[startPos], currentPos - startPos);
                    outf.close();
                    std::cout << "Extracted MPEG audio [Index " << (mpegCount-1) << "] at 0x" << std::hex << startPos << std::dec
                              << " (" << (currentPos - startPos) << " bytes, " << frames << " frames)" << std::endl;
                    i = currentPos;
                    continue;
                }
            }
        }
        i++;
    }

    if (mpegCount == 0) {
        std::cout << "No MPEG streams found in " << path << std::endl;
    } else {
        std::cout << "Extracted " << mpegCount << " MPEG files to " << outDir << std::endl;
    }
}
