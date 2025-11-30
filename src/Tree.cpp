#include "Tree.h"
#include <cstring>

// ==========================================
// Реализация LeafNode
// ==========================================

LeafNode::LeafNode(const char* str, int len) {
    this->length = len;
    this->data = new char[len]; // NOSONAR
    this->lineCount = 0;
    
    // Инициализируем всю выделенную память нулями, чтобы избежать чтения "мусора".
    if (len > 0) {
        std::memset(this->data, 0, len);
    }

    // Копируем фактические данные, если str валиден.
    if (len > 0 && str) { 
        std::memcpy(this->data, str, len);
    }

    // Расчет теперь безопасен, т.к. this->data инициализирован.
    for (int i = 0; i < len; i++) {
        if (this->data[i] == '\n') {
            this->lineCount++;
        }
    }
}

LeafNode::~LeafNode() {
    delete[] data; // NOSONAR
}

NodeType LeafNode::getType() const { return NodeType::NODE_LEAF; }
int LeafNode::getLength() const { return length; }
int LeafNode::getLineCount() const { return lineCount; }

// ==========================================
// Реализация InternalNode
// ==========================================

InternalNode::InternalNode(Node* l, Node* r) {
    this->left = l;
    this->right = r;
    
    // Инициализируем нулями
    this->totalLength = 0;
    this->totalLineCount = 0;

    // Берем готовые данные из детей. Это O(1).
    if (l) {
        this->totalLength += l->getLength();
        this->totalLineCount += l->getLineCount();
    }
    if (r) {
        this->totalLength += r->getLength();
        this->totalLineCount += r->getLineCount();
    }
}

void InternalNode::recalc() {
    totalLength = 0;
    totalLineCount = 0;
    if (left) {
        totalLength += left->getLength();
        totalLineCount += left->getLineCount();
    }
    if (right) {
        totalLength += right->getLength();
        totalLineCount += right->getLineCount();
    }
}

NodeType InternalNode::getType() const { return NodeType::NODE_INTERNAL; }
int InternalNode::getLength() const { return totalLength; }
int InternalNode::getLineCount() const { return totalLineCount; }


// ==========================================
// Реализация Tree
// ==========================================

Tree::Tree() : root(nullptr) {}

Tree::~Tree() {
    clear();
}

// перемещающий конструктор
LeafNode::LeafNode(LeafNode&& other) noexcept {
    this->length = other.length;
    this->lineCount = other.lineCount;
    this->data = other.data;

    other.length = 0;
    other.lineCount = 0;
    other.data = nullptr;
}

// перемещающее присваивание
LeafNode& LeafNode::operator=(LeafNode&& other) noexcept {
    if (this != &other) {
        delete[] this->data; //NOSONAR освободить текущие данные (безопасно, может быть nullptr)
        this->length = other.length;
        this->lineCount = other.lineCount;
        this->data = other.data;

        other.length = 0;
        other.lineCount = 0;
        other.data = nullptr;
    }
    return *this;
}


void Tree::clear() {
    clearRecursive(root);
    root = nullptr;
}

void Tree::clearRecursive(Node* node) {
    if (!node) return;
    
    // Используем getType, как ты хотел
    if (node->getType() == NodeType::NODE_INTERNAL) {
        auto inner = static_cast<InternalNode*>(node);
        clearRecursive(inner->left);
        clearRecursive(inner->right);
    }
    delete node; // NOSONAR // Виртуальный деструктор сработает корректно
}

bool Tree::isEmpty() const { return root == nullptr; }
Node* Tree::getRoot() const { return root; }

void Tree::setRoot(Node* newRoot) {
    if (root && root != newRoot) clear();
    root = newRoot;
}

// --- Построение (Logic Update) ---

