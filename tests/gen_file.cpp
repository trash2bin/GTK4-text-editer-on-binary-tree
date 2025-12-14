#include <iostream>
#include <fstream>
#include <random>
#include <string>
#include <vector>

// Генератор случайной строки UTF-8
std::string generateRandomLine(std::mt19937& rng, int minLen = 20, int maxLen = 80) {
    std::string ascii = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789 ,.!?;:";
    
    // Кириллица: диапазон А-я (русские буквы)
    std::vector<char32_t> cyrillic;
    for (char32_t c = 0x0410; c <= 0x044F; ++c) cyrillic.push_back(c);

    std::uniform_int_distribution<int> lenDist(minLen, maxLen);
    std::uniform_int_distribution<int> asciiDist(0, ascii.size() - 1);
    std::uniform_int_distribution<int> cyrDist(0, cyrillic.size() - 1);
    std::uniform_int_distribution<int> choiceDist(0, 1); // ASCII или кириллица

    int len = lenDist(rng);
    std::string line;
    for (int i = 0; i < len; ++i) {
        if (choiceDist(rng) == 0) {
            line += ascii[asciiDist(rng)];
        } else {
            // Кириллица в UTF-8
            char32_t c = cyrDist(rng) + 0x410;
            if (c <= 0x7F) {
                line += static_cast<char>(c);
            } else if (c <= 0x7FF) {
                line += static_cast<char>(0xC0 | ((c >> 6) & 0x1F));
                line += static_cast<char>(0x80 | (c & 0x3F));
            } else {
                line += static_cast<char>(0xE0 | ((c >> 12) & 0x0F));
                line += static_cast<char>(0x80 | ((c >> 6) & 0x3F));
                line += static_cast<char>(0x80 | (c & 0x3F));
            }
        }
    }
    line += '\n';
    return line;
}

int main() {
    std::string filename = "testFile.txt";
    std::size_t targetSize;

    std::cout << "Enter target file size in bytes: ";
    std::cin >> targetSize;

    std::ofstream out(filename, std::ios::binary);
    if (!out) {
        std::cerr << "Cannot open file for writing\n";
        return 1;
    }

    std::random_device rd;
    std::mt19937 rng(rd());

    std::size_t written = 0;
    while (written < targetSize) {
        std::string line = generateRandomLine(rng);
        if (written + line.size() > targetSize) {
            line.resize(targetSize - written); // обрезаем последнюю строку
        }
        out.write(line.data(), line.size());
        written += line.size();
    }

    std::cout << "Generated UTF-8 file '" << filename << "' of size " << written << " bytes\n";
    return 0;
}
