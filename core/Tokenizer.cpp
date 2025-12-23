#include <iostream>
#include <string>
#include <stdlib.h>
#include <vector>

#include "./Lexer.cpp"

class Token {
    private:
        std::string type = "";
    public:
        int start = 0;
        int end = 0;
        std::string token;
};

class StringToken : public Token {
    private:
        std::string type = "string";
    public:
        std::string context;
        int size = 0;
        void log()
        {
            std::cout << "Token String{" << this->token<<"} Start=" <<  this->start  << " End=" <<  this->end << " Context{"<< this->context << "} Size="<< this->context.size() <<"\n";
        }
};
class NumberToken : public Token {
    private:
        std::string type = "number";
    public:
        bool isFloat = false;
        bool hasEpsilon = false;
        int base = 10;
        void log()
        {
            std::cout << "NumberToken "<< (this->isFloat ? "Float" : "Integer") <<"{" << this->token << "} HasExponent="<< (this->hasEpsilon ? "Yes" : "No") << " Base=" << this->base << " Start=" <<  this->start  << " End=" <<  this->end << "\n";
        }
};
class BoolToken : public Token {
    private:
        std::string type = "boolean";
    public:
        void log()
        {
            std::cout << "BoolToken Value{"<<this->token<<"} Start=" <<  this->start  << " End=" <<  this->end << " \n";
        }
};
class OperatorToken : public Token {
    private:
        std::string type = "operator";
    public:
        void log()
        {
            std::cout << "OperatorToken Context{"<<this->token<<"} Start=" <<  this->start  << " End=" <<  this->end << " \n";
        }
};
class DelimiterToken : public Token {
    private:
        std::string type = "delimiter";
    public:
        void log()
        {
            std::cout << "DelimiterToken Context{"<<this->token<<"} Start=" <<  this->start  << " End=" <<  this->end << " \n";
        }
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


class Tokenizer {
public:
    Lexer hmx;
    void parse(std::string input)
    {
        this->hmx.setText(input);
        this->scope();
    }
    void scope()
    {
        this->hmx.skipWhiteSpace();
        // Stringler
        if(this->hmx.getchar() == '"')
        {
            StringToken t = this->readString();
            t.log();
            return;
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
            numberToken.log();
            return;
        }

        // Boolean
        if(this->hmx.include("true",false))
        {
            BoolToken BoolToken;
            BoolToken.token = "true";
            BoolToken.start = this->hmx.getOffset();
            this->hmx.toChar(+4);
            BoolToken.end = this->hmx.getOffset();
            BoolToken.log();
            return;
        }
        if(this->hmx.include("false",false))
        {
            BoolToken BoolToken;
            BoolToken.token = "false";
            BoolToken.start = this->hmx.getOffset();
            this->hmx.toChar(+5);
            BoolToken.end = this->hmx.getOffset();
            BoolToken.log();
            return;
        }

        for (const std::string_view& del : delimiters) {
            if(this->hmx.include(std::string(del),false))
            {
                DelimiterToken dtoken;
                dtoken.start = this->hmx.getOffset();
                this->hmx.toChar(+del.size());
                dtoken.end = this->hmx.getOffset();
                dtoken.token = del;
                dtoken.log();
                return;
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
                optoken.log();
                return;
            }
        }
    }
    StringToken readString()
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
};