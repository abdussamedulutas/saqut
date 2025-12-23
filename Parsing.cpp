#include <iostream>
#include <string>
#include <stdlib.h>
#include "./core/Tokenizer.cpp"

int main()
{
    std::string girdi; 

    std::cout << "\nsaQut Compiler\n\n";

    while(true)
    {
        std::cout << ">> ";
        std::getline(std::cin, girdi);

        Tokenizer token;
        token.parse(girdi);

        if (girdi == ".exit")
        {
            exit(0);
        };
        std::cout << "\n";
    }

    return 0;
}