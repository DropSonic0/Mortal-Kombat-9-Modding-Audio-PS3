#include "FSB.h"
#include <cstring>
#include <algorithm>

std::string GetFormatString(uint32_t mode) {
    if (mode & 0x00000008) return "PCM8";
    if (mode & 0x00000010) return "PCM16";
    if (mode & 0x00000020) return "PCM24";
    if (mode & 0x00000040) return "PCM32";
    if (mode & 0x00000080) return "PCMFLOAT";
    if (mode & 0x00000100) return "GCADPCM";
    if (mode & 0x00000200) return "IMAADPCM";
    if (mode & 0x00000400) return "VAG";
    if (mode & 0x00000800) return "HEVAG";
    if (mode & 0x00001000) return "XMA";
    if (mode & 0x00002000) return "MPEG";
    if (mode & 0x00004000) return "CELT";
    if (mode & 0x00008000) return "AT9";
    if (mode & 0x00010000) return "XWMA";
    if (mode & 0x00020000) return "VORBIS";
    return "UNKNOWN";
}

std::vector<FSBSample> ParseFSB(const std::string& fsbPath, uint32_t baseOffset, uint32_t displayOffset) {
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

        uint32_t finalDisplayOffset = (displayOffset > 0) ? displayOffset : baseOffset;

        std::cout << "  [Sample " << i << "] " << s.name << " | Format: " << GetFormatString(mode)
                  << " | Channels: " << channels << " | Freq: " << frequency << "Hz"
                  << " | Offset: 0x" << std::hex << (finalDisplayOffset + s.offset)
                  << " | Size: " << std::dec << s.size << " bytes | End: 0x" << std::hex << (finalDisplayOffset + s.offset + s.size) << std::dec << std::endl;

        currentDataOffset += Align(actualDataSize, 32);
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
        std::string sName = s.name + ".bin";
        std::ofstream sf(outDir + "/" + sName, std::ios::binary);
        f.seekg(s.offset);
        std::vector<char> buf(s.size);
        f.read(buf.data(), s.size);
        
        sf.write(buf.data(), s.size);
        sf.close();
    }
    std::cout << "Extracted " << samples.size() << " samples to " << outDir << std::endl;
}

