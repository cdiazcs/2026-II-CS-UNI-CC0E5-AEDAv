#include "texto_excel.h"

#include <iostream>
#include <sstream>

// programa por consola: lee comandos linea por linea y los aplica a la hoja
int main() {
    pc4texto::Sheet sheet;
    std::cout << "PC4 Excel Texto | SET A1 =SUMA(2,3) | GET A1 | SAVE ruta | LOAD ruta | EXIT\n";

    std::string line;
    while (std::cout << "> " && std::getline(std::cin, line)) {
        try {
            // la primera palabra es el comando
            std::istringstream in(line);
            std::string cmd;
            std::string arg;
            in >> cmd;

            if (cmd == "EXIT" || cmd == "SALIR") {
                break;
            }

            if (cmd == "SET") {
                // SET celda contenido: el contenido es todo el resto de la linea
                in >> arg;
                std::string data;
                std::getline(in, data);
                if (!data.empty() && data[0] == ' ') {
                    data.erase(0, 1);
                }
                sheet.set(arg, data);
                std::cout << "OK\n";
            } else if (cmd == "GET") {
                // GET celda: muestra el valor calculado
                in >> arg;
                std::cout << sheet.calculate(arg).display() << '\n';
            } else if (cmd == "RAW") {
                // RAW celda: muestra lo que se escribio, sin calcular
                in >> arg;
                std::cout << sheet.get(arg) << '\n';
            } else if (cmd == "SAVE") {
                in >> arg;
                sheet.save(arg);
                std::cout << "Guardado\n";
            } else if (cmd == "LOAD") {
                in >> arg;
                sheet.load(arg);
                std::cout << "Cargado\n";
            } else if (!cmd.empty()) {
                std::cout << "Comando no reconocido\n";
            }
        } catch (const std::exception& e) {
            // cualquier error se muestra y el programa sigue
            std::cout << "ERROR: " << e.what() << '\n';
        }
    }
}
