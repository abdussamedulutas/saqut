#include <iostream>
#include <fstream>
#include <sstream>
#include <string>
#include <stdlib.h>
#include "./core/Tokenizer.cpp"
#include "./core/Parser.cpp"

int main()
{
    std::ifstream dosyaOku("source.sqt", std::ios::in | std::ios::binary);
    std::string icerik;

    if (dosyaOku.is_open()) {
        std::stringstream buffer;
        buffer << dosyaOku.rdbuf(); // Dosya içeriğini buffer'a boşalt
        icerik = buffer.str();
        dosyaOku.close();
    }

    Tokenizer tokenizer;
    Parser parser;
    
    auto tokens = tokenizer.scan(icerik);
    parser.parse(tokens);


    return 0;
}