Node* Tree::buildFromTextRecursive(const char* text, int len) {
    if (len <= 0) return nullptr;

    // УСЛОВИЕ ЛИСТА:
    // Если текст влезает в лимит размера, делаем лист.
    // Это гарантирует, что даже файл без \n будет разбит на куски.
    if (len <= MAX_LEAF_SIZE) {
        return new LeafNode(text, len); // NOSONAR
    } 

    // ПОИСК ТОЧКИ РАЗРЕЗА:
    // Пытаемся найти \n рядом с серединой, чтобы не резать слова.
    int half = len / 2;
    int splitIndex = -1;
    
    // Ищем \n в диапазоне +/- 256 байт от середины (или меньше, если файл мал)
    int searchRange = (len < 512) ? (len / 4) : 256;

    // Ищем вправо от середины
    for (int i = 0; i < searchRange; i++) {
        if (half + i < len && text[half + i] == '\n') {
            splitIndex = half + i + 1; // Режем ПОСЛЕ \n
            break;
        }
    }
    
    // Если не нашли, ищем влево
    if (splitIndex == -1) {
        for (int i = 0; i < searchRange; i++) {
            if (half - i > 0 && text[half - i] == '\n') {
                splitIndex = half - i + 1;
                break;
            }
        }
    }

    // ФОЛЛБЭК (Fallback):
    // Если \n нет (minified файл) или он далеко — режем жестко пополам.
    if (splitIndex == -1) {
        splitIndex = half;
    }

    Node* left = buildFromTextRecursive(text, splitIndex);
    Node* right = buildFromTextRecursive(text + splitIndex, len - splitIndex);

    return new InternalNode(left, right); // NOSONAR
}

void Tree::fromText(const char* text, int len) {
    clear();
    if (!text || len <= 0) return;
    root = buildFromTextRecursive(text, len);
}

// --- Экспорт в текст ---

void Tree::collectTextRecursive(Node* node, char* buffer, int& pos) {
    if (!node) return;
    
    if (node->getType() == NodeType::NODE_LEAF) {
        auto leaf = static_cast<LeafNode*>(node);
        // memcpy быстрее цикла
        if (leaf->length > 0 && leaf->data) {
            std::memcpy(buffer + pos, leaf->data, leaf->length);
            pos += leaf->length;
        }
    } else {
        auto inner = static_cast<InternalNode*>(node);
        collectTextRecursive(inner->left, buffer, pos);
        collectTextRecursive(inner->right, buffer, pos);
    }
}

char* Tree::toText() {
    if (!root) {
        auto empty = new char[1]; // NOSONAR
        empty[0] = '\0';
        return empty;
    }
    
    // ТЕПЕРЬ МЫ ЗНАЕМ ДЛИНУ ЗА O(1)!
    // Не нужно запускать calculateLengthRecursive
    int totalLen = root->getLength();
    
    auto buffer = new char[totalLen + 1]; // NOSONAR
    int pos = 0;
    collectTextRecursive(root, buffer, pos);
    buffer[pos] = '\0';
    return buffer;
}

// --- Получение строки (Get Line) ---

LeafNode* Tree::findLeafByLineRecursive(Node* node, int& localLineIndex) {
    if (!node) return nullptr;

    if (node->getType() == NodeType::NODE_LEAF) {
        return static_cast<LeafNode*>(node);
    }

    auto inner = static_cast<InternalNode*>(node);
    
    // Ключевой момент оптимизации:
    // Мы спрашиваем у левого ребенка, сколько в нем строк. Это O(1) операция.
    int leftLines = 0;
    if (inner->left) {
        leftLines = inner->left->getLineCount();
    }

    if (localLineIndex < leftLines) {
        // Искомая строка слева
        return findLeafByLineRecursive(inner->left, localLineIndex);
    } else {
        // Искомая строка справа. Корректируем индекс.
        localLineIndex -= leftLines;
        return findLeafByLineRecursive(inner->right, localLineIndex);
    }
}

