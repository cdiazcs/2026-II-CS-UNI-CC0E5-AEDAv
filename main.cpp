#include <iostream>
#include "Demos.h"
#include "containers/vector.h"

using namespace std;

// g++ main.cpp -o main
// para ejecutar: ./main
// g++ -std=c++23 main.cpp Demos.cpp -o main
int main() {
    //define variable
    TX x;
            
    cin>>x;
    
    DemoVector(x);
    return 0;
}