#include "XXX.h"
#include <iostream>
#include <string>

void PrintUsage() {
    std::cout << "MK9Tool for PS3 by Jules" << std::endl;
    std::cout << "Usage:" << std::endl;
    std::cout << "  Extraction: MK9Tool <file.xxx>" << std::endl;
    std::cout << "  Patch All:  MK9Tool patchall <xxx_file> <folder_with_bins>" << std::endl;
    std::cout << "  Patching:   MK9Tool patch <xxx_file> <sample_name> <new_audio_bin>" << std::endl;
    std::cout << "  Extr. FSB:  MK9Tool extractfsb <fsb_file>" << std::endl;
}

int main(int argc, char* argv[]) {
    if (argc < 2) {
        PrintUsage();
        return 1;
    }

    std::string arg1 = argv[1];

    if (arg1 == "patch") {
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
    } else if (arg1 == "extractfsb") {
        if (argc < 3) {
            PrintUsage();
            return 1;
        }
        ExtractFSB(argv[2]);
    } else if (argc == 2) {
        std::string filename = argv[1];
        if (filename.find(".xxx") != std::string::npos || filename.find(".XXX") != std::string::npos) {
            ExtractXXX(argv[1]);
        } else {
            PrintUsage();
            return 1;
        }
    } else {
        PrintUsage();
        return 1;
    }

    return 0;
}