char* Tree::getLine(int lineNumber) {
    if (!root || lineNumber < 0) return nullptr;
    
    // Проверка: а есть ли такая строка вообще
    if (lineNumber < 0) return nullptr;
    if (lineNumber >= root->getLineCount()) return nullptr;


    int localIndex = lineNumber;
    // Этот метод спустится по дереву за O(log N) и вернет Лист.
    // localIndex станет номером строки ВНУТРИ этого листа.
    auto leaf = findLeafByLineRecursive(root, localIndex);

    if (!leaf) return nullptr;

    // Дальше логика поиска внутри листа (почти как у тебя было)
    int currentLine = 0;
    int startPos = 0;
    int endPos = -1;
    bool foundStart = false;

    // Если localIndex == 0, значит строка начинается с начала листа
    if (localIndex == 0) foundStart = true;


    for (int i = 0; i < leaf->length; i++) {
        // Поиск конца строки (выполняется, если начало уже найдено)
        if (foundStart) { // Уровень 2
            if (leaf->data[i] == '\n') { // Уровень 3
                endPos = i;
                break;
            }
            continue; // Если не нашли конец, продолжаем сканирование
        }
        
        // Поиск начала строки (выполняется, если !foundStart)
        
        // Если это не перенос строки, пропускаем (Level 2)
        if (leaf->data[i] != '\n') {
            continue;
        }

        // Это перенос строки, увеличиваем счетчик
        currentLine++;
        
        // Проверяем, достигли ли мы нужной строки (Level 2)
        if (currentLine == localIndex) {
            startPos = i + 1; // Начало сразу после \n
            foundStart = true;
        }
        
        // После нахождения начала, продолжаем итерацию, чтобы следующий символ
        // был обработан блоком 'if (foundStart)' для поиска конца.
    }

    // Если дошли до конца листа, а \n не встретили — значит строка идет до конца блока
    if (foundStart && endPos == -1) {
        endPos = leaf->length;
    }

    if (!foundStart) return nullptr; // Что-то пошло не так

    int lineLen = endPos - startPos;
    // Защита от отрицательной длины
    if (lineLen < 0) lineLen = 0; 

    auto result = new char[lineLen + 1]; // NOSONAR
    if (lineLen > 0) {
        std::memcpy(result, leaf->data + startPos, lineLen);
    }
    result[lineLen] = '\0';

    return result;
}

// Поиск листа по смещению (внутри Leaf — localOffset станет смещением в листе)
LeafNode* Tree::findLeafByOffsetRecursive(Node* node, int& localOffset) {
    if (!node) return nullptr;
    if (node->getType() == NodeType::NODE_LEAF) {
        return static_cast<LeafNode*>(node);
    }
    auto inner = static_cast<InternalNode*>(node);
    int leftLen = 0;
    if (inner->left) leftLen = inner->left->getLength();
    if (localOffset < leftLen) {
        return findLeafByOffsetRecursive(inner->left, localOffset);
    } else {
        localOffset -= leftLen;
        return findLeafByOffsetRecursive(inner->right, localOffset);
    }
}

// splitLeafAtOffset: возвращает новый суб-дерево указатель (InternalNode или Leaf/NULL)
Node* Tree::splitLeafAtOffset(LeafNode* leaf, int offset) {
    if (!leaf) return nullptr;
    if (offset < 0) offset = 0;
    if (offset > leaf->length) offset = leaf->length;

    int leftLen = offset;
    int rightLen = leaf->length - offset;

    LeafNode* leftLeaf = nullptr;
    LeafNode* rightLeaf = nullptr;

    if (leftLen > 0) leftLeaf = new LeafNode(leaf->data, leftLen); //NOSONAR
    if (rightLen > 0) rightLeaf = new LeafNode(leaf->data + offset, rightLen);//NOSONAR

    // удаляем исходный лист (который заменяется)
    delete leaf;//NOSONAR

    // Если одна часть пустая — возвращаем только ненулевую часть:
    if (!leftLeaf && !rightLeaf) return nullptr;
    if (!leftLeaf) return rightLeaf;
    if (!rightLeaf) return leftLeaf;

    return new InternalNode(leftLeaf, rightLeaf);//NOSONAR
}


// Вспомогательная: вычислить индекс разреза (поведение как в buildFromTextRecursive)
int Tree::findSplitIndexForLeaf(const LeafNode* leaf) const {
    if (!leaf || !leaf->data) return 0;
    int half = leaf->length / 2;
    int searchRange = (leaf->length < 512) ? (leaf->length / 4) : 256;

    // вправо
    for (int i = 0; i < searchRange; ++i) {
        int idx = half + i;
        if (idx < leaf->length && leaf->data[idx] == '\n') return idx + 1;
    }
    // влево
    for (int i = 0; i < searchRange; ++i) {
        int idx = half - i;
        if (idx > 0 && idx < leaf->length && leaf->data[idx] == '\n') return idx + 1;
    }
    return half;
}

