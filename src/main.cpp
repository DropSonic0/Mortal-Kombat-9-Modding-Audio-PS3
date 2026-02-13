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
        std::string input = argv[1];
        if (IsDirectory(input)) {
            std::string xxx_file;
            std::cout << "Enter the .xxx file to inject into: ";
            std::getline(std::cin, xxx_file);
            if (!xxx_file.empty() && xxx_file.front() == '"' && xxx_file.back() == '"') {
                xxx_file = xxx_file.substr(1, xxx_file.size() - 2);
            }
            PatchAllXXXAudio(xxx_file, input);
        } else if (input.size() >= 4 && (input.substr(input.size() - 4) == ".xxx" || input.substr(input.size() - 4) == ".XXX")) {
            ExtractXXX(input);
        } else if (input.size() >= 4 && (input.substr(input.size() - 4) == ".fsb" || input.substr(input.size() - 4) == ".FSB")) {
            ExtractFSB(input);
        } else if (input.size() >= 8 && (input.substr(input.size() - 8) == ".wav.bin" || input.substr(input.size() - 8) == ".WAV.BIN")) {
            std::string xxx_file;
            std::cout << "Enter the .xxx file to inject into: ";
            std::getline(std::cin, xxx_file);
            if (!xxx_file.empty() && xxx_file.front() == '"' && xxx_file.back() == '"') {
                xxx_file = xxx_file.substr(1, xxx_file.size() - 2);
            }
            std::string sample_name = GetFileNameWithoutExtension(input);
            PatchXXXAudio(xxx_file, sample_name, input);
        } else {
            PrintUsage();
            return 1;
        }
    } else {
        PrintUsage();
        return 1;
    }

    std::cout << "\nPress Enter to exit..." << std::endl;
    std::cin.get();
    return 0;
}
