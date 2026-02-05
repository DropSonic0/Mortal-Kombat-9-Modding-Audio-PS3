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

// MK9 PS3 XXX Tool - Modding Tool PRO
// Herramienta completa para extraer, inyectar y reconstruir archivos de MK9 PS3.

uint32_t swap32(uint32_t v) {
    return ((v >> 24) & 0xff) | ((v >> 8) & 0xff00) | ((v << 8) & 0xff0000) | ((v << 24) & 0xff000000);
}

uint32_t readLE32(const char* data) {
    const unsigned char* u = (const unsigned char*)data;
    return (uint32_t)u[0] | ((uint32_t)u[1] << 8) | ((uint32_t)u[2] << 16) | ((uint32_t)u[3] << 24);
}

string get_filename_without_ext(string path) {
    size_t last_slash = path.find_last_of("\\/");
    string filename = (last_slash == string::npos) ? path : path.substr(last_slash + 1);
    size_t last_dot = filename.find_last_of(".");
    if (last_dot == string::npos) return filename;
    return filename.substr(0, last_dot);
}

void extract_fsb_samples(const char* fsbData, size_t fsbSize, string outDir) {
    if (fsbSize < 24) return;
    uint32_t numSamples = readLE32(fsbData + 4);
    uint32_t shdrSize = readLE32(fsbData + 8);
    uint32_t dataSize = readLE32(fsbData + 12);
    if (numSamples == 0 || numSamples > 2000) return;

    MKDIR(outDir.c_str());
    uint32_t entrySize = shdrSize / numSamples;
    const char* shdrPtr = fsbData + 24;
    const char* dataPtr = fsbData + 24 + shdrSize;
    uint32_t currentOffset = 0;

    for (uint32_t i = 0; i < numSamples; ++i) {
        const char* entry = shdrPtr + (i * entrySize);
        string name = "sample_" + to_string(i);
        for (uint32_t j = 20; j < entrySize - 4; ++j) {
            if (entry[j] >= 32 && entry[j] <= 126 && entry[j+1] >= 32 && entry[j+1] <= 126) {
                name = string(entry + j);
                break;
            }
        }
        uint32_t sampleLen = readLE32(entry + 8);
        if (sampleLen == 0 || sampleLen > dataSize) sampleLen = dataSize / numSamples;

        if (currentOffset + sampleLen <= dataSize) {
            ofstream sOut(outDir + "/" + name + ".dat", ios::binary);
            sOut.write(dataPtr + currentOffset, sampleLen);
            sOut.close();
            cout << "      > Extraido: " << name << ".dat" << endl;
        }
        currentOffset += sampleLen;
    }
}

void extract(string filename) {
    ifstream f(filename, ios::binary);
    if (!f) { cout << "Error abriendo " << filename << endl; return; }
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
    f.seekg(headerSize);
    size_t dataSize = fileSize - headerSize;
    vector<char> data(dataSize);
    f.read(data.data(), dataSize);
    ofstream dOut(outDir + "/data.bin", ios::binary);
    dOut.write(data.data(), dataSize);

    cout << "  - Extraidos archivos .bin (header y data)." << endl;

    int audioCount = 0;
    for (size_t i = 0; i <= (data.size() >= 24 ? data.size() - 24 : 0); ++i) {
        if (memcmp(&data[i], "FSB4", 4) == 0) {
            uint32_t shdr = readLE32(&data[i + 8]);
            uint32_t datSize = readLE32(&data[i + 12]);
            size_t total = 24 + shdr + datSize;
            if (i + total > data.size()) total = data.size() - i;

            string fsbName = "audio_" + to_string(audioCount) + ".fsb";
            string fsbPath = outDir + "/" + fsbName;

            ofstream fOut(fsbPath, ios::binary);
            fOut.write(&data[i], total);
            fOut.close();

            size_t absOffset = headerSize + i;
            cout << "  - FSB Encontrado: " << fsbName << endl;
            cout << "    Offset en .XXX: 0x" << hex << absOffset << dec << " (Usa esto para inyectar un nuevo .fsb)" << endl;

            extract_fsb_samples(&data[i], total, outDir + "/audio_" + to_string(audioCount) + "_samples");
            audioCount++;
        }
    }
    cout << "\nLISTO: Extraccion completa." << endl;
}

