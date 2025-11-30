#ifndef TREE_H
#define TREE_H


//! КРАЙ ПО КОТОРОМУ РЕЖЕТСЯ ЛИСТ - НЕКОРРЕКТНОЕ ПОВЕДЕНИЕ ПОСЛЕ 
const int MAX_LEAF_SIZE = 4096; //TODO: фикс

enum class NodeType : char {
    NODE_INTERNAL = 0,
    NODE_LEAF = 1
};

struct Node {
    // Оставляем getType, как ты просил
    virtual NodeType getType() const = 0;
    
    // Новые виртуальные методы для быстрого доступа к статистике
    virtual int getLength() const = 0;     // Вес в байтах
    virtual int getLineCount() const = 0;  // Вес в строках (\n)
    
    virtual ~Node() = default;
};

struct LeafNode : public Node {
    int length;
    int lineCount; // Кэшированное значение
    char* data;

    LeafNode(const char* str, int len);
    ~LeafNode() override;

     // запрет копирования
    LeafNode(const LeafNode&) = delete; 
     // запрет присваивания копированием
    LeafNode& operator=(const LeafNode&) = delete;

    // Реализуем перемещающий конструктор и перемещающее присваивание:
    LeafNode(LeafNode&& other) noexcept;
    LeafNode& operator=(LeafNode&& other) noexcept;

    NodeType getType() const override;
    int getLength() const override;
    int getLineCount() const override;
};

struct InternalNode : public Node {
    Node* left;
    Node* right;

    // Кэшированные суммы детей
    int totalLength;
    int totalLineCount;

    InternalNode(Node* l, Node* r);
    ~InternalNode() override = default;
    
    NodeType getType() const override;
    int getLength() const override;
    int getLineCount() const override;

    void recalc(); // пересчитать totalLength и totalLineCount из детей
};

class Tree {
private:
    Node* root;

    void clearRecursive(Node* node);
    Node* buildFromTextRecursive(const char* text, int len);
    
    // Вспомогательная рекурсия для сбора текста (теперь проще)
    void collectTextRecursive(Node* node, char* buffer, int& pos);

    // Вспомогательная функция для поиска листа по номеру строки
    // Изменяет localLineIndex, приводя его к индексу внутри найденного листа
    LeafNode* findLeafByLineRecursive(Node* node, int& localLineIndex);

    LeafNode* findLeafByOffsetRecursive(Node* node, int& localOffset);
    Node* splitLeafAtOffset(LeafNode* leaf, int offset);

    Node* insertIntoLeaf(LeafNode* leaf, int pos, const char* data, int len);
    int findSplitIndexForLeaf(const LeafNode* leaf) const;
    // Рекурсивные реализации вставки/удаления (возвращают новый Node* для замены в родителе)
    Node* insertRecursive(Node* node, int pos, const char* data, int len);

    // Удалить len байт в листе, возвращает новый Node* (новый лист или nullptr)
    Node* eraseFromLeaf(LeafNode* leaf, int pos, int len);

    // Если internal-узел имеет одного или ни одного ребёнка — заменить и удалить internal.
    // В противном случае пересчитать кэши и вернуть сам inner.
    Node* collapseInternalIfNeeded(InternalNode* inner);

    Node* eraseRecursive(Node* node, int pos, int len);

    void getTextRangeRecursive(Node* node, int& offset, int& len, char* out, int& outPos) const;

    void buildKMPTable(const char* pattern, int patternLen, int* lps) const;

    int findSubstringRecursive(Node* node, const char* pattern, int patternLen, const int* lps, int& j, int& processed) const;

public:
    Tree();
    ~Tree();

    void clear();
    bool isEmpty() const;

    void fromText(const char* text, int len);
    char* toText();
    
    char* getLine(int lineNumber);

    int getTotalLineCount() const;
    int getOffsetForLine(int lineIndex0Based) const;

    // возвращает новый буфер длиной len (или nullptr, если len==0).
    // Владелец вызывающий код должен вызвать delete[]
    char* getTextRange(int offset, int len) const;

    int findSubstring(const char* pattern, int patternLen) const; // offset или -1

    // Возвращает номер строки (0-based), в которой начинается совпадение шаблона,
    // или -1 если не найдено.
    int findSubstringLine(const char* pattern, int patternLen) const;

    void insert(int pos, const char* data, int len); // публичная обёртка
    void erase(int pos, int len);// публичная обёртка


    Node* getRoot() const;
    void setRoot(Node* newRoot);
};

#endif // TREE_H