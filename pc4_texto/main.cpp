#include "texto_excel.h"
#include <iostream>
#include <sstream>
int main() {
    pc4texto::Sheet sheet;
    std::cout << "PC4 Excel Texto | SET A1 =SUMA(2,3) | GET A1 | SAVE ruta | LOAD ruta | EXIT\n";
    std::string line;
    while(std::cout << "> " && std::getline(std::cin,line)) {
        try {
            std::istringstream in(line);std::string cmd,arg;in>>cmd;
            if(cmd=="EXIT" || cmd=="SALIR")break;
            if(cmd=="SET") { in>>arg;std::string data;std::getline(in,data);if(!data.empty()&&data[0]==' ')data.erase(0,1);sheet.set(arg,data);std::cout<<"OK\n"; }
            else if(cmd=="GET") {in>>arg;std::cout<<sheet.calculate(arg).display()<<'\n';}
            else if(cmd=="RAW") {in>>arg;std::cout<<sheet.get(arg)<<'\n';}
            else if(cmd=="SAVE") {in>>arg;sheet.save(arg);std::cout<<"Guardado\n";}
            else if(cmd=="LOAD") {in>>arg;sheet.load(arg);std::cout<<"Cargado\n";}
            else if(!cmd.empty())std::cout<<"Comando no reconocido\n";
        }catch(const std::exception& e){std::cout<<"ERROR: "<<e.what()<<'\n';}
    }
}
