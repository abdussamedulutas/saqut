#ifndef SAQUT_LEXER
#define SAQUT_LEXER

#include <iostream>
#include <string>
#include <vector>

struct INumber {
    int start = 0;
    int end   = 0;
    std::string token;
    bool isFloat    = false;
    bool hasEpsilon = false;
    int base        = 10;
    bool positive   = true;
};

class Lexer {
public:
    std::string input;
    int size   = 0;
    int offset = 0;
    std::vector<int> offsetMap;

    // --- Position tracking ---
    void beginPosition();
    int  getLastPosition();
    void acceptPosition();
    void setLastPosition(int n);
    void rejectPosition();

    // --- Reading ---
    bool isEnd();
    int* positionRange();
    std::string getPositionRange();
    bool include(std::string word, bool accept = true);

    int  getOffset();
    int  setOffset(int n);
    char getchar(int additionalOffset);
    char getchar();
    void nextChar();
    void toChar(int n);

    void setText(std::string input);
    void skipWhiteSpace();
    bool isNumeric();
    INumber readNumeric();
};

// ============================================================
// Implementation
// ============================================================

void Lexer::beginPosition() {
    offsetMap.push_back(getLastPosition());
}

int Lexer::getLastPosition() {
    if (offsetMap.empty()) return offset;
    return offsetMap.back();
}

void Lexer::acceptPosition() {
    int t = offsetMap.back();
    setLastPosition(t);
}

void Lexer::setLastPosition(int n) {
    if (offsetMap.empty())
        offset = n;
    else
        offsetMap.back() = n;
}

bool Lexer::isEnd() {
    return size <= getOffset();
}

void Lexer::rejectPosition() {
    offsetMap.pop_back();
}

int* Lexer::positionRange() {
    int len = offsetMap.size();
    if (len == 0)
        return new int[2]{0, offset};
    if (len == 1)
        return new int[2]{offset, offsetMap[0]};
    return new int[2]{offsetMap[len - 2], offsetMap[len - 1]};
}

std::string Lexer::getPositionRange() {
    int* a = positionRange();
    std::string mem;
    for (int i = a[0]; i < a[1]; i++)
        mem.push_back(input.at(i));
    return mem;
}

bool Lexer::include(std::string word, bool accept) {
    beginPosition();
    for (size_t i = 0; i < word.size(); i++) {
        if (isEnd()) {
            rejectPosition();
            return false;
        }
        if (word[i] != getchar()) {
            rejectPosition();
            return false;
        }
        nextChar();
    }
    if (accept)
        acceptPosition();
    else
        rejectPosition();
    return true;
}

int Lexer::getOffset() {
    return getLastPosition();
}

int Lexer::setOffset(int n) {
    setLastPosition(n);
    return getLastPosition();
}

char Lexer::getchar(int additionalOffset) {
    int target = getOffset() + additionalOffset;
    if (target >= size) {
        std::cerr << "Lexer hatası: sınır aşımı\n";
        return '\0';
    }
    return input.at(target);
}

char Lexer::getchar() {
    int target = getOffset();
    if (target >= size) {
        std::cerr << "Lexer hatası: sınır aşımı\n";
        return '\0';
    }
    return input.at(target);
}

void Lexer::nextChar() {
    if (!isEnd())
        setOffset(getOffset() + 1);
}

void Lexer::toChar(int n) {
    if (!isEnd())
        setOffset(getOffset() + n);
}

void Lexer::setText(std::string text) {
    input = text;
    size  = text.length();
}

void Lexer::skipWhiteSpace() {
    while (!isEnd()) {
        switch (getchar()) {
            case '\r':
            case '\n':
            case '\b':
            case '\t':
            case ' ':
                nextChar();
                break;
            default:
                return;
        }
    }
}

bool Lexer::isNumeric() {
    char c = getchar();
    return (c >= '0' && c <= '9');
}

INumber Lexer::readNumeric() {
    INumber num;
    num.start = getLastPosition();

    if (getchar() == '-') {
        nextChar();
        num.positive = false;
    } else if (getchar() == '+') {
        nextChar();
        num.positive = true;
    } else {
        num.positive = true;
    }

    bool nextDot = false;
    if (getchar() == '0') {
        num.token.push_back('0');
        nextChar();
        char c = getchar();
        switch (c) {
            case 'x':
                num.token.push_back(c);
                num.base = 16;
                break;
            case 'b':
                num.token.push_back(c);
                num.base = 2;
                break;
            default:
                if (c != '.') {
                    num.token.push_back(c);
                    num.base = 8;
                } else {
                    num.token.push_back(c);
                    num.base   = 10;
                    nextDot    = true;
                    num.isFloat = true;
                }
                break;
        }
        nextChar();
    } else {
        num.base = 10;
    }

    while (!isEnd()) {
        char c = getchar();
        switch (c) {
            case '0':
            case '1':
                num.token.push_back(c);
                break;
            case '2': case '3': case '4': case '5':
            case '6': case '7':
                if (num.base >= 8)
                    num.token.push_back(c);
                else {
                    num.end = getLastPosition();
                    return num;
                }
                break;
            case '8': case '9':
                if (num.base >= 10)
                    num.token.push_back(c);
                else {
                    num.end = getLastPosition();
                    return num;
                }
                break;
            case 'a': case 'A': case 'b': case 'B':
            case 'c': case 'C': case 'd': case 'D':
            case 'f': case 'F':
                if (num.base >= 16)
                    num.token.push_back(c);
                else {
                    num.end = getLastPosition();
                    return num;
                }
                break;
            case '.':
                if (!nextDot) {
                    if (num.token.empty())
                        num.token += "0.";
                    else
                        num.token.push_back('.');
                    nextDot    = true;
                    num.isFloat = true;
                } else {
                    num.end = getLastPosition();
                    return num;
                }
                break;
            case 'e': case 'E':
                if (num.base == 16) {
                    num.token.push_back(c);
                    break;
                }
                if (num.base == 10) {
                    num.hasEpsilon = true;
                    num.token.push_back(c);
                    nextChar();
                    c = getchar();
                    if (c == '+' || c == '-') {
                        num.token.push_back(c);
                        nextChar();
                    }
                    while (!isEnd()) {
                        c = getchar();
                        if (c >= '0' && c <= '9') {
                            num.token.push_back(c);
                            nextChar();
                        } else {
                            num.end = getLastPosition();
                            return num;
                        }
                    }
                    break;
                }
                num.end = getLastPosition();
                return num;
            default:
                num.end = getLastPosition();
                return num;
        }
        nextChar();
    }
    num.end = getLastPosition();
    return num;
}

#endif
