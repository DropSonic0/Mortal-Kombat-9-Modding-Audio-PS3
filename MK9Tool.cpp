#include <iostream>
#include <vector>
#include <fstream>
#include <string>
#include <stdint.h>
#include <algorithm>
#include <iomanip>
#include <cstring>

using namespace std;

// MK9 PS3 XXX Tool - Modding Tool
// Esta herramienta permite extraer, reemplazar y empaquetar archivos .XXX de MK9 PS3.

uint32_t swap32(uint32_t v) {
    return ((v >> 24) & 0xff) | ((v >> 8) & 0xff00) | ((v << 8) & 0xff0000) | ((v << 24) & 0xff000000);
}

void extract(string filename) {
    ifstream f(filename, ios::binary);
    if (!f) {
        cout << "Error: No se pudo abrir " << filename << endl;
        return;
    }

    unsigned char tag[4];
    f.read((char*)tag, 4);
    if (tag[0] != 0x9E || tag[1] != 0x2A || tag[2] != 0x83 || tag[3] != 0xC1) {
        cout << "Error: No es un archivo .XXX de PS3 valido (Tag incorrecto)." << endl;
        return;
    }

    f.seekg(8);
    uint32_t headerSize;
    f.read((char*)&headerSize, 4);
    headerSize = swap32(headerSize);

    cout << "Detectado paquete Big Endian (PS3)." << endl;
    cout << "Tamano del Header: " << headerSize << " bytes (0x" << hex << headerSize << dec << ")." << endl;

    // Extraer header
    f.seekg(0);
    vector<char> headerData(headerSize);
    f.read(headerData.data(), headerSize);

    string hName = filename + ".header";
    ofstream hOut(hName, ios::binary);
    hOut.write(headerData.data(), headerSize);
    hOut.close();

    // Extraer data
    f.seekg(0, ios::end);
    size_t fileSize = f.tellg();
    size_t dataSize = fileSize - headerSize;

    if (dataSize > 0) {
        f.seekg(headerSize);
        vector<char> data(dataSize);
        f.read(data.data(), dataSize);

        string dName = filename + ".data";
        ofstream dOut(dName, ios::binary);
        dOut.write(data.data(), dataSize);
        dOut.close();
        cout << "Archivos extraidos: " << hName << " y " << dName << endl;

        // Escaneo de audios FSB4
        int audioCount = 0;
        for (size_t i = 0; i < data.size() - 4; ++i) {
            if (memcmp(&data[i], "FSB4", 4) == 0) {
                string audioName = filename + "_audio_" + to_string(audioCount++) + ".fsb";

                // Calculo de tamano (Little Endian en FSB4)
                uint32_t sampleHdrSize = *(uint32_t*)&data[i + 8];
                uint32_t audioDataSize = *(uint32_t*)&data[i + 12];
                size_t fsbTotalSize = 24 + sampleHdrSize + audioDataSize;

                if (i + fsbTotalSize > data.size()) fsbTotalSize = data.size() - i;

                cout << "  -> Audio encontrado: " << audioName << " | Offset en .data: 0x" << hex << i << dec << " | Tamano: " << fsbTotalSize << " bytes." << endl;

                ofstream aOut(audioName, ios::binary);
                aOut.write(&data[i], fsbTotalSize);
                aOut.close();
            }
        }
        cout << "\nPara modificar el audio: Modifica el archivo .fsb y usa el comando 'replace'." << endl;
    } else {
        cout << "El archivo no contiene datos adicionales." << endl;
    }
}

void replace(string dataFile, string newFile, string offsetStr) {
    uint32_t offset = (uint32_t)stoul(offsetStr, nullptr, 16);

    ifstream nIn(newFile, ios::binary | ios::ate);
    if (!nIn) { cout << "Error: No se pudo abrir el nuevo archivo: " << newFile << endl; return; }
    size_t newSize = nIn.tellg();
    nIn.seekg(0);
    vector<char> newData(newSize);
    nIn.read(newData.data(), newSize);
    nIn.close();

    fstream dOut(dataFile, ios::binary | ios::in | ios::out);
    if (!dOut) { cout << "Error: No se pudo abrir el archivo .data: " << dataFile << endl; return; }

    dOut.seekp(offset);
    dOut.write(newData.data(), newSize);
    dOut.close();

    cout << "Exito: Se han inyectado " << newSize << " bytes en " << dataFile << " en el offset 0x" << hex << offset << dec << endl;
    cout << "AVISO: Si el nuevo audio es mas grande que el original, podria causar errores si sobreescribe otros datos." << endl;
}

void pack(string headerFile, string dataFile, string outFile) {
    ifstream hIn(headerFile, ios::binary);
    ifstream dIn(dataFile, ios::binary);

    if (!hIn) { cout << "Error: No se pudo abrir el header: " << headerFile << endl; return; }
    if (!dIn) { cout << "Error: No se pudo abrir el data: " << dataFile << endl; return; }

    ofstream out(outFile, ios::binary);
    out << hIn.rdbuf();
    out << dIn.rdbuf();

    cout << "Archivo .XXX creado exitosamente: " << outFile << endl;
}

int main(int argc, char** argv) {
    cout << "======================================" << endl;
    cout << "   MK9 PS3 XXX Tool - Modding Tool    " << endl;
    cout << "======================================" << endl;

    if (argc < 2) {
        cout << "Uso:" << endl;
        cout << "  1. Extraer: MK9Tool.exe <archivo.xxx>" << endl;
        cout << "  2. Reemplazar: MK9Tool.exe replace <archivo.data> <nuevo_audio.fsb> <offset_en_hex>" << endl;
        cout << "  3. Empaquetar: MK9Tool.exe <header> <data> <nuevo_archivo.xxx>" << endl;
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
