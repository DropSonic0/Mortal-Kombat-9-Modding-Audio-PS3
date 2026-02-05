#include <iostream>
#include <vector>
#include <fstream>
#include <string>
#include <stdint.h>
#include <algorithm>
#include <iomanip>
#include <cstring>

#ifdef _WIN32
#include <direct.h>
#define MKDIR(p) _mkdir(p)
#else
#include <sys/stat.h>
#define MKDIR(p) mkdir(p, 0777)
#endif

using namespace std;

// MK9 PS3 XXX Tool - Modding Tool PRO v5
// Herramienta optimizada para extraer, parchar y reconstruir archivos de MK9 PS3.

uint32_t swap32(uint32_t v) {
    return ((v >> 24) & 0xff) | ((v >> 8) & 0xff00) | ((v << 8) & 0xff0000) | ((v << 24) & 0xff000000);
}

uint32_t readLE32(const char* data) {
    const unsigned char* u = (const unsigned char*)data;
    return (uint32_t)u[0] | ((uint32_t)u[1] << 8) | ((uint32_t)u[2] << 16) | ((uint32_t)u[3] << 24);
}

uint16_t readLE16(const char* data) {
    const unsigned char* u = (const unsigned char*)data;
    return (uint16_t)u[0] | ((uint16_t)u[1] << 8);
}

string get_filename_without_ext(string path) {
    size_t last_slash = path.find_last_of("\\/");
    string filename = (last_slash == string::npos) ? path : path.substr(last_slash + 1);
    size_t last_dot = filename.find_last_of(".");
    if (last_dot == string::npos) return filename;
    return filename.substr(0, last_dot);
}

// Extractor Basico de Muestras FSB4 (Seguro)
void extract_fsb_samples_safe(const char* fsbData, size_t fsbSize, string outDir) {
    if (fsbSize < 24) return;
    uint32_t numSamples = readLE32(fsbData + 4);
    uint32_t shdrSize = readLE32(fsbData + 8);
    uint32_t dataSize = readLE32(fsbData + 12);

    if (numSamples == 0 || numSamples > 2000) return;

    MKDIR(outDir.c_str());

    const char* shdrPtr = fsbData + 24;
    const char* dataPtr = fsbData + 24 + shdrSize;
    uint32_t currentOffset = 0;

    for (uint32_t i = 0; i < numSamples; ++i) {
        if (shdrPtr + 8 > fsbData + fsbSize) break;
        uint16_t entrySize = readLE16(shdrPtr);
        if (entrySize < 32 || shdrPtr + entrySize > fsbData + fsbSize) break;

        string name = "sample_" + to_string(i);
        // Buscar nombre en el entry (tipico en FSB4)
        for (uint32_t j = 20; j < (uint32_t)entrySize - 4; ++j) {
            if (shdrPtr[j] >= 32 && shdrPtr[j] <= 126 && shdrPtr[j+1] >= 32 && shdrPtr[j+1] <= 126) {
                name = string(shdrPtr + j);
                break;
            }
        }

        uint32_t sampleLen = readLE32(shdrPtr + 8);

        if (sampleLen > 0 && currentOffset + sampleLen <= dataSize && dataPtr + currentOffset + sampleLen <= fsbData + fsbSize) {
            ofstream sOut(outDir + "/" + name + ".dat", ios::binary);
            sOut.write(dataPtr + currentOffset, sampleLen);
            sOut.close();
            cout << "      > Sonido: " << name << ".dat" << endl;
        }
        currentOffset += sampleLen;
        shdrPtr += entrySize;
    }
}

void extract(string filename) {
    ifstream f(filename, ios::binary);
    if (!f) return;
    unsigned char tag[4];
    f.read((char*)tag, 4);
    if (tag[0] != 0x9E) { cout << "Error: No es un archivo .XXX de PS3." << endl; return; }

    f.seekg(8);
    uint32_t headerSize;
    f.read((char*)&headerSize, 4);
    headerSize = swap32(headerSize);

    string outDir = get_filename_without_ext(filename) + "_extracted";
    MKDIR(outDir.c_str());
    cout << "Directorio de salida: " << outDir << endl;

    f.seekg(0);
    vector<char> hData(headerSize);
    f.read(hData.data(), headerSize);
    ofstream hOut(outDir + "/header.bin", ios::binary);
    hOut.write(hData.data(), headerSize);

    f.seekg(0, ios::end);
    size_t fileSize = f.tellg();
    if (fileSize < headerSize) return;
    size_t dataSize = fileSize - headerSize;

    f.seekg(headerSize);
    vector<char> data(dataSize);
    f.read(data.data(), dataSize);
    ofstream dOut(outDir + "/data.bin", ios::binary);
    dOut.write(data.data(), dataSize);

    cout << "  - Archivos base extraidos." << endl;

    int audioCount = 0;
    for (size_t i = 0; i <= (data.size() >= 24 ? data.size() - 24 : 0); ++i) {
        if (memcmp(&data[i], "FSB4", 4) == 0) {
            uint32_t shdr = readLE32(&data[i + 8]);
            uint32_t datSize = readLE32(&data[i + 12]);
            size_t expectedTotal = 24 + shdr + datSize;

            bool truncated = false;
            size_t available = data.size() - i;
            size_t toWrite = expectedTotal;

            if (expectedTotal > available) {
                truncated = true;
                toWrite = available;
                cout << "  - AVISO: El FSB_" << audioCount << " parece estar truncado en este archivo (Streaming Bank)." << endl;
                cout << "    Se rellenara con ceros para que sea compatible con otras herramientas." << endl;
            }

            string fsbPath = outDir + "/audio_" + to_string(audioCount) + ".fsb";
            vector<char> fsbFullData(&data[i], &data[i] + toWrite);
            if (truncated) fsbFullData.resize(expectedTotal, 0);

            ofstream fOut(fsbPath, ios::binary);
            fOut.write(fsbFullData.data(), fsbFullData.size());
            fOut.close();

            cout << "  - FSB Encontrado: audio_" << audioCount << ".fsb (Offset: 0x" << hex << headerSize + i << dec << ")" << endl;
            extract_fsb_samples_safe(fsbFullData.data(), fsbFullData.size(), outDir + "/audio_" + to_string(audioCount) + "_samples");
            audioCount++;
        }
    }
    cout << "\nLISTO: Extraccion completa." << endl;
}

