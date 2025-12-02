#include "BinaryTreeFile.h"
#include <stdexcept>
#include <iostream>
#include <cstring>
#include <fstream> // Добавляем, чтобы использовать std::ofstream

// Предполагается, что Tree.h и BinaryTreeFile.h включают корректные определения
// узлов и класса Tree, как было в предыдущем шаге.

// ==========================================
// Константы
// ==========================================
namespace {
    constexpr char FILE_MAGIC[4] = {'T','R','E','E'}; // NOSONAR
    constexpr std::uint32_t FILE_VERSION = 1;
    constexpr std::int64_t OFFSET_NONE = -1;
}

// ==========================================
// Обработка ошибок
// ==========================================

class BinaryTreeFileError : public std::runtime_error {
public:
    using std::runtime_error::runtime_error; 
};

// ==========================================
// Реализация класса BinaryTreeFile
// ==========================================

BinaryTreeFile::BinaryTreeFile() : std::fstream() {}

BinaryTreeFile::~BinaryTreeFile() {
    if (is_open()) close();
}

bool BinaryTreeFile::openFile(const char* filename) {
    m_filename = filename;
    // Открываем для чтения/записи (если файла нет — создаём)
    open(m_filename.c_str(), std::ios::binary | std::ios::in | std::ios::out);
    if (!is_open()) {
        // Создаем файл, затем снова открываем
        std::ofstream create(m_filename.c_str(), std::ios::binary | std::ios::out);
        if (!create) return false;
        create.close();
        open(m_filename.c_str(), std::ios::binary | std::ios::in | std::ios::out);
    }
    return is_open();
}

// --- Сохранение ---

std::int64_t BinaryTreeFile::writeNodeRecursive(Node* node) {
    if (!node) return OFFSET_NONE;

    // Сначала рекурсивно сохраняем детей (Post-order traversal)
    std::int64_t leftOff = OFFSET_NONE;
    std::int64_t rightOff = OFFSET_NONE;

    if (node->getType() == NodeType::NODE_INTERNAL) {
        auto inner = static_cast<InternalNode*>(node);
        leftOff = writeNodeRecursive(inner->left);
        rightOff = writeNodeRecursive(inner->right);
    }

    // Запоминаем текущую позицию для смещения
    seekp(0, std::ios::end);
    auto currentPos = static_cast<std::int64_t>(tellp());

    // 1. Записываем тип узла
    auto type = static_cast<char>(node->getType());
    write(&type, sizeof(char));
    if (!good()) throw BinaryTreeFileError("I/O error writing node type");

    if (node->getType() == NodeType::NODE_LEAF) {
        // 2. Лист: длина + lineCount + данные
        auto leaf = static_cast<LeafNode*>(node);

        // length (int32)
        write_le_int32(leaf->length);

        // lineCount (int32) -- вот что мы добавляем
        write_le_int32(leaf->lineCount);

        // payload (raw bytes)
        if (leaf->length > 0) {
            write(leaf->data, leaf->length);
            if (!good()) throw BinaryTreeFileError("I/O error writing leaf data");
        }
    } else {
        // Внутренний узел: смещения детей
        write_le_int64(leftOff);
        write_le_int64(rightOff);
    }
    return currentPos;
}

void BinaryTreeFile::saveTree(const Tree& tree) {
    if (m_filename.empty()) {
        throw BinaryTreeFileError("No file opened for saving (filename missing)");
    }

    if (is_open()) close();

    // Открываем файл в режиме truncate для очистки
    open(m_filename.c_str(), std::ios::binary | std::ios::in | std::ios::out | std::ios::trunc);
    if (!is_open()) {
        std::ofstream out(m_filename.c_str(), std::ios::binary | std::ios::trunc);
        if (!out) throw BinaryTreeFileError("Cannot open file for writing");
        out.close();
        open(m_filename.c_str(), std::ios::binary | std::ios::in | std::ios::out);
        if (!is_open()) throw BinaryTreeFileError("Cannot reopen file for writing");
    }

    // Заголовок: magic(4) + version(4) + rootOffset(8) (всего 16 байт)
    seekp(0, std::ios::beg);
    write(FILE_MAGIC, 4);
    write_le_uint32(FILE_VERSION);
    // Плэйсхолдер для смещения корня (8 байт)
    write_le_int64(OFFSET_NONE);

    // Пишем узлы (post-order), получаем смещение корня
    std::int64_t rootOffset = writeNodeRecursive(tree.getRoot());

    // Обновляем реальный rootOffset в заголовок
    seekp(4 + 4, std::ios::beg); // magic(4) + version(4)
    write_le_int64(rootOffset);
    flush();
}


