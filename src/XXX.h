#ifndef XXX_H
#define XXX_H

#include "FSB.h"

void ExtractXXX(const std::string& path);
void PackXXX(const std::string& headerPath, const std::string& dataPath, const std::string& outPath);
void InjectAsset(const std::string& targetPath, const std::string& assetPath, uint32_t offset);
void PatchXXXAudio(const std::string& xxxPath, const std::string& sampleName, const std::string& newAudioPath);
void PatchXXXAudioByIndex(const std::string& xxxPath, int fsbIndex, int sampleIndex, const std::string& newAudioPath);
void PatchAllXXXAudio(const std::string& xxxPath, const std::string& folderPath);
void ReplaceXXXFSB(const std::string& xxxPath, int targetIndex, const std::string& newFsbPath);

#endif
