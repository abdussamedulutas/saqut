#include <iostream>
#include <string>
#include <stdlib.h>
#include <vector>
#include "./Lexer.cpp"

#ifndef TOKENIZER
#define TOKENIZER

class Token {
    protected:
        std::string type = "";
    public:
        int start = 0;
        int end = 0;
        std::string token;
        std::string gettype(){
            return this->type;
        }
};

class StringToken : public Token {
    public:
        StringToken(){
            this->type = "string";
        };
        std::string context;
        int size = 0;
};
class NumberToken : public Token {
    public:
        NumberToken(){
            this->type = "number";
        }
        bool isFloat = false;
        bool hasEpsilon = false;
        int base = 10;
};
class OperatorToken : public Token {
    public:
        OperatorToken(){
            this->type = "operator";
        }
};
class DelimiterToken : public Token {
    public:
        DelimiterToken(){
            this->type = "delimiter";
        }
};
class KeywordToken : public Token {
    public:
        KeywordToken(){
            this->type = "keyword";
        }
};
class IdentifierToken : public Token {
    public:
        IdentifierToken(){
            this->type = "identifier";
        }
        std::string context;
        int size = 0;
};


const constexpr std::string_view operators[] = {
    // --- Mantıksal Karşılaştırma ---
    "==", "!=", "<=", ">=", "&&", "||",

    // --- Aritmetik (Çift Karakterli) ---
    "++", "--", "<<", ">>",

    // --- Atama Operatörleri ---
    "+=", "-=", "*=", "/=", "%=", "&=", "|=", "^=",

    // --- Aritmetik (Tek Karakterli) ---
    "+", "-", "*", "/", "%", "<", ">",

    // --- Bitwise ve Mantıksal (Tek Karakterli) ---
    "^", "!", "~", "&", "|",

    // --- Temel Atama ---
    "="
};

const constexpr std::string_view delimiters[] = {
    // Bağlayıcılar
    "->",
    "::",
    // Sınırlandırıcılar
    "[",
    "]",
    "(",
    ")",
    "{",
    "}",
    // Ayırıcılar
    ";",
    ",",
    ":",
    // Bağlayıcılar
    ".",
};

const constexpr std::string_view keywords[] = {
    "implements",
    "protected",
    "interface",
    "continue",
    "private",
    "finally",
    "extends",
    "default",
    "throws",
    "switch",
    "return",
    "public",
    "assert",
    "false",
    "while",
    "throw",
    "class",
    "catch",
    "break",
    "null",
    "true",
    "enum",
    "else",
    "case",
    "new",
    "try",
    "for",
    "if",
    "do"
};


class Tokenizer {
public:
    Lexer hmx;
    std::vector<Token> scan(std::string input);
    Token scope();
    IdentifierToken readIndetifier();
    StringToken readString();
    void skipOneLineComment();
    void skipMultiLineComment();
};


