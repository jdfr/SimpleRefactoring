#include <stdio.h>
#include <string>
#include "test.hpp"

//COMMENTS ASSUME THAT CONFIG VALUE IS true

bool f0() { return configVariable("UseSpanishLanguage",3,4); } //REWRITTEN TO true

bool f1() { return !configVariable("UseSpanishLanguage",3,4); } //REWRITTEN TO false

bool f2(bool q) { return ((!q)) && configVariable("UseSpanishLanguage",3,4); } //REWRITTEN TO !q

int main() { 
     int j;
     //REWRITTEN (then BRANCH): UNO
     if ((((configVariable("UseSpanishLanguage", 1,2))))){printf("UNO");}else{printf("ONE");}
     //REWRITTEN: DOS
     if (configOption(((("UseSpanishLanguage"))), 3))     printf("DOS"); else         printf("TWO");
     //REWRITTEN (then BRANCH): TRES
     if (zzz::config("UseSpanishLanguage",3,4)) {
         printf("TRES"); 
     } else {
         printf("THREE");
     }
     //REWRITTEN (then BRANCH): CUATRO
     if (configVariable("UseSpanishLanguage",3,4)) 
     {
         printf("CUATRO"); 
     } 
     else 
     {
         printf("FOUR");
     }
     //REWRITTEN (then BRANCH): CINCO
     if (configVariable("UseSpanishLanguage",3,4)) 
       {
         printf("CINCO"); 
       }
     else 
       {
         printf("FIVE");
       }
     //REWRITTEN (then BRANCH): SEIS
     if (configVariable("UseSpanishLanguage",3,4)) 
       { printf("SEIS"); }
     else 
       {  printf("SIX"); }
     //REWRITTEN (then BRANCH): SIETE
     if (configVariable(std::string("UseSpanishLanguage"),3,4)) 
         printf("SIETE"); 
     else 
         printf("SEVEN");
     //NOT REWRITTEN
     if (configVariable(std::string("UseSpanishLanguage")+"Always",3,4)) 
         printf("OCHO"); 
     else 
         printf("EIGHT");
     //REWRITTEN (else BRANCH): NINE
     if (!configVariable("UseSpanishLanguage",3,4)) 
         printf("NUEVE"); 
     else 
         printf("NINE");
     bool q = true;
     //REWRITTEN (condition): !q
     if (!q && configVariable("UseSpanishLanguage",3,4))
         printf("ONCE");
     else
         printf("ELEVEN");
     //REWRITTEN (else BRANCH): TWELVE
     if (!q && !configVariable("UseSpanishLanguage",3,4))
         printf("DOCE");
     else
         printf("TWELVE");
     //REWRITTEN (then BRANCH): TRECE
     if (!q || configVariable("UseSpanishLanguage",3,4))
         printf("TRECE");
     else
         printf("THIRTEEN");
     //REWRITTEN (condition): !q
     if (((((!q)) || !configVariable("UseSpanishLanguage",3,4))))
         printf("CATORCE");
     else
         printf("FOURTEEN");
     //REWRITTEN (then BRANCH): QUINCE
     if (!q || ((int)((configVariable("UseSpanishLanguage",3,4)))))
         printf("QUINCE");
     else
         printf("FIFTEEN");
     //REWRITTEN (condition): !q
     if (((configVariable("UseSpanishLanguage",3,4) && ((!q)))))
         printf("DIECISEIS");
     else
         printf("SIXTEEN");
     //REWRITTEN (condition): !q
     if (((!(configVariable("UseSpanishLanguage",3,4)))) || ((!q)))
         printf("DIECISIETE");
     else
         printf("SEVENTEEN");
     //REWRITTEN (condition): false > 0
     if (((!(configVariable("UseSpanishLanguage",3,4)))) > 0)
         printf("DIECIOCHO");
     else
         printf("EIGHTEEN");
     const char *x;
     //REWRITTEN (LHS): "DIECINUEVE"
     x = !q || configVariable("UseSpanishLanguage",3,4) ?
             "DIECINUEVE"
             :
             "NINETEEN";
     //REWRITTEN (RHS): "TWENTY"
     x = !q && !configVariable("UseSpanishLanguage",3,4) ? "VEINTE" : (("TWENTY"));
     //REWRITTEN (condition): configVariable(std::string("AnotherConfigValue"),3,4)
     if (configVariable(std::string("AnotherConfigValue"),3,4) && configVariable(std::string("UseSpanishLanguage"),3,4))
         printf("VEINTIUNO");
     else
         printf("TWENTYONE");
     //REWRITTEN (condition): configVariable(std::string("AnotherConfigValue"),3,4)
     if (configVariable(std::string("UseSpanishLanguage"),3,4) && configVariable(std::string("AnotherConfigValue"),3,4))
         printf("VEINTIDOS");
     else
         printf("TWENTYTWO");
     //REWRITTEN (condition): !q
     if (configVariable(std::string("UseSpanishLanguage"),3,4) ? ((!q)) : q+42)
         printf("VEINTITRES");
     else
         printf("TWENTYTHREE");
     bool w0 = configVariable("UseSpanishLanguage",3,4); //REWRITTEN TO true
     bool w1 = !configVariable("UseSpanishLanguage",3,4); //REWRITTEN TO false
     bool w2 = ((!q)) && configVariable("UseSpanishLanguage",3,4); //REWRITTEN TO !q
     w2 = configVariable("UseSpanishLanguage",3,4); //REWRITTEN TO true
     w2 = !configVariable("UseSpanishLanguage",3,4); //REWRITTEN TO false
     w2 = ((!q)) && configVariable("UseSpanishLanguage",3,4); //REWRITTEN TO !q
     w2 = f2(((configVariable("UseSpanishLanguage",3,4)))); //REWRITTEN to true
     w2 = f2((!(configVariable("UseSpanishLanguage",3,4)))); //REWRITTEN to false
     w2 = f2(((!q)) && ((configVariable("UseSpanishLanguage",3,4)))); //REWRITTEN to !q
     return 0; 
    
}
