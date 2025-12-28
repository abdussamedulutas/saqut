#include <iostream>
#include <string>
#include <vector>

#ifndef LEXER
#define LEXER
struct INumber {
    int start = 0;
    int end = 0;
    std::string token;
    bool isFloat = false;
    bool hasEpsilon = false;
    int base = 10;
    bool positive = true;
};

class Lexer {
public:
    std::string input = "";
    int size = 0;
    int offset = 0;
    std::vector<int> offsetMap;
    void beginPosition();
    int getLastPosition();
    void acceptPosition();
    void setLastPosition(int);
    bool isEnd();
    void rejectPosition();
    int * positionRange();
    std::string getPositionRange();
    bool include(std::string,bool);
    int getOffset();
    int setOffset(int);
    char getchar(int);
    char getchar();
    void nextChar();
    void toChar(int);
    void setText(std::string);
    void skipWhiteSpace();
    bool isNumeric();
    INumber readNumeric();
};

void Lexer::beginPosition()
{
    this->offsetMap.push_back(this->getLastPosition());
}


int Lexer::getLastPosition()
{
    if(this->offsetMap.size() == 0)
    {
        return this->offset;
    }
    else
    {
        return this->offsetMap[this->offsetMap.size() - 1];
    }
}
void Lexer::acceptPosition()
{
    int T = this->offsetMap[this->offsetMap.size() - 1];
    this->setLastPosition(T);
}
void Lexer::setLastPosition(int n)
{
    if(this->offsetMap.size() == 0)
    {
        this->offset = n;
    }
    else
    {
        this->offsetMap[this->offsetMap.size() - 1] = n;
    }
}
bool Lexer::isEnd()
{
    bool result = this->size <= this->getOffset();
    return result;
}
void Lexer::rejectPosition()
{
    this->offsetMap.pop_back();
}
int * Lexer::positionRange()
{
    int len = this->offsetMap.size();
    if(len == 0)
    {
        return new int[2]{0, this->offset};
    }
    else if(len == 1)
    {
        return new int[2]{
            this->offset,
            this->offsetMap[len - 1]
        };
    }else{
        return new int[2]{
            this->offsetMap[len - 2],
            this->offsetMap[len - 1]
        };
    }
}
std::string Lexer::getPositionRange()
{
    int *A = this->positionRange();
    std::string mem;

    for (int i = A[0]; i < A[1];i++)
    {
        mem.push_back(this->input.at(i));
    }
    return mem;
}

bool Lexer::include(std::string word,bool accept = true)
{
    this->beginPosition();
    for (int i = 0; i < word.size(); i++)
    {
        if(this->isEnd())
        {
            if(word.size() == i)
            { 
                break;
            }else{
                this->rejectPosition();
                return false;
            }
        }
        if(word.at(i) != this->getchar())
        {
            this->rejectPosition();
            return false;
        }
        this->nextChar();
    }
    if(accept)
    {
        this->acceptPosition();
    }
    else
    {
        this->rejectPosition();
    };
    return true;
}
int Lexer::getOffset()
{
    return this->getLastPosition();
}
int Lexer::setOffset(int n)
{
    this->setLastPosition(n);
    return this->getLastPosition();
}
char Lexer::getchar(int additionalOffset)
{
    int target = this->getOffset() + additionalOffset;
    if(this->size - 1 < target)
    {
        std::cerr << "Hata yanlış erişim\n";
        return '\0';
    }else{
        return this->input.at(target);
    }
}
char Lexer::getchar()
{
    int target = this->getOffset();
    if(this->size - 1 < target)
    {
        std::cerr << "Hata yanlış erişim\n";
        return '\0';
    }else{
        return this->input.at(target);
    }
}
void Lexer::nextChar()
{
    if(this->isEnd() == true)
    {
        return;
    };
    this->setOffset(this->getOffset() + 1);
}
void Lexer::toChar(int n)
{
    if(this->isEnd() == true)
    {
        return;
    };
    this->setOffset(this->getOffset() + n);
}
void Lexer::setText(std::string input) {
    this->input = input;
    this->size = input.length();
}
void Lexer::skipWhiteSpace()
{
    while(this->isEnd() == false)
    {
        switch(this->getchar())
        { 
            case '\r':
            case '\n': 
            case '\b': 
            case '\t':
            case ' ':{
                this->nextChar();
                break;
            }
            default:{
                return;
            }
        }
    }
}

