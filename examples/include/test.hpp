#include <stdio.h>
#include <string>

bool configOption(std::string a, int b);
bool configVariable(std::string a, int b, char cc);

namespace zzz {
bool config(std::string a, int b, int r);
};

void functionDeclaredInHeader() { 
     int j;
     //REWRITTEN IN HPP (then BRANCH): UNO_HPP
     if ((((configVariable("UseSpanishLanguage", 1,2))))){printf("UNO_HPP");}else{printf("ONE_HPP");}
}
