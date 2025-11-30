#include <iostream>
#include <cstring>
#include <string>
#include <stdexcept>
#include "Tree.h"

// Глобальные счетчики для статистики
int total_tests = 0;
int failed_tests = 0;

// Вспомогательная функция для преобразования NodeType в строку
std::string nodeTypeToString(NodeType type) {
    switch(type) {
        case NodeType::NODE_INTERNAL: return "NODE_INTERNAL";
        case NodeType::NODE_LEAF: return "NODE_LEAF";
        default: return "UNKNOWN";
    }
}

// Макросы для упрощения проверок
#define ASSERT(condition, message) do { \
    total_tests++; \
    if (!(condition)) { \
        std::cerr << "[FAIL] " << message << " (" << __FILE__ << ":" << __LINE__ << ")" << std::endl; \
        failed_tests++; \
        return false; \
    } \
} while(0)

#define ASSERT_EQUAL(actual, expected, message) do { \
    total_tests++; \
    if ((actual) != (expected)) { \
        std::cerr << "[FAIL] " << message << " - Expected: " << (expected) << ", Actual: " << (actual) \
                  << " (" << __FILE__ << ":" << __LINE__ << ")" << std::endl; \
        failed_tests++; \
        return false; \
    } \
} while(0)

// Специальная версия для сравнения NodeType
#define ASSERT_EQUAL_NODE_TYPE(actual, expected, message) do { \
    total_tests++; \
    if ((actual) != (expected)) { \
        std::cerr << "[FAIL] " << message << " - Expected: " << nodeTypeToString(expected) \
                  << ", Actual: " << nodeTypeToString(actual) \
                  << " (" << __FILE__ << ":" << __LINE__ << ")" << std::endl; \
        failed_tests++; \
        return false; \
    } \
} while(0)

#define ASSERT_THROW(expression, exception_type, message) do { \
    total_tests++; \
    bool exception_caught = false; \
    try { \
        expression; \
    } catch (const exception_type&) { \
        exception_caught = true; \
    } catch (...) { \
        std::cerr << "[FAIL] " << message << " - Unexpected exception type" \
                  << " (" << __FILE__ << ":" << __LINE__ << ")" << std::endl; \
        failed_tests++; \
        return false; \
    } \
    if (!exception_caught) { \
        std::cerr << "[FAIL] " << message << " - Expected exception not thrown" \
                  << " (" << __FILE__ << ":" << __LINE__ << ")" << std::endl; \
        failed_tests++; \
        return false; \
    } \
} while(0)

// Вспомогательная функция для сравнения текста
bool compareText(const char* expected, const char* actual, size_t len) {
    if (!expected && !actual) return true;
    if (!expected || !actual) return false;
    return std::memcmp(expected, actual, len) == 0;
}

// Создание текста с кириллицей (БЕЗ \n в конце последней строки!)
std::string createCyrillicText(int lineCount) {
    std::string result;
    for (int i = 0; i < lineCount; ++i) {
        result += "Строка " + std::to_string(i + 1) + " с кириллицей: привет мир!";
        if (i < lineCount - 1) {
            result += '\n';
        }
    }
    return result;
}

// Тест 1: Пустое дерево
bool testEmptyTreeOperations() {
    Tree tree;
    ASSERT(tree.isEmpty(), "Tree should be empty initially");
    ASSERT(tree.getRoot() == nullptr, "Root should be null for empty tree");
    ASSERT(tree.getTotalLineCount() == 0, "Line count should be 0 for empty tree");

    // Попытка получить несуществующие данные
    ASSERT_THROW(tree.getOffsetForLine(0), std::out_of_range, "Should throw for offset in empty tree");
    ASSERT(tree.getLine(0) == nullptr, "getLine(0) should return null for empty tree");
    ASSERT(tree.getTextRange(0, 10) == nullptr, "getTextRange should return null for empty tree");
    ASSERT(tree.findSubstring("test", 4) == -1, "findSubstring should return -1 for empty tree");

    // Безопасная очистка
    tree.clear();
    ASSERT(tree.isEmpty(), "Tree should be empty after clear");

    return true;
}