Node* Tree::insertIntoLeaf(LeafNode* leaf, int pos, const char* data, int len) {
    if (!leaf) {
        return new LeafNode(data, len); // NOSONAR
    }

    if (pos < 0) pos = 0;
    if (pos > leaf->length) pos = leaf->length;

    int leafLen = leaf->length;
    int newLen = leafLen + len;

    // Сборка временного буфера: [prefix][data][suffix]
    char* buf = new char[newLen]; // NOSONAR
    if (pos > 0 && leaf->data) {
        std::memcpy(buf, leaf->data, pos);
    }
    if (len > 0 && data) {
        std::memcpy(buf + pos, data, len);
    }
    if (pos < leafLen && leaf->data) {
        std::memcpy(buf + pos + len, leaf->data + pos, leafLen - pos);
    }

    // Новый лист
    LeafNode* newLeaf = new LeafNode(buf, newLen);// NOSONAR
    delete[] buf;// NOSONAR
    delete leaf; // NOSONAR// удалить старый лист

    // Если превысили порог — split
    if (newLeaf->length > MAX_LEAF_SIZE) {
        int splitIndex = findSplitIndexForLeaf(newLeaf);
        Node* splitted = splitLeafAtOffset(newLeaf, splitIndex);
        return splitted;
    }

    return newLeaf;
}


// Вставляет [data, data+len) в позицию pos внутри node и возвращает новый Node* для замены.
Node* Tree::insertRecursive(Node* node, int pos, const char* data, int len) {
    if (len <= 0) return node;

    if (!node) {
        return new LeafNode(data, len); // NOSONAR
    }

    if (node->getType() == NodeType::NODE_LEAF) {
        return insertIntoLeaf(static_cast<LeafNode*>(node), pos, data, len);
    }

    // Internal node: опустим лишнюю вложенность — минимальный код
    auto inner = static_cast<InternalNode*>(node);

    if (int leftLen = (inner->left ? inner->left->getLength() : 0); pos <= leftLen) {
        inner->left = insertRecursive(inner->left, pos, data, len);
    } else {
        inner->right = insertRecursive(inner->right, pos - leftLen, data, len);
    }

    inner->recalc();
    return inner;
}

// Удалить len байт, начиная с pos, внутри листа.
// Возвращает новый лист (или nullptr), удаляет старый leaf.
Node* Tree::eraseFromLeaf(LeafNode* leaf, int pos, int len) {
    if (!leaf || len <= 0) return leaf;

    // Нормализуем pos
    if (pos < 0) pos = 0;
    if (pos >= leaf->length) return leaf; // ничего не удаляем

    int delLen = len;
    if (pos + delLen > leaf->length) delLen = leaf->length - pos;

    int newLen = leaf->length - delLen;
    if (newLen <= 0) {
        delete leaf;// NOSONAR // удалили весь лист
        return nullptr;
    }

    // Создаём буфер без использования 'p'
    char* buf = new char[newLen]; // NOSONAR
    if (pos > 0 && leaf->data) {
        std::memcpy(buf, leaf->data, pos);
    }
    if (pos + delLen < leaf->length && leaf->data) {
        int tail = leaf->length - (pos + delLen);
        std::memcpy(buf + pos, leaf->data + pos + delLen, tail);
    }

    LeafNode* newLeaf = new LeafNode(buf, newLen); // NOSONAR
    delete[] buf; // NOSONAR
    delete leaf;  // NOSONAR
    return newLeaf;
}

