
#include <iostream>
#include <fstream> // ofstream para escribir en archivo
#include <string>
#include "containers/vector.h"
#include "Demos.h"
using namespace std;

void AddOne(TX &value) {
    value += 1;
}

template <typename T>
void AddX(T &value, T x) {
    value += x;
}

void Square(TX &value) {
    value *= value;
}

void DemoVector() {
    Vector<TX> vec;
    for (TX i = 0; i < 10; ++i) {
        vec.push_back(i);
    }

    cout << "Vector using print: ";
    vec.write(cout);
    cout << endl;

    ofstream of("vector.txt");
    vec.write(of);
    of.close();

    cout << "Vector using cout directly: ";
    cout << vec << endl;

    vec.ApplyFunction(AddOne);
    cout << "Vector after applying AddOne function: ";
    cout << vec << endl;

    vec.ApplyFunction(AddX<TX>, 5);
    cout << "Vector after applying lambda function (Add 5): ";
    cout << vec << endl;

    vec.ApplyFunction(Square);
    cout << "Vector after applying Square function: ";
    cout << vec << endl;

    Vector<string> strVec;
    strVec.push_back("Hello");
    strVec.push_back("World");

    cout << "VectorStr: ";
    cout << strVec << endl;

    strVec.ApplyFunction(AddX<string>, "!");
    cout << "VectorStr after applying lambda function (Add !): ";
    cout << strVec << endl;


}