// --- Загрузка ---

Node* BinaryTreeFile::readLeafNodeAt(std::int64_t offset, std::int64_t fileSize) {
    // Проверка: требуется минимум 1 (type) + 4 (length) + 4 (lineCount)
    if (std::int64_t minNeeded = offset + 1 + static_cast<std::int64_t>(sizeof(std::int32_t)) + static_cast<std::int64_t>(sizeof(std::int32_t));
        minNeeded > fileSize) {
        throw BinaryTreeFileError("Corrupt file: not enough bytes for leaf header");
    }

    // В позиции после типа (функция вызывается так, что позиция seekg установлена уже на offset+1)
    // читаем длину листа
    std::int32_t len = read_le_int32();
    if (len < 0) throw BinaryTreeFileError("Corrupt file: negative leaf length");

    // читаем сохранённый lineCount
    std::int32_t lines = read_le_int32();
    if (lines < 0) throw BinaryTreeFileError("Corrupt file: negative leaf lineCount");

    // Проверка, что данные листа влезают в файл
    if (std::int64_t needed = offset + 1 + static_cast<std::int64_t>(sizeof(std::int32_t)) + static_cast<std::int64_t>(sizeof(std::int32_t)) + static_cast<std::int64_t>(len);
        needed > fileSize) {
        throw BinaryTreeFileError("Corrupt file: leaf data exceeds file size");
    }

    char* buf = nullptr;
    if (len > 0) {
        buf = new char[len]; // NOSONAR
        read(buf, static_cast<std::streamsize>(len));
        if (gcount() != static_cast<std::streamsize>(len) || !good()) {
            delete[] buf; //NOSONAR
            throw BinaryTreeFileError("I/O error reading leaf data");
        }
    }

    // Создаём лист — предполагается, что конструктор LeafNode копирует буфер
    LeafNode* leaf = new LeafNode(buf, len); // NOSONAR

    // Устанавливаем явно сохранённый lineCount (перезапишет, если конструктор сам считал)
    leaf->lineCount = lines;

    // Освобождаем временный буфер (если он был скопирован в LeafNode)
    if (buf) { delete[] buf; buf = nullptr; } //NOSONAR

    return leaf;
}


Node* BinaryTreeFile::readInternalNodeAt(std::int64_t offset, std::int64_t fileSize) {
    // Внутренний узел теперь содержит только 1 байт типа + 2 * int64 (смещения детей)
    if (std::int64_t headerNeeded = offset + 1 + static_cast<std::int64_t>(sizeof(std::int64_t)) * 2;
        headerNeeded > fileSize) {
        throw BinaryTreeFileError("Corrupt file: not enough bytes for internal header (offsets)");
    }

    std::int64_t lOff = read_le_int64();
    std::int64_t rOff = read_le_int64();
    // (Удалено: чтение int32 count, т.к. данные вычисляются конструктором)

    // Валидация смещений
    if ((lOff != OFFSET_NONE && (lOff < 0 || lOff >= fileSize)) ||
        (rOff != OFFSET_NONE && (rOff < 0 || rOff >= fileSize))) {
        throw BinaryTreeFileError("Corrupt file: child offset out of bounds");
    }

    // Рекурсивно читаем детей
    Node* l = readNodeRecursive(lOff, fileSize);
    Node* r = readNodeRecursive(rOff, fileSize);

    // InternalNode ctor теперь сам рассчитает totalLength и totalLineCount на основе l и r!
    InternalNode* node = new InternalNode(l, r); // NOSONAR
    // (Удалено: node->lineCount = count; — это больше не нужно!)
    return node;
}

