#include "XXX.h"
#include "MPEG.h"
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

std::string CleanPath(std::string path) {
    if (!path.empty() && path.front() == '"' && path.back() == '"') {
        path = path.substr(1, path.size() - 2);
    }
    return path;
}

int main(int argc, char* argv[]) {
    if (argc < 2) {
        while (true) {
            std::cout << "\nMK9Tool Interactive Menu" << std::endl;
            std::cout << "1. Extract .XXX container" << std::endl;
            std::cout << "2. Patch All (Folder to .XXX)" << std::endl;
            std::cout << "3. Patch Single Sample (.bin to .XXX)" << std::endl;
            std::cout << "4. Extract .FSB file" << std::endl;
            std::cout << "5. Scan and Extract MPEG/MP3 (Advanced)" << std::endl;
            std::cout << "0. Exit" << std::endl;
            std::cout << "Select an option: ";

            std::string choice;
            std::getline(std::cin, choice);

            if (choice == "0") break;
            else if (choice == "1") {
                std::cout << "Enter .xxx path or folder path: ";
                std::string path;
                std::getline(std::cin, path);
                path = CleanPath(path);
                if (IsDirectory(path)) {
                    std::vector<std::string> files = GetFilesInDirectory(path);
                    for (const auto& file : files) {
                        if (file.size() >= 4 && (file.substr(file.size() - 4) == ".xxx" || file.substr(file.size() - 4) == ".XXX")) {
                            std::cout << "\nProcessing " << file << "..." << std::endl;
                            ExtractXXX(path + "/" + file);
                        }
                    }
                } else {
                    ExtractXXX(path);
                }
            } else if (choice == "2") {
                std::cout << "Enter .xxx path: ";
                std::string xxxPath;
                std::getline(std::cin, xxxPath);
                std::cout << "Enter folder path with .bin files: ";
                std::string folderPath;
                std::getline(std::cin, folderPath);
                PatchAllXXXAudio(CleanPath(xxxPath), CleanPath(folderPath));
            } else if (choice == "3") {
                std::cout << "Enter .xxx path: ";
                std::string xxxPath;
                std::getline(std::cin, xxxPath);
                std::cout << "Enter sample name (e.g. sfx_hit): ";
                std::string sampleName;
                std::getline(std::cin, sampleName);
                std::cout << "Enter new .bin path: ";
                std::string binPath;
                std::getline(std::cin, binPath);
                PatchXXXAudio(CleanPath(xxxPath), sampleName, CleanPath(binPath));
            } else if (choice == "4") {
                std::cout << "Enter .fsb path: ";
                std::string path;
                std::getline(std::cin, path);
                ExtractFSB(CleanPath(path));
            } else if (choice == "5") {
                std::cout << "Enter file path or folder path to scan for MP3: ";
                std::string path;
                std::getline(std::cin, path);
                path = CleanPath(path);
                if (IsDirectory(path)) {
                    std::vector<std::string> files = GetFilesInDirectory(path);
                    for (const auto& file : files) {
                        std::cout << "\nScanning " << file << "..." << std::endl;
                        ExtractMPEG(path + "/" + file);
                    }
                } else {
                    ExtractMPEG(path);
                }
            } else {
                std::cout << "Invalid option." << std::endl;
            }
            std::cout << "\n-----------------------------------" << std::endl;
        }
        return 0;
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
    } else if (arg1 == "mpegscan") {
        if (argc < 3) {
            PrintUsage();
            return 1;
        }
        ExtractMPEG(argv[2]);
    } else if (argc == 2) {
        std::string input = argv[1];
        if (IsDirectory(input)) {
            std::cout << "Directory detected. Choose action:\n1. Patch all .bin files from this folder into an .xxx file\n2. Extract all .xxx files in this folder\n3. Scan all files in this folder for MPEG/MP3\nSelect (1-3): ";
            std::string action;
            std::getline(std::cin, action);
            if (action == "1") {
                std::cout << "Enter the .xxx file to inject into: ";
                std::string xxx_file;
                std::getline(std::cin, xxx_file);
                PatchAllXXXAudio(CleanPath(xxx_file), input);
            } else if (action == "2") {
                std::vector<std::string> files = GetFilesInDirectory(input);
                for (const auto& file : files) {
                    if (file.size() >= 4 && (file.substr(file.size() - 4) == ".xxx" || file.substr(file.size() - 4) == ".XXX")) {
                        std::cout << "\nProcessing " << file << "..." << std::endl;
                        ExtractXXX(input + "/" + file);
                    }
                }
            } else if (action == "3") {
                std::vector<std::string> files = GetFilesInDirectory(input);
                for (const auto& file : files) {
                    std::cout << "\nScanning " << file << "..." << std::endl;
                    ExtractMPEG(input + "/" + file);
                }
            }
        } else if (input.size() >= 4 && (input.substr(input.size() - 4) == ".xxx" || input.substr(input.size() - 4) == ".XXX")) {
            ExtractXXX(input);
        } else if (input.size() >= 4 && (input.substr(input.size() - 4) == ".fsb" || input.substr(input.size() - 4) == ".FSB")) {
            ExtractFSB(input);
        } else if (input.size() >= 8 && (input.substr(input.size() - 8) == ".wav.bin" || input.substr(input.size() - 8) == ".WAV.BIN")) {
            std::string xxx_file;
            std::cout << "Enter the .xxx file to inject into: ";
            std::getline(std::cin, xxx_file);
            std::string sample_name = GetFileNameWithoutExtension(input);
            PatchXXXAudio(CleanPath(xxx_file), sample_name, input);
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
