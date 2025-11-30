#include "Tree.h"
#include <cassert>
#include <cstring>
#include <stdexcept>
#include <sstream>

// ==========================================
// Реализация LeafNode
// ==========================================

LeafNode::LeafNode(const char* str, int len) {
    this->length = len;
    this->data = new char[len]; // NOSONAR
    this->lineCount = 1;
    
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
LeafNode::LeafNode(LeafNode&& other) noexcept 
    : length(0), lineCount(0), data(nullptr) {
    *this = std::move(other);
}

LeafNode& LeafNode::operator=(LeafNode&& other) noexcept {
    if (this != &other) {
        delete[] data; //NOSONAR  // Очищаем текущие данные
        
        length = other.length;
        lineCount = other.lineCount;
        data = other.data;
        
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
    //! КРАЙ ПО КОТОРОМУ РЕЖЕТСЯ ЛИСТ - НЕКОРРЕКТНОЕ ПОВЕДЕНИЕ ПОСЛЕ
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


    Node* left = nullptr;
    Node* right = nullptr;

    // Построим детей. Если создание одного из них бросит — мы ничего не удаляем,
    // потому что ничего ещё не было выделено на нашей стороне.
    left = buildFromTextRecursive(text, splitIndex);
    right = buildFromTextRecursive(text + splitIndex, len - splitIndex);

    // Попытка создать internal; если бросит — не забываем удалить детей.
    try {
        Node* node = new InternalNode(left, right); //NOSONAR
        return node;
    } catch (...) {
        // Удаляем уже созданных потомков во избежание утечек.
        if (left)  clearRecursive(left);
        if (right) clearRecursive(right);
        throw;
    }
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

// Tree.cpp
int Tree::getTotalLineCount() const {
    if (!root) return 0;
    return root->getLineCount();
}

// static helper: вычислить байтовое смещение для начала указанной строки внутри поддерева.
// Предполагается: node != nullptr и lineIndex корректен для этого поддерева.
// При нарушении инвариантов — assertion в debug.
static int getOffsetForLineRecursive(Node* node, int lineIndex) {
    assert(node != nullptr);

    if (node->getType() == NodeType::NODE_LEAF) {
        // Т.к. мы проверили getType, static_cast безопасен и быстрее dynamic_cast.
        auto leaf = static_cast<LeafNode*>(node);
        // Защита на случай нарушения инварианта (только debug)
        assert(leaf != nullptr);

        int linesSeen = 0;
        for (int i = 0; i < leaf->length; ++i) {
            if (linesSeen == lineIndex) return i; // offset внутри листа
            if (leaf->data[i] == '\n') ++linesSeen;
        }
        if (linesSeen == lineIndex) return leaf->length;
        // Если индекс оказался некорректным — бросим понятное исключение в релизе.
        throw std::out_of_range("Line index out of range inside leaf");
    } else {
        // internal node
        auto in = static_cast<InternalNode*>(node);
        assert(in != nullptr);

        int leftLines = in->left ? in->left->getLineCount() : 0;
        if (lineIndex < leftLines) {
            return getOffsetForLineRecursive(in->left, lineIndex);
        } else {
            int leftLen = in->left ? in->left->getLength() : 0;
            return leftLen + getOffsetForLineRecursive(in->right, lineIndex - leftLines);
        }
    }
}


int Tree::getOffsetForLine(int lineIndex0Based) const {
    if (!root) throw std::out_of_range("Tree is empty");
    if (lineIndex0Based < 0 || lineIndex0Based >= getTotalLineCount()) {
        std::basic_ostringstream<char> oss;
        oss << "Line index out of range (0.." << (getTotalLineCount()-1) << ")";
        throw std::out_of_range(oss.str());
    }
    return getOffsetForLineRecursive(root, lineIndex0Based);
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
Node* Tree::splitLeafAtOffset(LeafNode* leaf, int offset) { //NOSONAR
    if (!leaf) return nullptr;
    if (offset < 0) offset = 0;
    if (offset > leaf->length) offset = leaf->length;

    int leftLen = offset;
    int rightLen = leaf->length - offset;

    LeafNode* leftLeaf = nullptr;
    LeafNode* rightLeaf = nullptr;

    // Попытка создать левый лист (если нужен)
    if (leftLen > 0) {
        try {
            leftLeaf = new LeafNode(leaf->data, leftLen); //NOSONAR
        } catch (...) {
            if (leftLeaf)  clearRecursive(leftLeaf);
            if (rightLeaf) clearRecursive(rightLeaf);
            throw;
        }
    }

    // Попытка создать правый лист (если нужен)
    if (rightLen > 0) {
        try {
            rightLeaf = new LeafNode(leaf->data + offset, rightLen); //NOSONAR
        } catch (...) {
            // если левый уже создан — удалить его, чтобы не было утечки
            if (leftLeaf) { delete leftLeaf; leftLeaf = nullptr; }//NOSONAR
            throw;
        }
    }

    // На этом моменте leftLeaf/rightLeaf созданы успешно (возможны nullptr).
    // Создаём возвращаемую структуру и только после успешного создания — удаляем исходный leaf.
    Node* result = nullptr;
    try {
        if (!leftLeaf && !rightLeaf) {
            result = nullptr;
        } else if (!leftLeaf) {
            result = rightLeaf;
            rightLeaf = nullptr; // ownership перенесён
        } else if (!rightLeaf) {
            result = leftLeaf;
            leftLeaf = nullptr;
        } else {
            result = new InternalNode(leftLeaf, rightLeaf);//NOSONAR
            leftLeaf = nullptr; rightLeaf = nullptr;
        }
    } catch (...) {
        // Если создание InternalNode упало — удалить дочерние листья (если они ещё у нас)
        if (leftLeaf) { delete leftLeaf; leftLeaf = nullptr; }//NOSONAR
        if (rightLeaf) { delete rightLeaf; rightLeaf = nullptr; }//NOSONAR
        throw;
    }

    // Теперь безопасно удалить оригинал — ownership перенесён.
    delete leaf;//NOSONAR
    return result;
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

// ------------------ insertIntoLeaf (защита временного буфера) ------------------
Node* Tree::insertIntoLeaf(LeafNode* leaf, int pos, const char* data, int len) {
    if (!leaf) {
        // Прямо создаём лист; если бросит — ничего не утекает здесь.
        return new LeafNode(data, len);//NOSONAR
    }

    if (pos < 0) pos = 0;
    if (pos > leaf->length) pos = leaf->length;

    int leafLen = leaf->length;
    int newLen = leafLen + len;

    // Временный буфер
    char* buf = nullptr;
    try {
        buf = new char[newLen];//NOSONAR
    } catch (...) { //NOSONAR
        throw; // если не выделилось — просто пробросим
    }

    if (pos > 0 && leaf->data) {
        std::memcpy(buf, leaf->data, pos);
    }
    if (len > 0 && data) {
        std::memcpy(buf + pos, data, len);
    }
    if (pos < leafLen && leaf->data) {
        std::memcpy(buf + pos + len, leaf->data + pos, leafLen - pos);
    }

    // Создаём новый лист; если бросит — освободим buf
    LeafNode* newLeaf = nullptr;
    try {
        newLeaf = new LeafNode(buf, newLen);//NOSONAR
    } catch (...) {
        delete[] buf;//NOSONAR
        throw;
    }
    // newLeaf создан успешно — временный buf больше не нужен
    delete[] buf;//NOSONAR

    // Удаляем исходный лист (ownership перенесён)
    delete leaf;//NOSONAR

    // Если слишком большой — разбиваем; splitLeafAtOffset берёт владение newLeaf или удалит его при ошибке.
    //! КРАЙ ПО КОТОРОМУ РЕЖЕТСЯ ЛИСТ - НЕКОРРЕКТНОЕ ПОВЕДЕНИЕ ПОСЛЕ
    if (newLeaf->length > MAX_LEAF_SIZE) {
        int splitIndex = findSplitIndexForLeaf(newLeaf);
        Node* splitted = nullptr;
        try {
            splitted = splitLeafAtOffset(newLeaf, splitIndex);
            return splitted;
        } catch (...) {
            // split упал — newLeaf может быть удалён внутри splitLeafAtOffset при ошибке,
            // либо если split выбросил до удаления newLeaf — удалим его здесь, но проверить на nullptr.
            // В нашем splitLeafAtOffset newLeaf не удаляется при исключении (мы защитились), поэтому:
            if (newLeaf) delete newLeaf;//NOSONAR
            throw;
        }
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

    if (pos < 0) pos = 0;
    if (pos >= leaf->length) return leaf;

    int delLen = len;
    if (pos + delLen > leaf->length) delLen = leaf->length - pos;

    int newLen = leaf->length - delLen;
    if (newLen <= 0) {
        delete leaf; //NOSONAR
        return nullptr;
    }

    char* buf = nullptr;
    try {
        buf = new char[newLen]; //NOSONAR
    } catch (...) { //NOSONAR
        throw;
    }

    if (pos > 0 && leaf->data) {
        std::memcpy(buf, leaf->data, pos);
    }
    if (pos + delLen < leaf->length && leaf->data) {
        int tail = leaf->length - (pos + delLen);
        std::memcpy(buf + pos, leaf->data + pos + delLen, tail);
    }

    LeafNode* newLeaf = nullptr;
    try {
        newLeaf = new LeafNode(buf, newLen); //NOSONAR
    } catch (...) {
        delete[] buf; //NOSONAR
        throw;
    }
    delete[] buf; //NOSONAR
    delete leaf; //NOSONAR
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
}

void Tree::erase(int pos, int len) {
    if (!root || len <= 0) return;
    int total = root->getLength();
    if (pos < 0) pos = 0;
    if (pos >= total) return;

    if (pos + len > total) len = total - pos;

    root = eraseRecursive(root, pos, len);
}


void Tree::getTextRangeRecursive(Node* node, int& offset, int& len, char* out, int& outPos) const {
    if (!node || len <= 0) return;

    if (node->getType() == NodeType::NODE_LEAF) {
        auto leaf = static_cast<LeafNode*>(node); // у тебя уже проверка через getType
        if (offset >= leaf->length) {
            offset -= leaf->length;
            return;
        }

        int copyFrom = offset;
        int toCopy = (len < leaf->length - copyFrom) ? len : (leaf->length - copyFrom);

        std::memcpy(out + outPos, leaf->data + copyFrom, static_cast<size_t>(toCopy));

        outPos += toCopy;
        len -= toCopy;
        offset = 0;
        return;
    } else {
        auto in = static_cast<InternalNode*>(node);
        if (in->left) getTextRangeRecursive(in->left, offset, len, out, outPos);
        if (len > 0 && in->right) getTextRangeRecursive(in->right, offset, len, out, outPos);
    }
}

char* Tree::getTextRange(int offset, int len) const {
    // Если дерево пустое — возвращаем nullptr (как раньше).
    if (!root) return nullptr;

    // Нормализуем len: если запрошено 0 байт — возвращаем пустую нуль-терминированную строку.
    if (len <= 0) {
        auto empty = new char[1]; //NOSONAR
        empty[0] = '\0';
        return empty;
    }

    int total = root->getLength();
    if (offset < 0 || offset > total) throw std::out_of_range("Offset out of range");
    if (offset + len > total) len = total - offset; // обрезка до конца

    // Выделяем +1 байт для нуль-терминатора.
    auto out = new char[len + 1]; //NOSONAR // теперь место под '\0'
    int outPos = 0;
    int off = offset;
    int l = len;
    getTextRangeRecursive(root, off, l, out, outPos);

    // Гарантируем нуль-терминатор; outPos должен быть равен len, но на всякий случай ставим '\0' по outPos.
    out[outPos] = '\0';

    return out;
}
// --- вспомогательная функция: строим lps (longest prefix suffix) для KMP вручную ---
void Tree::buildKMPTable(const char* pattern, int patternLen, int* lps) const {
    int len = 0;
    lps[0] = 0;
    int i = 1;
    while (i < patternLen) {
        if (pattern[i] == pattern[len]) {
            len++;
            lps[i] = len;
            i++;
        } else {
            if (len != 0) {
                len = lps[len - 1];
            } else {
                lps[i] = 0;
                i++;
            }
        }
    }
}

// --- рекурсивный обход листов с поиском ---
int Tree::findSubstringRecursive(Node* node, const char* pattern, int patternLen, const int* lps, int& j, int& processed) const {
    if (!node) return -1;

    if (node->getType() == NodeType::NODE_LEAF) {
        // static_cast безопасен, т.к. проверили тип
        auto leaf = static_cast<LeafNode*>(node);
        assert(leaf != nullptr);

        for (int i = 0; i < leaf->length; ++i) {
            auto c = static_cast<unsigned char>(leaf->data[i]);
            while (j > 0 && c != static_cast<unsigned char>(pattern[j])) j = lps[j - 1];
            if (c == static_cast<unsigned char>(pattern[j])) j++;
            if (j == patternLen) {
                int matchEnd = processed + i;
                int matchStart = matchEnd - patternLen + 1;
                return matchStart;
            }
        }
        processed += leaf->length;
        return -1;
    } else {
        // internal node — static_cast после проверки типа
        auto in = static_cast<InternalNode*>(node);
        assert(in != nullptr);

        int r = -1;
        if (in->left) { r = findSubstringRecursive(in->left, pattern, patternLen, lps, j, processed); if (r != -1) return r; }
        if (in->right) { r = findSubstringRecursive(in->right, pattern, patternLen, lps, j, processed); if (r != -1) return r; }
        return -1;
    }
}


int Tree::findSubstring(const char* pattern, int patternLen) const {
    if (!root || !pattern || patternLen <= 0) return -1;

    int* lps = nullptr;
    try {
        lps = new int[patternLen]; //NOSONAR
    } catch (...) {
        delete[] lps; //NOSONAR
        throw;
    }

    try {
        buildKMPTable(pattern, patternLen, lps);

        int j = 0;
        int processed = 0;
        int result = findSubstringRecursive(root, pattern, patternLen, lps, j, processed);

        delete[] lps; //NOSONAR
        return result;
    } catch (...) {
        delete[] lps; //NOSONAR
        throw;
    }
}

// Рекурсивный KMP-по-листам, но аккумулирует количество строк (processedLines).
// Возвращает номер строки (0-based) если найдено, иначе -1.
// Параметры:
//  - node: текущий узел
//  - pattern, patternLen: шаблон и его длина
//  - lps: предвычисленная таблица KMP
//  - j: текущее состояние автомата KMP (сохраняется между листами)
//  - processedLines: сколько строк ( '\n' ) уже полностью пройдены раньше (в предыдущих листьях)
static int findSubstringLineRecursive(Node* node,
                                      const char* pattern, int patternLen,
                                      const int* lps,
                                      int& j,
                                      int& processedLines) {
    if (!node) return -1;

    if (node->getType() == NodeType::NODE_LEAF) {
        auto leaf = static_cast<LeafNode*>(node);
        // Проходим байты листа, применяем KMP.
        for (int i = 0; i < leaf->length; ++i) {
            auto c = static_cast<unsigned char>(leaf->data[i]);
            while (j > 0 && c != static_cast<unsigned char>(pattern[j])) {
                j = lps[j - 1];
            }
            if (c == static_cast<unsigned char>(pattern[j])) ++j;
            if (j == patternLen) {
                // Нашли совпадение: вычислим номер строки внутри leaf до начала совпадения.
                int matchEndIndex = i;
                int matchStartIndex = matchEndIndex - patternLen + 1;
                if (matchStartIndex < 0) matchStartIndex = 0; // безопасность

                int localNewlines = 0;
                // считаем number of '\n' в leaf->data[0 .. matchStartIndex-1]
                for (int k = 0; k < matchStartIndex && k < leaf->length; ++k) {
                    if (leaf->data[k] == '\n') ++localNewlines;
                }

                // итоговый номер строки (0-based)
                return processedLines + localNewlines;
            }
        }

        // Если не нашли в этом листе — аккумулируем все строки из листа и идём дальше.
        processedLines += leaf->getLineCount();
        return -1;
    } else {
        auto in = static_cast<InternalNode*>(node);
        int r = -1;
        if (in->left) {
            r = findSubstringLineRecursive(in->left, pattern, patternLen, lps, j, processedLines);
            if (r != -1) return r;
        }
        if (in->right) {
            r = findSubstringLineRecursive(in->right, pattern, patternLen, lps, j, processedLines);
            if (r != -1) return r;
        }
        return -1;
    }
}

// Публичная обёртка: возвращает номер строки (0-based) где начинается совпадение,
// или -1 если не найдено.
int Tree::findSubstringLine(const char* pattern, int patternLen) const {
    if (!root || !pattern || patternLen <= 0) return -1;

    // выделяем lps
    int* lps = nullptr;
    try {
        lps = new int[patternLen]; // NOSONAR
    } catch (...) {
        // если не удалось выделить память — возвратим ошибку
        if (lps) delete[] lps; // NOSONAR
        throw;
    }

    try {
        // построим KMP-таблицу (у тебя есть buildKMPTable)
        buildKMPTable(pattern, patternLen, lps);

        int j = 0;
        int processedLines = 0;
        int res = findSubstringLineRecursive(root, pattern, patternLen, lps, j, processedLines);

        delete[] lps; // NOSONAR
        return res;
    } catch (...) {
        if (lps) delete[] lps; // NOSONAR
        throw;
    }
}