// Если один/оба ребёнка отсутствуют — заменить internal на существующего ребёнка (или nullptr).
// Иначе пересчитать кэши и вернуть inner.
Node* Tree::collapseInternalIfNeeded(InternalNode* inner) {
    if (!inner) return nullptr;

    if (!inner->left && !inner->right) {
        delete inner; // NOSONAR
        return nullptr;
    }
    if (!inner->left) {
        Node* r = inner->right;
        delete inner; // NOSONAR
        return r;
    }
    if (!inner->right) {
        Node* l = inner->left;
        delete inner; // NOSONAR
        return l;
    }

    // оба ребёнка существуют — обновляем кэши и возвращаем узел
    inner->recalc();
    return inner;
}

// Удалить len байт, начиная с pos. Возвращает новое поддерево.
Node* Tree::eraseRecursive(Node* node, int pos, int len) {
    if (!node || len <= 0) return node;

    // Если лист — делегируем в отдельную функцию
    if (node->getType() == NodeType::NODE_LEAF) {
        return eraseFromLeaf(static_cast<LeafNode*>(node), pos, len);
    }

    // Internal node
    auto inner = static_cast<InternalNode*>(node);

    // Используем init-statement (современный стиль)
    if (int leftLen = (inner->left ? inner->left->getLength() : 0); pos + len <= leftLen) {
        // Всё удаление в левом поддереве
        inner->left = eraseRecursive(inner->left, pos, len);
    } else if (pos >= leftLen) {
        // Всё удаление в правом
        inner->right = eraseRecursive(inner->right, pos - leftLen, len);
    } else {
        // Разрезано: часть слева, часть справа
        int leftDel = leftLen - pos;
        int rightDel = len - leftDel;
        inner->left = eraseRecursive(inner->left, pos, leftDel);
        inner->right = eraseRecursive(inner->right, 0, rightDel);
    }

    // Свернуть internal если нужно (включая пересчёт кэшей)
    return collapseInternalIfNeeded(inner);
}


void Tree::insert(int pos, const char* data, int len) {
    if (len <= 0) return;

    int total = 0;
    if (root) total = root->getLength();
    if (pos < 0) pos = 0;
    if (pos > total) pos = total;

    root = insertRecursive(root, pos, data, len);
    // Для простоты — делаем глобальный ребаланс после операции.
    rebalance();
}

void Tree::erase(int pos, int len) {
    if (!root || len <= 0) return;
    int total = root->getLength();
    if (pos < 0) pos = 0;
    if (pos >= total) return;

    if (pos + len > total) len = total - pos;

    root = eraseRecursive(root, pos, len);
    rebalance();
}

// Рекурсивный проход, объединяем соседние листья когда это возможно.
// Используется Node*& чтобы иметь возможность заменить node на новый лист.
void Tree::rebalanceRecursive(Node*& node) {
    if (!node) return;
    if (node->getType() == NodeType::NODE_LEAF) return;

    auto inner = static_cast<InternalNode*>(node);
    // рекурсивно обработать детей
    if (inner->left) rebalanceRecursive(inner->left);
    if (inner->right) rebalanceRecursive(inner->right);

    // если оба child — листья — попытаться объединить
    if (inner->left && inner->right &&
        inner->left->getType() == NodeType::NODE_LEAF &&
        inner->right->getType() == NodeType::NODE_LEAF) {

        auto L = static_cast<LeafNode*>(inner->left);
        auto R = static_cast<LeafNode*>(inner->right);
        int combined = L->length + R->length;
        if (combined <= MAX_LEAF_SIZE) {
            // собрать единый буфер
            auto merged = new char[combined];//NOSONAR
            int p = 0;
            if (L->length > 0 && L->data) {
                std::memcpy(merged + p, L->data, L->length);
                p += L->length;
            }
            if (R->length > 0 && R->data) {
                std::memcpy(merged + p, R->data, R->length);
                p += R->length;
            }
            // удаляем старые объекты
            delete L;//NOSONAR
            delete R;//NOSONAR
            delete inner;//NOSONAR

            // создаём новый лист и заменяем node
            LeafNode* newLeaf = new LeafNode(merged, combined);//NOSONAR

            delete[] merged; //NOSONAR

            node = newLeaf;
            return;
        }
    }

    // иначе пересчитать кэши
    inner->recalc();
}

void Tree::rebalance() {
    rebalanceRecursive(root);
}