// Тест 2: Инициализация из текста
bool testFromTextInitialization() {
    // ASCII текст
    const char asciiText[] = "Hello\nWorld\nTest";
    Tree asciiTree;
    asciiTree.fromText(asciiText, strlen(asciiText));
    ASSERT(!asciiTree.isEmpty(), "Tree should not be empty after initialization");
    ASSERT_EQUAL(asciiTree.getTotalLineCount(), 3, "Line count mismatch for ASCII text");

    char* asciiResult = asciiTree.toText();
    ASSERT(asciiResult != nullptr, "toText() returned null for ASCII text");
    ASSERT(compareText(asciiText, asciiResult, strlen(asciiText)), "ASCII text mismatch");
    delete[] asciiResult;

    // UTF-8 текст с кириллицей
    std::string cyrillic = createCyrillicText(5);
    Tree cyrTree;
    cyrTree.fromText(cyrillic.c_str(), cyrillic.size());
    ASSERT_EQUAL(cyrTree.getTotalLineCount(), 5, "Line count mismatch for Cyrillic text");

    char* cyrResult = cyrTree.toText();
    ASSERT(cyrResult != nullptr, "toText() returned null for Cyrillic text");
    ASSERT(compareText(cyrillic.c_str(), cyrResult, cyrillic.size()), "Cyrillic text mismatch");
    delete[] cyrResult;

    // Очень длинный текст (проверка разбиения на листья)
    std::string longText(100000, 'A'); // 100KB
    longText.replace(50000, 5, "\nBBB\n"); // Вставляем разделители
    Tree longTree;
    longTree.fromText(longText.c_str(), longText.size());

    char* longResult = longTree.toText();
    ASSERT(longResult != nullptr, "toText() returned null for long text");
    ASSERT(compareText(longText.c_str(), longResult, longText.size()), "Long text mismatch");
    delete[] longResult;

    return true;
}

// Тест 3: Вставка и удаление
bool testInsertAndEraseOperations() {
    Tree tree;
    std::string base = "Line1\nLine2\nLine3"; // Без \n в конце!
    tree.fromText(base.c_str(), base.size());
    
    // Вставка в начало (кириллица)
    std::string prefix = "Начало: ";
    tree.insert(0, prefix.c_str(), prefix.size());
    ASSERT_EQUAL(tree.getTotalLineCount(), 3, "Line count should remain 3 after prefix insertion");
    
    // Вставка в середину (разделение строки)
    std::string middle = " (вставка) ";
    int offset = tree.getOffsetForLine(1) + 3; // После "Line"
    tree.insert(offset, middle.c_str(), middle.size());
    
    // Вставка в конец
    std::string suffix = "\nКонец текста.";
    tree.insert(tree.getRoot()->getLength(), suffix.c_str(), suffix.size());
    ASSERT_EQUAL(tree.getTotalLineCount(), 4, "Line count should be 4 after suffix insertion");
    
    // Проверка полного текста
    char* fullText = tree.toText();
    std::string expected = prefix + "Line1\nLine2 (вставка) \nLine3" + suffix;
    ASSERT(fullText != nullptr, "toText() returned null after insertions");
    ASSERT(compareText(expected.c_str(), fullText, expected.size()), "Text mismatch after insertions");
    delete[] fullText;
    
    // Удаление части с кириллицей
    int eraseStart = prefix.size() + 1; // После "Начало: L"
    tree.erase(eraseStart, 5); // Удаляем "ine1"
    
    // Проверка после удаления
    char* afterErase = tree.toText();
    std::string expectedAfterErase = prefix + "L\nLine2 (вставка) \nLine3" + suffix;
    ASSERT(afterErase != nullptr, "toText() returned null after erasure");
    ASSERT(compareText(expectedAfterErase.c_str(), afterErase, expectedAfterErase.size()), "Text mismatch after erasure");
    delete[] afterErase;
    
    // Удаление за границами (должно выбросить исключение)
    ASSERT_THROW(tree.erase(-1, 1), std::out_of_range, "Should throw for negative erase position");
    int totalLen = tree.getRoot()->getLength();
    ASSERT_THROW(tree.erase(totalLen + 10, 5), std::out_of_range, "Should throw for erase beyond text end");
    
    return true;
}

