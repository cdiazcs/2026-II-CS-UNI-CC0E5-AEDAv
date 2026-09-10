
#include <iostream>
#include <fstream> // ofstream para escribir en archivo
#include "containers/vector.h"

#include "Demos.h"
using namespace std;

void DemoVector(TX x) {
    Vector<TX> vec;
    for (TX i = 0; i < 10; ++i) {
        vec.push_back(i);
    }
    cout<<"Vector original: ";
    vec.print(cout);
    vec.add(x); //adition 

    cout << "Vector after adding " << x << ": ";
    vec.print(cout);

    ofstream of("vector.txt");
    vec.print(of);
    of.close();

    cout << "Vector using cout directly: ";
    cout << vec << endl;
}