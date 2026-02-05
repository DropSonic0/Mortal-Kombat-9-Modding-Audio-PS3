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

// MK9 PS3 XXX Tool - Modding Tool
// Herramienta prolija para extraer, reemplazar y empaquetar archivos .XXX de MK9 PS3.

uint32_t swap32(uint32_t v) {
    return ((v >> 24) & 0xff) | ((v >> 8) & 0xff00) | ((v << 8) & 0xff0000) | ((v << 24) & 0xff000000);
}

// Lee 32 bits en Little Endian desde un buffer de bytes
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

void extract(string filename) {
    ifstream f(filename, ios::binary);
    if (!f) {
        cout << "Error: No se pudo abrir " << filename << endl;
        return;
    }

    unsigned char tag[4];
    if (!f.read((char*)tag, 4)) return;
    if (tag[0] != 0x9E || tag[1] != 0x2A || tag[2] != 0x83 || tag[3] != 0xC1) {
        cout << "Error: No es un archivo .XXX de PS3 valido (Tag incorrecto)." << endl;
        return;
    }

    f.seekg(8);
    uint32_t headerSize;
    f.read((char*)&headerSize, 4);
    headerSize = swap32(headerSize);

    cout << "Detectado paquete Big Endian (PS3)." << endl;

    string outDir = get_filename_without_ext(filename) + "_extracted";
    MKDIR(outDir.c_str());
    cout << "Extrayendo en la carpeta: " << outDir << endl;

    // Extraer header
    f.seekg(0);
    vector<char> headerData(headerSize);
    f.read(headerData.data(), headerSize);

    string hName = outDir + "/header.bin";
    ofstream hOut(hName, ios::binary);
    hOut.write(headerData.data(), headerSize);
    hOut.close();

    // Extraer data
    f.seekg(0, ios::end);
    size_t fileSize = f.tellg();
    if (fileSize < headerSize) {
        cout << "Error: El archivo es mas pequeno que el header." << endl;
        return;
    }
    size_t dataSize = fileSize - headerSize;

    if (dataSize > 0) {
        f.seekg(headerSize);
        vector<char> data(dataSize);
        f.read(data.data(), dataSize);

        string dName = outDir + "/data.bin";
        ofstream dOut(dName, ios::binary);
        dOut.write(data.data(), dataSize);
        dOut.close();

        cout << "  - Creado: " << hName << endl;
        cout << "  - Creado: " << dName << endl;

        // Escaneo de audios FSB4
        int audioCount = 0;
        if (data.size() >= 4) {
            for (size_t i = 0; i <= data.size() - 4; ++i) {
                if (memcmp(&data[i], "FSB4", 4) == 0) {
                    string audioName = outDir + "/audio_" + to_string(audioCount++) + ".fsb";

                    // Metadata de FSB4 (siempre Little Endian)
                    // Offset 8: shdr_size, Offset 12: data_size
                    uint32_t shdr_size = (i + 12 <= data.size()) ? readLE32(&data[i + 8]) : 0;
                    uint32_t data_size = (i + 16 <= data.size()) ? readLE32(&data[i + 12]) : 0;
                    size_t fsbTotalSize = 24 + shdr_size + data_size;

                    if (i + fsbTotalSize > data.size()) fsbTotalSize = data.size() - i;

                    cout << "  - Audio encontrado: " << audioName << " (Offset en data.bin: 0x" << hex << i << dec << ")" << endl;

                    ofstream aOut(audioName, ios::binary);
                    aOut.write(&data[i], fsbTotalSize);
                    aOut.close();
                }
            }
        }
        cout << "\nLISTO: Los archivos estan en la carpeta '" << outDir << "'" << endl;
        cout << "Para modificar el audio: Modifica el archivo .fsb y usa 'replace' con data.bin" << endl;
    } else {
        cout << "El archivo no contiene datos adicionales fuera del header." << endl;
    }
}

void replace(string dataFile, string newFile, string offsetStr) {
    uint32_t offset = 0;
    try {
        offset = (uint32_t)stoul(offsetStr, nullptr, 16);
    } catch (...) {
        cout << "Error: Offset invalido." << endl;
        return;
    }

    ifstream nIn(newFile, ios::binary | ios::ate);
    if (!nIn) { cout << "Error: No se pudo abrir el nuevo archivo: " << newFile << endl; return; }
    size_t newSize = nIn.tellg();
    nIn.seekg(0);
    vector<char> newData(newSize);
    nIn.read(newData.data(), newSize);
    nIn.close();

    fstream dOut(dataFile, ios::binary | ios::in | ios::out);
    if (!dOut) { cout << "Error: No se pudo abrir el archivo: " << dataFile << endl; return; }

    dOut.seekp(offset);
    dOut.write(newData.data(), newSize);
    dOut.close();

    cout << "Inyectado: " << newSize << " bytes en " << dataFile << " (Offset: 0x" << hex << offset << dec << ")" << endl;
}

void pack(string headerFile, string dataFile, string outFile) {
    ifstream hIn(headerFile, ios::binary);
    ifstream dIn(dataFile, ios::binary);

    if (!hIn) { cout << "Error: No se pudo abrir el header: " << headerFile << endl; return; }
    if (!dIn) { cout << "Error: No se pudo abrir el data: " << dataFile << endl; return; }

    ofstream out(outFile, ios::binary);
    out << hIn.rdbuf();
    out << dIn.rdbuf();

    cout << "Archivo .XXX creado: " << outFile << endl;
}

int main(int argc, char** argv) {
    cout << "======================================" << endl;
    cout << "   MK9 PS3 XXX Tool - Modding Tool    " << endl;
    cout << "======================================" << endl;

    if (argc < 2) {
        cout << "Uso:" << endl;
        cout << "  1. Extraer: MK9Tool.exe <archivo.xxx>" << endl;
        cout << "  2. Reemplazar: MK9Tool.exe replace <data.bin> <nuevo.fsb> <offset>" << endl;
        cout << "  3. Empaquetar: MK9Tool.exe <header.bin> <data.bin> <salida.xxx>" << endl;
        return 0;
    }

    string cmd = argv[1];
    if (cmd == "replace" && argc == 5) {
        replace(argv[2], argv[3], argv[4]);
    } else if (argc == 2) {
        extract(argv[1]);
    } else if (argc == 4) {
        pack(argv[1], argv[2], argv[3]);
    } else {
        cout << "Comando no reconocido o faltan parametros." << endl;
    }

    return 0;
}
