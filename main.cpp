#include "Vector.h"
#include "Cryption.h"
#include "Helpers.h"

#include <iostream>
#include <iomanip>
#include <vector>
#include <list>
#include <fstream>
#include <string>

int main()
{
    bool exit = false;
    Vault vault;

        std::cout << std::setw(70) << "Password Manager Loaded\n";
        //list cmds
        vault.displayCmds();

        //for storing variable user input for parsing
        Vector<char> userInput;


        while (!exit)
        {
            userInput.clear();

            char c;
            while (std::cin.get(c))
            {
                if (c == '\n')
                    break;

                userInput.push_back(c);
            }

            userInput.push_back('\0');

            char* usernPut = userInput.data();

            Helpers::parseUserInput(usernPut, vault);
        }




    return 0;
}