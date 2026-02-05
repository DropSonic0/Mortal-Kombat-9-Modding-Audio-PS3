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
// Herramienta optimizada para extraer .XXX y sus audios .FSB internos.

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

// Extractor Basico de Muestras FSB4
void extract_fsb_samples_basic(const char* fsbData, size_t fsbSize, string outDir) {
    if (fsbSize < 24) return;
    uint32_t numSamples = readLE32(fsbData + 4);
    uint32_t shdrSize = readLE32(fsbData + 8);
    uint32_t dataSize = readLE32(fsbData + 12);

    if (numSamples == 0 || numSamples > 1000) return;

    MKDIR(outDir.c_str());
    uint32_t entrySize = shdrSize / numSamples;
    if (entrySize < 32) return;

    const char* shdrPtr = fsbData + 24;
    const char* dataPtr = fsbData + 24 + shdrSize;
    uint32_t currentOffset = 0;

    for (uint32_t i = 0; i < numSamples; ++i) {
        const char* entry = shdrPtr + (i * entrySize);
        // Intentar buscar el nombre en el entry (suele estar al final o despues de los primeros 20 bytes)
        string name = "sample_" + to_string(i);
        for (uint32_t j = 20; j < entrySize - 4; ++j) {
            if (entry[j] >= 32 && entry[j] <= 126 && entry[j+1] >= 32 && entry[j+1] <= 126) {
                name = string(entry + j);
                break;
            }
        }

        uint32_t sampleLen = readLE32(entry + 8); // Estimacion tipica de offset de tamano
        // Nota: En MK9 esto puede variar, por lo que extraemos pedazos basados en el promedio si falla
        if (sampleLen == 0 || sampleLen > dataSize) sampleLen = dataSize / numSamples;

        if (currentOffset + sampleLen <= dataSize) {
            ofstream sOut(outDir + "/" + name + ".dat", ios::binary);
            sOut.write(dataPtr + currentOffset, sampleLen);
            sOut.close();
            cout << "      > Extraido: " << name << endl;
        }
        currentOffset += sampleLen;
    }
}

void extract(string filename) {
    ifstream f(filename, ios::binary);
    if (!f) return;

    unsigned char tag[4];
    f.read((char*)tag, 4);
    if (tag[0] != 0x9E) return;

    f.seekg(8);
    uint32_t headerSize;
    f.read((char*)&headerSize, 4);
    headerSize = swap32(headerSize);

    string outDir = get_filename_without_ext(filename) + "_extracted";
    MKDIR(outDir.c_str());
    cout << "Carpeta de salida: " << outDir << endl;

    f.seekg(0);
    vector<char> hData(headerSize);
    f.read(hData.data(), headerSize);
    ofstream hOut(outDir + "/header.bin", ios::binary);
    hOut.write(hData.data(), headerSize);

    f.seekg(0, ios::end);
    size_t dataSize = (size_t)f.tellg() - headerSize;
    f.seekg(headerSize);
    vector<char> data(dataSize);
    f.read(data.data(), dataSize);
    ofstream dOut(outDir + "/data.bin", ios::binary);
    dOut.write(data.data(), dataSize);

    cout << "  - Extraidos archivos base." << endl;

    int audioCount = 0;
    for (size_t i = 0; i <= (data.size() >= 24 ? data.size() - 24 : 0); ++i) {
        if (memcmp(&data[i], "FSB4", 4) == 0) {
            uint32_t shdr = readLE32(&data[i + 8]);
            uint32_t dat = readLE32(&data[i + 12]);
            size_t total = 24 + shdr + dat;
            if (i + total > data.size()) total = data.size() - i;

            string fsbPath = outDir + "/audio_" + to_string(audioCount) + ".fsb";
            ofstream fOut(fsbPath, ios::binary);
            fOut.write(&data[i], total);
            fOut.close();

            cout << "  - FSB Encontrado: " << fsbPath << ". Extrayendo muestras internas..." << endl;
            extract_fsb_samples_basic(&data[i], total, outDir + "/audio_" + to_string(audioCount) + "_samples");
            audioCount++;
        }
    }
    cout << "\nLISTO: Todo se extrajo en '" << outDir << "'" << endl;
}

void replace(string dataFile, string newFile, string offsetStr) {
    uint32_t offset = (uint32_t)stoul(offsetStr, nullptr, 16);
    ifstream nIn(newFile, ios::binary | ios::ate);
    if (!nIn) return;
    size_t sz = nIn.tellg();
    nIn.seekg(0);
    vector<char> buf(sz);
    nIn.read(buf.data(), sz);
    fstream dOut(dataFile, ios::binary | ios::in | ios::out);
    dOut.seekp(offset);
    dOut.write(buf.data(), sz);
    cout << "Inyectado correctamente." << endl;
}

void pack(string h, string d, string o) {
    ifstream hi(h, ios::binary), di(d, ios::binary);
    ofstream out(o, ios::binary);
    out << hi.rdbuf() << di.rdbuf();
    cout << "Archivo .XXX creado." << endl;
}

int main(int argc, char** argv) {
    cout << "MK9 PS3 XXX Tool - Modding PRO" << endl;
    if (argc < 2) {
        cout << "1. Extraer: MK9Tool <file.xxx>\n2. Inyectar: MK9Tool replace <data> <new> <offset>\n3. Crear: MK9Tool <h> <d> <out.xxx>" << endl;
        return 0;
    }
    if (string(argv[1]) == "replace" && argc == 5) replace(argv[2], argv[3], argv[4]);
    else if (argc == 2) extract(argv[1]);
    else if (argc == 4) pack(argv[1], argv[2], argv[3]);
    return 0;
}
