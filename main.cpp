/**
 * @file main.cpp
 * @ingroup demos
 * @brief Punto de entrada principal que ejecuta las demostraciones del repositorio.
 */

#include "demos.h"
#include <exception>
#include <iostream>
#include <string_view>

using namespace std;

/**
 * @brief Ejecuta secuencialmente todas las demostraciones de estructuras de datos.
 *
 * Compilar con `make` y ejecutar con `./main`.
 * @return Código de salida del programa (`0` si finaliza correctamente).
 */
int main(int argc, char* argv[]) {
    if (argc > 1) {
        if (argc != 2 || std::string_view(argv[1]) != "--texto") {
            std::cerr << "Uso: " << argv[0] << " [--texto]\n";
            return 1;
        }
        try {
            DemoTexto();
            return 0;
        } catch (const std::exception& error) {
            std::cerr << "Error en DemoTexto: " << error.what() << '\n';
            return 1;
        }
    }

    DemoVector();
    DemoRaceCondition();
    DemoLinkedList();
    DemoHeap();
    DemoBinaryTree();
    DemoHashTable();
    DemoMerkleTree();
    DemoGotoxy();
    return 0;
}