// Тест 4: Получение строк по номеру
bool testGetLineFunctionality() {
    std::string cyrillic = createCyrillicText(10);
    Tree tree;
    tree.fromText(cyrillic.c_str(), cyrillic.size());
    
    // Проверка каждой строки
    for (int i = 0; i < 10; ++i) {
        char* line = tree.getLine(i);
        ASSERT(line != nullptr, ("Line " + std::to_string(i) + " should not be null").c_str());
        
        std::string expected = "Строка " + std::to_string(i + 1) + " с кириллицей: привет мир!";
        ASSERT(strcmp(line, expected.c_str()) == 0, 
               ("Line " + std::to_string(i) + " content mismatch. Expected: " + expected + ", Actual: " + line).c_str());
        delete[] line;
    }
    
    // Попытка получить несуществующую строку
    ASSERT(tree.getLine(-1) == nullptr, "getLine(-1) should return null");
    ASSERT(tree.getLine(10) == nullptr, "getLine(10) should return null for 10 lines (0-based index 0..9)");
    
    // Строка с только \n
    Tree newlineTree;
    newlineTree.fromText("\n\n\n", 3);
    ASSERT_EQUAL(newlineTree.getTotalLineCount(), 4, "Line count mismatch for newline-only text");
    
    char* emptyLine = newlineTree.getLine(1);
    ASSERT(emptyLine != nullptr, "Empty line should not be null");
    ASSERT(strcmp(emptyLine, "") == 0, "Empty line content should be empty string");
    delete[] emptyLine;
    
    return true;
}

// Тест 5: Получение смещения для строки (теперь только ASCII для однозначности)
bool testGetOffsetForLine() {
    // Используем ASCII текст для однозначных ожиданий
    std::string text = "First line\nSecond line\nThird line";
    Tree tree;
    tree.fromText(text.c_str(), text.size());
    
    ASSERT_EQUAL(tree.getOffsetForLine(0), 0, "Offset for line 0 mismatch");
    ASSERT_EQUAL(tree.getOffsetForLine(1), 11, "Offset for line 1 mismatch"); // Длина "First line\n" = 11
    ASSERT_EQUAL(tree.getOffsetForLine(2), 23, "Offset for line 2 mismatch"); // 11 + длина "Second line\n" = 12
    
    // Исключения для неверных индексов
    ASSERT_THROW(tree.getOffsetForLine(-1), std::out_of_range, "Should throw for negative line index");
    ASSERT_THROW(tree.getOffsetForLine(3), std::out_of_range, "Should throw for line index beyond count");
    
    return true;
}

// Тест 6: Поиск подстроки (ASCII версия для однозначности)
bool testFindSubstring() {
    // Чистый ASCII текст для однозначных позиций
    std::string text = "Hello world\nThis is a test\nAnother line\nFinal test";
    Tree tree;
    tree.fromText(text.c_str(), text.size());
    
    // Поиск подстрок
    ASSERT_EQUAL(tree.findSubstring("world", 5), 6, "First substring position mismatch");
    ASSERT_EQUAL(tree.findSubstring("test", 4), 23, "Second substring position mismatch"); // После "This is a "
    ASSERT_EQUAL(tree.findSubstring("Another", 7), 34, "Third substring position mismatch");
    ASSERT_EQUAL(tree.findSubstring("Final", 5), 46, "Fourth substring position mismatch");
    
    // Не найдено
    ASSERT_EQUAL(tree.findSubstring("missing", 7), -1, "Non-existent substring should return -1");
    
    // Граничные случаи
    ASSERT_EQUAL(tree.findSubstring("", 0), -1, "Empty pattern should return -1");
    ASSERT_EQUAL(tree.findSubstring(text.c_str(), text.size()), 0, "Full text pattern should match at position 0");
    
    return true;
}

