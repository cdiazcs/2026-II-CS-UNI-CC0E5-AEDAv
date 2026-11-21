
#include "demos.h"
#include <exception>
#include <iostream>
#include <string_view>

using namespace std;


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
    DemoExcelTexto();
    DemoGotoxy();
    return 0;
}