void inject(string targetFile, string sourceFile, string offsetStr) {
    uint32_t offset = (uint32_t)stoul(offsetStr, nullptr, 16);
    ifstream sIn(sourceFile, ios::binary | ios::ate);
    if (!sIn) return;
    size_t sz = sIn.tellg();
    sIn.seekg(0);
    vector<char> buf(sz);
    sIn.read(buf.data(), sz);

    fstream tOut(targetFile, ios::binary | ios::in | ios::out);
    tOut.seekp(offset);
    tOut.write(buf.data(), sz);
    cout << "Inyectado correctamente en 0x" << hex << offset << dec << endl;
}

void patch_sample(string xxxFile, string soundName, string newSampleFile) {
    fstream f(xxxFile, ios::binary | ios::in | ios::out);
    if (!f) return;
    f.seekg(8); uint32_t hSize; f.read((char*)&hSize, 4); hSize = swap32(hSize);
    f.seekg(0, ios::end); size_t fSize = f.tellg();
    f.seekg(hSize); vector<char> data(fSize - hSize); f.read(data.data(), data.size());

    ifstream nIn(newSampleFile, ios::binary | ios::ate);
    if (!nIn) return;
    size_t newLen = nIn.tellg(); nIn.seekg(0); vector<char> nData(newLen); nIn.read(nData.data(), newLen);

    bool found = false;
    for (size_t i = 0; i <= (data.size() >= 24 ? data.size() - 24 : 0); ++i) {
        if (memcmp(&data[i], "FSB4", 4) == 0) {
            uint32_t numSamples = readLE32(&data[i + 4]);
            uint32_t shdrSize = readLE32(&data[i + 8]);
            const char* shdrPtr = &data[i + 24];
            uint32_t currentSampleOffset = 0;

            for (uint32_t j = 0; j < numSamples; ++j) {
                uint16_t entrySize = readLE16(shdrPtr);
                string name = "";
                for (uint32_t k = 20; k < (uint32_t)entrySize - 4; ++k) {
                    if (shdrPtr[k] >= 32 && shdrPtr[k] <= 126 && shdrPtr[k+1] >= 32 && shdrPtr[k+1] <= 126) {
                        name = string(shdrPtr + k); break;
                    }
                }
                uint32_t sampleLen = readLE32(shdrPtr + 8);
                if (name == soundName) {
                    size_t absOffset = hSize + i + 24 + shdrSize + currentSampleOffset;
                    f.seekp(absOffset); f.write(nData.data(), min((size_t)sampleLen, newLen));
                    cout << "Parcheado '" << name << "' en 0x" << hex << absOffset << dec << endl;
                    found = true; break;
                }
                currentSampleOffset += sampleLen;
                shdrPtr += entrySize;
            }
        }
        if (found) break;
    }
}

void pack(string h, string d, string o) {
    ifstream hi(h, ios::binary), di(d, ios::binary);
    ofstream out(o, ios::binary);
    out << hi.rdbuf() << di.rdbuf();
    cout << "Creado: " << o << endl;
}

int main(int argc, char** argv) {
    cout << "======================================" << endl;
    cout << "   MK9 PS3 XXX Tool - Modding PRO v5  " << endl;
    cout << "======================================" << endl;

    if (argc < 2) {
        cout << "COMANDOS:" << endl;
        cout << "  1. Extraer:     MK9Tool.exe <archivo.xxx>" << endl;
        cout << "  2. Inyectar FSB: MK9Tool.exe inject <archivo.xxx> <nuevo.fsb> <offset_hex>" << endl;
        cout << "  3. Parchar Aud:  MK9Tool.exe patch <archivo.xxx> <nombre_sonido> <nuevo.dat>" << endl;
        cout << "  4. Reconstruir: MK9Tool.exe <header.bin> <data.bin> <salida.xxx>" << endl;
        cout << "\nNOTA SOBRE AUDIOS TRUNCADOS (STREAMING):" << endl;
        cout << "  Si el FSB da error al abrir, es porque los datos de audio estan en otro archivo" << endl;
        cout << "  del juego (.TFC o .AUD). Esta herramienta rellenara con ceros para que puedas" << endl;
        cout << "  al menos ver los nombres de los sonidos en el FSB Extractor." << endl;
        return 0;
    }

    string cmd = argv[1];
    if (cmd == "inject" && argc == 5) inject(argv[2], argv[3], argv[4]);
    else if (cmd == "patch" && argc == 5) patch_sample(argv[2], argv[3], argv[4]);
    else if (argc == 2) extract(argv[1]);
    else if (argc == 4) pack(argv[1], argv[2], argv[3]);
    return 0;
}
