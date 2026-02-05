#include <iostream>
#include <vector>
#include <fstream>
#include <string>
#include <stdint.h>
#include <algorithm>
#include <iomanip>
#include <cstring>

using namespace std;

// MK9 PS3 XXX Tool
// Este programa permite extraer y crear archivos .XXX de Mortal Kombat 9 PS3.
// Desarrollado para ayudar en el modding de PS3.

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
    cout << "Tamano del Header: " << headerSize << " bytes." << endl;

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

        // Escaneo basico de archivos conocidos en el bloque de datos
        int audioCount = 0;
        for (size_t i = 0; i < data.size() - 4; ++i) {
            if (memcmp(&data[i], "FSB4", 4) == 0) {
                string audioName = filename + "_audio_" + to_string(audioCount++) + ".fsb";

                // Intentar leer el tamano real del FSB4
                // Offset 8: tamano de headers de samples (4 bytes)
                // Offset 12: tamano de los datos de audio (4 bytes)
                uint32_t sampleHdrSize = *(uint32_t*)&data[i + 8];
                uint32_t audioDataSize = *(uint32_t*)&data[i + 12];
                // Nota: Los FSB4 en MK9 PS3 suelen ser Little Endian internamente

                size_t fsbTotalSize = 24 + sampleHdrSize + audioDataSize;

                // Verificamos que el tamano sea razonable
                if (i + fsbTotalSize > data.size()) {
                    fsbTotalSize = data.size() - i;
                }

                cout << "  -> Se encontro audio FSB4: " << audioName << " (Offset: 0x" << hex << i << ", Tamaño: 0x" << fsbTotalSize << dec << ")" << endl;

                ofstream aOut(audioName, ios::binary);
                aOut.write(&data[i], fsbTotalSize);
                aOut.close();
            }
        }
    } else {
        cout << "El archivo no contiene datos adicionales fuera del header." << endl;
    }
}

void pack(string headerFile, string dataFile, string outFile) {
    ifstream hIn(headerFile, ios::binary);
    ifstream dIn(dataFile, ios::binary);

    if (!hIn) { cout << "Error: No se pudo abrir el header: " << headerFile << endl; return; }
    if (!dIn) { cout << "Error: No se pudo abrir el data: " << dataFile << endl; return; }

    ofstream out(outFile, ios::binary);
    if (!out) { cout << "Error: No se pudo crear el archivo de salida." << endl; return; }

    out << hIn.rdbuf();
    out << dIn.rdbuf();

    cout << "Archivo .XXX creado exitosamente: " << outFile << endl;
}

int main(int argc, char** argv) {
    cout << "======================================" << endl;
    cout << "   MK9 PS3 XXX Tool - Modding Tool    " << endl;
    cout << "======================================" << endl;

    if (argc < 2) {
        cout << "Uso para extraer:" << endl;
        cout << "  MK9Tool.exe <archivo.xxx>" << endl;
        cout << endl;
        cout << "Uso para crear:" << endl;
        cout << "  MK9Tool.exe <header> <data> <salida.xxx>" << endl;
        return 0;
    }

    if (argc == 2) {
        extract(argv[1]);
    } else if (argc == 4) {
        pack(argv[1], argv[2], argv[3]);
    } else {
        cout << "Parametros incorrectos." << endl;
    }

    return 0;
}
