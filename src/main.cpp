#include "XXX.h"
#include <iostream>
#include <string>

void PrintUsage() {
    std::cout << "MK9Tool for PS3 by Jules" << std::endl;
    std::cout << "Usage:" << std::endl;
    std::cout << "  Extraction: MK9Tool <file.xxx> OR MK9Tool <file.fsb>" << std::endl;
    std::cout << "  Packing:    MK9Tool <header.bin> <data.bin> <out.xxx>" << std::endl;
    std::cout << "  Injection:  MK9Tool inject <target_file> <new_asset> <offset_hex>" << std::endl;
    std::cout << "  Replace FSB:MK9Tool replacefsb <xxx_file> <fsb_index> <new_fsb>" << std::endl;
    std::cout << "  Patch All:  MK9Tool patchall <xxx_file> <folder_with_bins>" << std::endl;
    std::cout << "  Patching:   MK9Tool patch <xxx_file> <sample_name> <new_audio_bin>" << std::endl;
    std::cout << "  Patch FSB:  MK9Tool patchfsb <fsb_file> <sample_name> <new_audio_bin>" << std::endl;
    std::cout << "  Patch FSBIdx:MK9Tool patchfsbidx <fsb_file> <sample_idx> <new_audio_bin>" << std::endl;
    std::cout << "  Patch XXXIdx:MK9Tool patchidx <xxx_file> <fsb_idx> <sample_idx> <new_audio_bin>" << std::endl;
    std::cout << "  Extr. FSB:  MK9Tool extractfsb <fsb_file> [--swap]" << std::endl;
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
    } else if (arg1 == "patchall" || arg1 == "all") {
        if (argc < 4) {
            PrintUsage();
            return 1;
        }
        PatchAllXXXAudio(argv[2], argv[3]);
    } else if (arg1 == "replacefsb") {
        if (argc < 5) {
            PrintUsage();
            return 1;
        }
        try {
            int index = std::stoi(argv[3]);
            ReplaceXXXFSB(argv[2], index, argv[4]);
        } catch (...) {
            std::cout << "Invalid index format. The index must be a number (e.g. 0, 1, 2...)" << std::endl;
            return 1;
        }
    } else if (arg1 == "patchfsb") {
        if (argc < 5) {
            PrintUsage();
            return 1;
        }
        PatchFSBSample(argv[2], argv[3], argv[4]);
    } else if (arg1 == "patchfsbidx") {
        if (argc < 5) {
            PrintUsage();
            return 1;
        }
        try {
            int sIdx = std::stoi(argv[3]);
            PatchFSBSampleByIndex(argv[2], sIdx, argv[4]);
        } catch (...) {
            std::cout << "Invalid sample index format." << std::endl;
            return 1;
        }
    } else if (arg1 == "patchidx") {
        if (argc < 6) {
            PrintUsage();
            return 1;
        }
        try {
            int fIdx = std::stoi(argv[3]);
            int sIdx = std::stoi(argv[4]);
            PatchXXXAudioByIndex(argv[2], fIdx, sIdx, argv[5]);
        } catch (...) {
            std::cout << "Invalid index format." << std::endl;
            return 1;
        }
    } else if (arg1 == "extractfsb") {
        if (argc < 3) {
            PrintUsage();
            return 1;
        }
        bool swap = (argc > 3 && std::string(argv[3]) == "--swap");
        ExtractFSB(argv[2], swap);
    } else if (argc == 2) {
        std::string ext = argv[1];
        if (ext.find(".fsb") != std::string::npos || ext.find(".FSB") != std::string::npos) {
            ExtractFSB(argv[1]);
        } else {
            ExtractXXX(argv[1]);
        }
    } else if (argc == 4) {
        PackXXX(argv[1], argv[2], argv[3]);
    } else {
        PrintUsage();
        return 1;
    }

    return 0;
}
