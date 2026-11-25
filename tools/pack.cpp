// Comprime un archivo con la API de compresión de Windows para incrustarlo como recurso.
// Formato de salida: 1 byte con el algoritmo (COMPRESS_ALGORITHM_*) y el bloque comprimido en modo búfer,
// que ya guarda el tamaño original. Se prueban XPRESS-Huffman y LZMS y se queda el más chico.
// Uso: wram_pack <entrada> <salida>
#include <windows.h>
#include <compressapi.h>

#include <cstdio>
#include <fstream>
#include <iterator>
#include <vector>

static std::vector<unsigned char> Pack(const std::vector<unsigned char>& in, DWORD algorithm) {
    COMPRESSOR_HANDLE c = nullptr;
    if (!CreateCompressor(algorithm, nullptr, &c)) return {};
    SIZE_T needed = 0;
    Compress(c, in.data(), in.size(), nullptr, 0, &needed);
    std::vector<unsigned char> out(needed + 1);
    SIZE_T written = 0;
    const BOOL ok = Compress(c, in.data(), in.size(), out.data() + 1, needed, &written);
    CloseCompressor(c);
    if (!ok) return {};
    out.resize(written + 1);
    out[0] = static_cast<unsigned char>(algorithm);
    return out;
}

int main(int argc, char** argv) {
    if (argc != 3) {
        std::fprintf(stderr, "uso: wram_pack <entrada> <salida>\n");
        return 2;
    }
    std::ifstream file(argv[1], std::ios::binary);
    const std::vector<unsigned char> in((std::istreambuf_iterator<char>(file)), std::istreambuf_iterator<char>());
    if (in.empty()) return 1;
    std::vector<unsigned char> best;
    for (const DWORD algorithm : {COMPRESS_ALGORITHM_XPRESS_HUFF, COMPRESS_ALGORITHM_LZMS}) {
        std::vector<unsigned char> candidate = Pack(in, algorithm);
        if (!candidate.empty() && (best.empty() || candidate.size() < best.size())) best = std::move(candidate);
    }
    if (best.empty()) return 1;
    std::ofstream out(argv[2], std::ios::binary | std::ios::trunc);
    out.write(reinterpret_cast<const char*>(best.data()), static_cast<std::streamsize>(best.size()));
    std::printf("%s: %zu -> %zu bytes (algoritmo %u)\n", argv[1], in.size(), best.size(), static_cast<unsigned>(best[0]));
    return out ? 0 : 1;
}
