#include "../src/Tree.h"
#include <iostream>
#include <string>
#include <cassert>
#include <cstring>
#include <functional>

// helper: compare tree->toText() with expected std::string, free returned buffer
bool compareTreeText(Tree& t, const std::string& expected) {
    char* out = t.toText();
    bool ok = (out != nullptr) && (std::string(out) == expected);
    delete[] out;
    return ok;
}

// helper: compare getLine result and free
bool compareGetLine(Tree& t, int lineIndex, const std::string& expected) {
    char* out = t.getLine(lineIndex);
    bool ok = (out != nullptr) && (std::string(out) == expected);
    if (out) delete[] out;
    return ok;
}

// make string of repeated char count times
std::string repeat(char c, int count) {
    if (count <= 0) return std::string();
    return std::string((size_t)count, c);
}

int main() {
    int fails = 0;
    auto run = [&](const std::string& name, const std::function<void()>& fn) {
        try {
            fn();
            std::cout << "[ OK ] " << name << "\n";
        } catch (const std::string& e) {
            ++fails;
            std::cout << "[FAIL] " << name << " : " << e << "\n";
        } catch (...) {
            ++fails;
            std::cout << "[FAIL] " << name << " : unknown exception\n";
        }
    };

    // TEST 1: fromText / toText roundtrip
    run("fromText/toText roundtrip", [&]() {
        Tree t;
        const char* txt = "Hello\nWorld\nLine3";
        t.fromText(txt, (int)std::strlen(txt));
        if (!compareTreeText(t, std::string(txt))) throw std::string("roundtrip mismatch");
    });

    // TEST 2: getLine basic
    run("getLine basic", [&]() {
        Tree t;
        const char* txt = "one\ntwo\nthree\n";
        t.fromText(txt, (int)std::strlen(txt));
        if (!compareGetLine(t, 0, "one")) throw std::string("line0");
        if (!compareGetLine(t, 1, "two")) throw std::string("line1");
        if (!compareGetLine(t, 2, "three")) throw std::string("line2");
        // last trailing newline -> depending on your semantics getLine(3) may be empty or invalid.
        // We just check that earlier lines are correct.
    });

    // TEST 3: insert at beginning / middle / end
    run("insert begin/mid/end", [&]() {
        Tree t;
        t.fromText("abc", 3);
        t.insert(0, "X", 1); // "Xabc"
        if (!compareTreeText(t, "Xabc")) throw std::string("insert begin");
        t.insert(2, "Y", 1); // "XaYbc"
        if (!compareTreeText(t, "XaYbc")) throw std::string("insert middle");
        t.insert(t.getRoot()->getLength(), "Z", 1); // append
        if (!compareTreeText(t, "XaYbcZ")) throw std::string("insert end");
    });

    // TEST 4: erase begin/mid/end
    run("erase begin/mid/end", [&]() {
        Tree t;
        const char* s = "0123456789";
        t.fromText(s, (int)std::strlen(s));
        t.erase(0, 1); // remove '0' -> "123456789"
        if (!compareTreeText(t, "123456789")) throw std::string("erase begin");
        t.erase(3, 2); // remove at pos 3 -> originally "123456789", pos3='4', remove '4','5' -> "1236789"
        if (!compareTreeText(t, "1236789")) throw std::string("erase middle");
        t.erase(t.getRoot()->getLength()-1, 1); // remove last char -> "123678"
        if (!compareTreeText(t, "123678")) throw std::string("erase end");
    });

    // TEST 5: insert that triggers split (large insert)
    run("insert causing split", [&]() {
        Tree t;
        t.fromText("A", 1);
        int big = MAX_LEAF_SIZE + 100;
        std::string bigs = repeat('B', big);
        t.insert(1, bigs.c_str(), big); // A + bigB
        // result should be "A" + bigs
        std::string expect = std::string("A") + bigs;
        if (!compareTreeText(t, expect)) throw std::string("insert split content mismatch");
    });

    // TEST 6: erase across leaf boundary
    run("erase across leaf boundary", [&]() {
        // create two blocks each > small so they become separate leaves via buildFromTextRecursive logic
        std::string s1 = repeat('X', MAX_LEAF_SIZE - 100); // slightly smaller than MAX, but build may split; safe enough
        std::string s2 = repeat('Y', MAX_LEAF_SIZE - 100);
        std::string combined = s1 + s2;
        Tree t;
        t.fromText(combined.c_str(), (int)combined.size());
        int pos = (int)s1.size() - 50; // start near end of s1
        int del = 200;                 // cross into s2
        // Prepare expected manually
        std::string expect = combined.substr(0, pos) + combined.substr(pos + del);
        t.erase(pos, del);
        if (!compareTreeText(t, expect)) throw std::string("erase across boundary mismatch");
    });

    // TEST 7: many small inserts + rebalance (content must stay identical)
    run("many small inserts + rebalance", [&]() {
        Tree t;
        std::string accum;
        for (int i = 0; i < 500; ++i) {
            std::string piece = "p" + std::to_string(i) + ";";
            int pos = t.getRoot() ? t.getRoot()->getLength() : 0;
            t.insert(pos, piece.c_str(), (int)piece.size());
            accum += piece;
        }
        // call rebalance; content should be preserved
        t.rebalance();
        if (!compareTreeText(t, accum)) throw std::string("many inserts content mismatch");
    });

    // TEST 8: erase whole text -> root becomes nullptr or empty
    run("erase whole text", [&]() {
        Tree t;
        t.fromText("hello world", 11);
        t.erase(0, 11);
        // tree should be empty
        if (!t.isEmpty() && (t.getRoot() != nullptr)) {
            // if not empty, toText should be empty string
            if (!compareTreeText(t, "")) throw std::string("erase whole mismatch");
        }
    });

    // Summary
    if (fails == 0) {
        std::cout << "\nALL TESTS PASSED\n";
        return 0;
    } else {
        std::cout << "\nFAILED TESTS: " << fails << "\n";
        return 1;
    }
}