// Тест 7: Получение диапазона текста
bool testGetTextRange() {
    std::string cyrillic = createCyrillicText(3);
    Tree tree;
    tree.fromText(cyrillic.c_str(), cyrillic.size());
    
    // Полный текст
    char* fullRange = tree.getTextRange(0, cyrillic.size());
    ASSERT(fullRange != nullptr, "Full text range should not be null");
    ASSERT(compareText(cyrillic.c_str(), fullRange, cyrillic.size()), "Full text range mismatch");
    delete[] fullRange;
    
    // Частичный диапазон (кириллица)
    int start = 15;
    int len = 30;
    char* partial = tree.getTextRange(start, len);
    ASSERT(partial != nullptr, "Partial text range should not be null");
    
    std::string expected(cyrillic, start, len);
    ASSERT(compareText(expected.c_str(), partial, len), "Partial text range mismatch");
    delete[] partial;
    
    // Запрос за пределами текста
    int totalLen = tree.getRoot()->getLength();
    char* endRange = tree.getTextRange(totalLen - 5, 10);
    ASSERT(endRange != nullptr, "End text range should not be null");
    if (endRange) {
        std::string expectedEnd(cyrillic, totalLen - 5, 5);
        ASSERT(compareText(expectedEnd.c_str(), endRange, 5), "End text range mismatch");
        delete[] endRange;
    }
    
    // Исключения для неверных параметров
    ASSERT_THROW(tree.getTextRange(-1, 5), std::out_of_range, "Should throw for negative offset");
    ASSERT_THROW(tree.getTextRange(totalLen + 10, 5), std::out_of_range, "Should throw for offset beyond text end");
    
    return true;
}

// Тест 9: Стресс-тест с кириллицей
bool testStressWithCyrillic() {
    // Генерируем 100 строк кириллицы (~10KB) БЕЗ \n в конце последней строки
    std::string largeText;
    for (int i = 0; i < 100; ++i) {
        largeText += "Строка " + std::to_string(i) + 
                     ": длинный текст с кириллицей и символами !@#$%^&*()";
        if (i < 99) {
            largeText += '\n';
        }
    }
    
    Tree tree;
    tree.fromText(largeText.c_str(), largeText.size());
    ASSERT_EQUAL(tree.getTotalLineCount(), 100, "Initial line count mismatch");
    
    // Проверка получения строк
    for (int i = 0; i < 100; i += 10) {
        char* line = tree.getLine(i);
        ASSERT(line != nullptr, ("Stress test: Line " + std::to_string(i) + " is null").c_str());
        delete[] line;
    }
    
    // Случайные операции вставки/удаления
    std::string insert1 = "ВСТАВКА_1_кириллица";
    tree.insert(500, insert1.c_str(), insert1.size());
    largeText.insert(500, insert1);
    
    std::string insert2 = "СРЕДИНА_ТЕКСТА";
    int midPos = largeText.size() / 2;
    tree.insert(midPos, insert2.c_str(), insert2.size());
    largeText.insert(midPos, insert2);
    
    // Удаление фрагментов
    tree.erase(1000, 50);
    largeText.erase(1000, 50);
    
    // Проверка финального состояния
    char* result = tree.toText();
    ASSERT(result != nullptr, "Stress test: toText() returned null");
    ASSERT(compareText(largeText.c_str(), result, largeText.size()), "Stress test: Text mismatch");
    delete[] result;
    
    // Поиск подстроки в модифицированном тексте
    std::string search = "длинный текст с кириллицей";
    int offset = tree.findSubstring(search.c_str(), search.size());
    ASSERT(offset >= 0, "Substring not found in stress test");
    ASSERT(offset < tree.getRoot()->getLength(), "Substring position beyond text length");
    
    return true;
}

// Основная функция запуска тестов
int main() {
    std::cout << "=== Starting Tree Unit Tests ===" << std::endl;
    
    bool (*testFunctions[])() = {
        testEmptyTreeOperations,
        testFromTextInitialization,
        testInsertAndEraseOperations,
        testGetLineFunctionality,
        testGetOffsetForLine,
        testFindSubstring,
        testGetTextRange,
        testStressWithCyrillic
    };
    
    int numTests = sizeof(testFunctions) / sizeof(testFunctions[0]);
    total_tests = 0;
    failed_tests = 0;
    
    for (int i = 0; i < numTests; i++) {
        std::cout << "Running test " << (i+1) << "/" << numTests << ": ";
        bool result = testFunctions[i]();
        if (result) {
            std::cout << "PASSED" << std::endl;
        } else {
            std::cout << "FAILED" << std::endl;
        }
    }
    
    std::cout << "\n=== Test Results ===" << std::endl;
    std::cout << "Total tests: " << total_tests << std::endl;
    std::cout << "Passed: " << (total_tests - failed_tests) << std::endl;
    std::cout << "Failed: " << failed_tests << std::endl;
    
    return failed_tests == 0 ? 0 : 1;
}