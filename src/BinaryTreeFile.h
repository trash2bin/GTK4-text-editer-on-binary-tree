#ifndef BINARY_TREE_FILE_H
#define BINARY_TREE_FILE_H

#include "Tree.h" // Нужен для доступа к структурам Node и классу Tree
#include <fstream>
#include <cstdint>

// Формат узла (leaf):
// [1 byte type == NODE_LEAF]
// [int32 length]        -- количество байт данных
// [int32 lineCount]     -- целый счётчик (кол-во строк / кэш)
// [length bytes]        -- данные (без '\0')
//
// Формат internal:
// [1 byte type == NODE_INTERNAL]
// [int64 leftOffset]
// [int64 rightOffset]
//
// Заголовок файла:
// [4 bytes magic "TREE"]
// [uint32 version]
// [int64 rootOffset]    -- OFFSET_NONE (-1) означает пустое дерево
class BinaryTreeFile : public std::fstream {
private:
    // Имя файла, чтобы можно было усечь/переоткрыть при сохранении
    std::string m_filename; 

    // Рекурсивные методы I/O, работающие с узлами (Node*)
    std::int64_t  writeNodeRecursive(Node* node);

    Node* readLeafNodeAt(std::int64_t offset, std::int64_t fileSize);
    Node* readInternalNodeAt(std::int64_t offset, std::int64_t fileSize);
    Node* readNodeRecursive(std::int64_t offset, std::int64_t fileSize);

    // Вспомогательные: чтение/запись в little-endian фиксированных типов
    void write_le_int32(std::int32_t v);
    void write_le_int64(std::int64_t v);
    void write_le_uint32(std::uint32_t v);

    std::int32_t read_le_int32();
    std::int64_t read_le_int64();
    std::uint32_t read_le_uint32();

public:
    BinaryTreeFile();
    ~BinaryTreeFile() override;

    bool openFile(const char* filename);
    
    // Основные операции сериализации/десериализации
    void saveTree(const Tree& tree);
    void loadTree(Tree& tree);
};

#endif // BINARY_TREE_FILE_H