Node* BinaryTreeFile::readNodeRecursive(std::int64_t offset, std::int64_t fileSize) {
    if (offset == OFFSET_NONE) return nullptr;
    if (offset < 0 || offset >= fileSize) {
        throw BinaryTreeFileError("Invalid node offset (out of file bounds)");
    }

    seekg(offset, std::ios::beg);

    char type = 0;
    read(&type, sizeof(type));
    if (gcount() != static_cast<std::streamsize>(sizeof(type)) || !good())
        throw BinaryTreeFileError("I/O error reading node type");

    if (type == static_cast<char>(NodeType::NODE_LEAF)) {
        // Необходимо снова установить указатель чтения, так как read() в readLeafNodeAt будет брать данные
        // Смещение узла (offset) указывает на начало типа. Мы уже прочитали тип.
        // Чтобы начать чтение данных листа, нужно вернуться на позицию после типа.
        seekg(offset + 1, std::ios::beg);
        return readLeafNodeAt(offset, fileSize);
    } else if (type == static_cast<char>(NodeType::NODE_INTERNAL)) {
        seekg(offset + 1, std::ios::beg);
        return readInternalNodeAt(offset, fileSize);
    } else {
        throw BinaryTreeFileError("Unknown node type in file");
    }
}


void BinaryTreeFile::loadTree(Tree& tree) {
    if (!is_open()){ 
        throw BinaryTreeFileError("file not open");
        return;}

    tree.clear();

    seekg(0, std::ios::end);
    auto fileSize = static_cast<std::int64_t>(tellg());
    if (fileSize < 16) return; // Минимальный размер заголовка

    seekg(0, std::ios::beg);
    char magic[4]; // NOSONAR
    read(magic, 4);
    if (std::memcmp(magic, FILE_MAGIC, 4) != 0) throw BinaryTreeFileError("Bad file magic - not a tree file");

    if (read_le_uint32() != FILE_VERSION) throw BinaryTreeFileError("Unsupported file version");

    std::int64_t rootOffset = read_le_int64();
    if (rootOffset == OFFSET_NONE) {
        tree.setRoot(nullptr);
        return;
    }

    Node* newRoot = readNodeRecursive(rootOffset, fileSize);
    tree.setRoot(newRoot);
}


// --- Функции для записи/чтения в Little-Endian (без изменений) ---

void BinaryTreeFile::write_le_uint32(std::uint32_t v) {
    unsigned char b[4]; // NOSONAR
    b[0] = static_cast<unsigned char>(v & 0xFF);
    b[1] = static_cast<unsigned char>((v >> 8) & 0xFF);
    b[2] = static_cast<unsigned char>((v >> 16) & 0xFF);
    b[3] = static_cast<unsigned char>((v >> 24) & 0xFF);
    write(reinterpret_cast<char*>(b), 4);
    if (!good()) throw BinaryTreeFileError("I/O error writing uint32");
}

void BinaryTreeFile::write_le_int32(std::int32_t v) {
    write_le_uint32(static_cast<std::uint32_t>(v));
}

void BinaryTreeFile::write_le_int64(std::int64_t v) {
    unsigned char b[8]; // NOSONAR
    auto uv = static_cast<std::uint64_t>(v);
    for (int i = 0; i < 8; ++i) b[i] = static_cast<unsigned char>((uv >> (8 * i)) & 0xFF);
    write(reinterpret_cast<char*>(b), 8);
    if (!good()) throw BinaryTreeFileError("I/O error writing int64");
}

std::uint32_t BinaryTreeFile::read_le_uint32() {
    unsigned char b[4]; // NOSONAR
    read(reinterpret_cast<char*>(b), 4);
    if (gcount() != 4 || !good()) throw BinaryTreeFileError("I/O error reading uint32");
    std::uint32_t v = (static_cast<std::uint32_t>(b[0]) |
                       (static_cast<std::uint32_t>(b[1]) << 8) |
                       (static_cast<std::uint32_t>(b[2]) << 16) |
                       (static_cast<std::uint32_t>(b[3]) << 24));
    return v;
}

std::int32_t BinaryTreeFile::read_le_int32() {
    return static_cast<std::int32_t>(read_le_uint32());
}

std::int64_t BinaryTreeFile::read_le_int64() {
    unsigned char b[8]; // NOSONAR
    read(reinterpret_cast<char*>(b), 8);
    if (gcount() != 8 || !good()) throw BinaryTreeFileError("I/O error reading int64");
    std::uint64_t uv = 0;
    for (int i = 0; i < 8; ++i) uv |= (static_cast<std::uint64_t>(b[i]) << (8 * i));
    return static_cast<std::int64_t>(uv);
}