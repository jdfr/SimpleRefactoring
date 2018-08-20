#include <stdio.h>
#include <string>
#include "test.hpp"

int main() { 
     int j;
     if ((((configVariable("UseSpanishLanguage", 1,2))))){printf("UNO");}else{printf("ONE");}
     if (configOption(((("UseSpanishLanguage"))), 3))     printf("DOS"); else         printf("TWO");
     if (zzz::config("UseSpanishLanguage",3,4)) {
         printf("TRES"); 
     } else {
         printf("THREE");
     }
     if (configVariable("UseSpanishLanguage",3,4)) 
     {
         printf("CUATRO"); 
     } 
     else 
     {
         printf("FOUR");
     }
     if (configVariable("UseSpanishLanguage",3,4)) 
     {
         printf("CINCO"); 
     } 
     else 
     {
         printf("FIVE");
     }
     if (configVariable("UseSpanishLanguage",3,4)) 
         printf("SEIS"); 
     else 
         printf("SIX");
     if (configVariable(std::string("UseSpanishLanguage"),3,4)) 
         printf("SIETE"); 
     else 
         printf("SEVEN");
     //The statements after this point should not be reduced
     if (configVariable(std::string("UseSpanishLanguage")+"Always",3,4)) 
         printf("OCHO"); 
     else 
         printf("EIGHT");
     if (!configVariable("UseSpanishLanguage",3,4)) 
         printf("NUEVE"); 
     else 
         printf("NINE");
     if (configVariable("UseSpanishLanguage",3,4) && true) 
         printf("DIEZ"); 
     else 
         printf("TEN");
     return 0; 
    
}