bool Lexer::isNumeric()
{   
    char c = this->getchar();
    switch (c)
    {
        case '0':
        case '1':
        case '2':
        case '3':
        case '4':
        case '5':
        case '6':
        case '7':
        case '8':
        case '9':{
            return true;
        }
        default:{
            return false;
        }
    }
}
    
INumber Lexer::readNumeric()
{
    INumber numberToken;
    numberToken.start = this->getLastPosition();
    if(this->getchar() == '-')
    {
        this->nextChar();
        numberToken.positive = false;
    }else if(this->getchar() == '+'){
        this->nextChar();
        numberToken.positive = true;
    }else{
        numberToken.positive = true;
    }

    bool nextDot = false;
    if(this->getchar() == '0')
    {
        numberToken.token.push_back('0');
        this->nextChar();
        char c = this->getchar();
        switch(c)
        {
            case 'x':{
                numberToken.token.push_back(c);
                numberToken.base = 16;
                break;
            }
            case 'b':{
                numberToken.token.push_back(c);
                numberToken.base = 2;
                break;
            }
            default:{
                if(c != '.')
                {
                    numberToken.token.push_back(c);
                    numberToken.base = 8;
                }else{
                    numberToken.token.push_back(c);
                    numberToken.base = 10;
                    nextDot = true;
                    numberToken.isFloat = true;
                }
                break;
            }
        }
        this->nextChar();
    }else{
        numberToken.base = 10;
    }


    while(this->isEnd() == false)
    {
        char c = this->getchar();
        switch (c)
        {
            case '0':
            case '1':{
                numberToken.token.push_back(c);
                break;
            }
            case '2':
            case '3':
            case '4':
            case '5':
            case '6':
            case '7':{
                if(numberToken.base >= 8)
                {
                    numberToken.token.push_back(c);
                    break;
                }else{
                    numberToken.end = this->getLastPosition();
                    return numberToken;
                }
            }
            case '8':
            case '9':{
                if(numberToken.base >= 10)
                {
                    numberToken.token.push_back(c);
                    break;
                }else{
                    numberToken.end = this->getLastPosition();
                    return numberToken;
                }
            }
            case 'a': case 'A':
            case 'b': case 'B':
            case 'c': case 'C':
            case 'd': case 'D':
            case 'f': case 'F':{
                if(numberToken.base >= 16)
                {
                    numberToken.token.push_back(c);
                    break;
                }else{
                    numberToken.end = this->getLastPosition();
                    return numberToken;
                }
            }
            case '.':{
                if(nextDot == false)
                {
                    if(numberToken.token.size() == 0)
                    {
                        numberToken.token.push_back('0');
                        numberToken.token.push_back('.');
                    }else{
                        numberToken.token.push_back('.');
                    }
                    nextDot = true;
                    numberToken.isFloat = true;
                    break;
                }else{
                    numberToken.end = this->getLastPosition();
                    return numberToken;
                }
            }
            case 'e':case 'E':{
                if(numberToken.base == 16)
                {
                    numberToken.token.push_back(c);
                    break;
                }
                if(numberToken.base == 10)
                {
                    numberToken.hasEpsilon = true;
                    numberToken.token.push_back(c);
                    this->nextChar();
                    c = this->getchar();

                    if(c == '+' || c == '-')
                    {
                        numberToken.token.push_back(c);
                        this->nextChar();
                    }

                    while(this->isEnd() == false)
                    {
                        char c = this->getchar();
                        switch (c)
                        {
                            case '0':
                            case '1':
                            case '2':
                            case '3':
                            case '4':
                            case '5':
                            case '6':
                            case '7':
                            case '8':
                            case '9':{
                                numberToken.token.push_back(c);
                                break;
                            }
                            default:{
                                numberToken.end = this->getLastPosition();
                                return numberToken;
                            }
                        }
                        this->nextChar();
                    }
                    break;
                }
                numberToken.end = this->getLastPosition();
                return numberToken;
            }
            default:{
                numberToken.end = this->getLastPosition();
                return numberToken;
            }
        }
        this->nextChar();
    }
    numberToken.end = this->getLastPosition();
    return numberToken;
};
#endif