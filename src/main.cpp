#include "XXX.h"
#include <iostream>
#include <string>

void PrintUsage() {
    std::cout << "MK9Tool for PS3 by Jules" << std::endl;
    std::cout << "Usage:" << std::endl;
    std::cout << "  Extraction: MK9Tool <file.xxx>" << std::endl;
    std::cout << "  Packing:    MK9Tool <header.bin> <data.bin> <out.xxx>" << std::endl;
    std::cout << "  Injection:  MK9Tool inject <target_file> <new_asset> <offset_hex>" << std::endl;
    std::cout << "  Patching:   MK9Tool patch <xxx_file> <sample_name> <new_audio_bin>" << std::endl;
}

int main(int argc, char* argv[]) {
    if (argc < 2) {
        PrintUsage();
        return 1;
    }

    std::string arg1 = argv[1];

    if (arg1 == "inject") {
        if (argc < 5) {
            PrintUsage();
            return 1;
        }
        try {
            uint32_t offset = (uint32_t)std::stoul(argv[4], nullptr, 16);
            InjectAsset(argv[2], argv[3], offset);
        } catch (...) {
            std::cout << "Invalid offset format. Use hex (e.g. 0x123 or 123)" << std::endl;
            return 1;
        }
    } else if (arg1 == "patch") {
        if (argc < 5) {
            PrintUsage();
            return 1;
        }
        PatchXXXAudio(argv[2], argv[3], argv[4]);
    } else if (argc == 2) {
        ExtractXXX(argv[1]);
    } else if (argc == 4) {
        PackXXX(argv[1], argv[2], argv[3]);
    } else {
        PrintUsage();
        return 1;
    }

    return 0;
}