void inject(string targetFile, string sourceFile, string offsetStr) {
    uint32_t offset = (uint32_t)stoul(offsetStr, nullptr, 16);
    ifstream sIn(sourceFile, ios::binary | ios::ate);
    if (!sIn) { cout << "Error: No se pudo abrir " << sourceFile << endl; return; }
    size_t sz = sIn.tellg();
    sIn.seekg(0);
    vector<char> buf(sz);
    sIn.read(buf.data(), sz);

    fstream tOut(targetFile, ios::binary | ios::in | ios::out);
    if (!tOut) { cout << "Error: No se pudo abrir " << targetFile << endl; return; }
    tOut.seekp(offset);
    tOut.write(buf.data(), sz);
    cout << "Inyectado: " << sz << " bytes en " << targetFile << " (Offset: 0x" << hex << offset << dec << ")" << endl;
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
            uint32_t entrySize = shdrSize / numSamples;
            const char* shdrPtr = &data[i + 24];
            uint32_t currentSampleOffset = 0;

            for (uint32_t j = 0; j < numSamples; ++j) {
                const char* entry = shdrPtr + (j * entrySize);
                string name = "";
                for (uint32_t k = 20; k < entrySize - 4; ++k) {
                    if (entry[k] >= 32 && entry[k] <= 126 && entry[k+1] >= 32 && entry[k+1] <= 126) {
                        name = string(entry + k); break;
                    }
                }
                uint32_t sampleLen = readLE32(entry + 8);
                if (name == soundName) {
                    size_t absOffset = hSize + i + 24 + shdrSize + currentSampleOffset;
                    f.seekp(absOffset); f.write(nData.data(), min((size_t)sampleLen, newLen));
                    cout << "Muestra '" << name << "' parcheada en offset 0x" << hex << absOffset << dec << endl;
                    found = true; break;
                }
                currentSampleOffset += sampleLen;
            }
        }
        if (found) break;
    }
}

void pack(string h, string d, string o) {
    ifstream hi(h, ios::binary), di(d, ios::binary);
    ofstream out(o, ios::binary);
    out << hi.rdbuf() << di.rdbuf();
    cout << "Archivo .XXX creado: " << o << endl;
}

int main(int argc, char** argv) {
    cout << "======================================" << endl;
    cout << "   MK9 PS3 XXX Tool - Modding PRO     " << endl;
    cout << "======================================" << endl;

    if (argc < 2) {
        cout << "COMANDOS:" << endl;
        cout << "  1. Extraer:     MK9Tool.exe <archivo.xxx>" << endl;
        cout << "  2. Inyectar FSB: MK9Tool.exe inject <archivo.xxx> <nuevo_fsb.fsb> <offset_hex>" << endl;
        cout << "  3. Parchar Aud:  MK9Tool.exe patch <archivo.xxx> <nombre_sonido> <nuevo.dat>" << endl;
        cout << "  4. Reconstruir: MK9Tool.exe <header.bin> <data.bin> <salida.xxx>" << endl;
        cout << "\nCOMO UNIR UN .FSB A UN .XXX:" << endl;
        cout << "  - Paso 1: Extrae el .xxx original para ver los offsets de los FSB." << endl;
        cout << "  - Paso 2: Modifica tu .fsb (usa FMOD Designer)." << endl;
        cout << "  - Paso 3: Usa el comando 'inject' con el offset que te dio la extraccion." << endl;
        return 0;
    }

    string cmd = argv[1];
    if (cmd == "inject" && argc == 5) inject(argv[2], argv[3], argv[4]);
    else if (cmd == "patch" && argc == 5) patch_sample(argv[2], argv[3], argv[4]);
    else if (argc == 2) extract(argv[1]);
    else if (argc == 4) pack(argv[1], argv[2], argv[3]);
    return 0;
}