std::vector<Token> Tokenizer::scan(std::string input)
{
    std::vector<Token> tokens;
    this->hmx.setText(input);
    while(1)
    {
        Token token = this->scope();
        tokens.push_back(token);
        if(this->hmx.isEnd())
        {
            break;
        }
    }
    return tokens;
}
Token Tokenizer::scope()
{
    this->hmx.skipWhiteSpace();

    // Yorum satırları
    if(this->hmx.include("//", true))
    {
        this->skipOneLineComment();
    }
    if(this->hmx.include("/*", true))
    {
        this->skipMultiLineComment();
    }

    if(this->hmx.isEnd()){
        Token token;
        token.token = "EOL";
        return token;
    };

    // Stringler
    if(this->hmx.getchar() == '"')
    {
        return this->readString();
    }

    // Sayılar
    if(this->hmx.isNumeric())
    {
        INumber lem = this->hmx.readNumeric();
        NumberToken numberToken;
        numberToken.base = lem.base;
        numberToken.start = lem.start;
        numberToken.end = lem.end;
        numberToken.hasEpsilon = lem.hasEpsilon;
        numberToken.isFloat = lem.isFloat;
        numberToken.token = lem.token;
        return numberToken;
    }

    for (const std::string_view& keys : keywords) {
        if(this->hmx.include(std::string(keys),false))
        {
            KeywordToken keytoken;
            keytoken.start = this->hmx.getOffset();
            this->hmx.toChar(+keys.size());
            keytoken.end = this->hmx.getOffset();
            keytoken.token = keys;
            return keytoken;
        }
    }

    for (const std::string_view& del : delimiters) {
        if(this->hmx.include(std::string(del),false))
        {
            DelimiterToken dtoken;
            dtoken.start = this->hmx.getOffset();
            this->hmx.toChar(+del.size());
            dtoken.end = this->hmx.getOffset();
            dtoken.token = del;
            return dtoken;
        }
    }

    for (const std::string_view& op : operators) {
        if(this->hmx.include(std::string(op),false))
        {
            OperatorToken optoken;
            optoken.start = this->hmx.getOffset();
            this->hmx.toChar(+op.size());
            optoken.end = this->hmx.getOffset();
            optoken.token = op;
            return optoken;
        }
    }

    return this->readIndetifier();
}
IdentifierToken Tokenizer::readIndetifier()
{
    this->hmx.beginPosition();
    IdentifierToken idenditifierToken;
    idenditifierToken.start = this->hmx.getOffset();

    while(this->hmx.isEnd() == false)
    {
        bool readed = false;
        char c = this->hmx.getchar();

        if(c >= 'a' && c <= 'z')
        {
            readed = true;
            idenditifierToken.token.push_back(c);
            this->hmx.nextChar();
            continue;
        }

        if(c >= 'A' && c <= 'Z')
        {
            readed = true;
            idenditifierToken.token.push_back(c);
            this->hmx.nextChar();
            continue;
        }


        if(c >= '0' && c <= '9')
        {
            readed = true;
            idenditifierToken.token.push_back(c);
            this->hmx.nextChar();
            continue;
        }

        switch(c)
        {
            case '_':{
                readed = true;
                idenditifierToken.token.push_back(c);
                this->hmx.nextChar();
                break;
            }
            case '$':{
                readed = true;
                idenditifierToken.token.push_back(c);
                this->hmx.nextChar();
                break;
            }
        }
        if(readed == false)
        {
            break;
        }
    }
    idenditifierToken.end = this->hmx.getOffset();
    idenditifierToken.size = idenditifierToken.context.size();
    this->hmx.acceptPosition();
    return idenditifierToken;
}
StringToken Tokenizer::readString()
{
    this->hmx.beginPosition();
    StringToken stringToken;
    bool started = false;
    bool isended = false;
    stringToken.start = this->hmx.getOffset();

    while(this->hmx.isEnd() == false)
    {
        char c = this->hmx.getchar();
        stringToken.token.push_back(c);
        switch(c)
        {
            case '"':{
                if(started == false)
                {
                    started = true;
                    break;
                }else{
                    isended = true;
                    break;
                }
            }
            case '\\':{
                this->hmx.nextChar();
                c = this->hmx.getchar();
                stringToken.token.push_back(c);
                stringToken.context.push_back(c);
                break;
            }
            default:{
                stringToken.context.push_back(c);
            }
        }
        this->hmx.nextChar();
        if(isended)
        {
            break;
        }
    }
    stringToken.end = this->hmx.getOffset();
    stringToken.size = stringToken.context.size();
    this->hmx.acceptPosition();
    return stringToken;
}
void Tokenizer::skipOneLineComment()
{
    while(this->hmx.isEnd() == false)
    {
        if(this->hmx.getchar() == '\n')
        {
            this->hmx.nextChar();
            this->hmx.skipWhiteSpace();
            return;
        }else{
            this->hmx.nextChar();
        }
    }
}
void Tokenizer::skipMultiLineComment()
{
    while(this->hmx.isEnd() == false)
    {
        if(this->hmx.include("*/",true))
        {
            this->hmx.skipWhiteSpace();
            return;
        }else{
            this->hmx.nextChar();
        }
    }
}

#endif