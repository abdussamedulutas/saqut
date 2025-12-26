#include <iostream>
#include <string>
#include <stdlib.h>
#include "./Tokenizer.cpp"

#ifndef PARSER
#define PARSER

std::string padRight(std::string str, size_t totalLen) {
    if (str.size() < totalLen) {
        str.append(totalLen - str.size(), ' ');
    }
    return str;
}

class Parser {
    public:
    void parse(std::vector<Token> tokens)
    {
        for(Token token : tokens)
        {
            std::cout << padRight(token.token,20) << token.gettype() << "\n";
        }
    }
};


#endif