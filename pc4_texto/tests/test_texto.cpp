#include "../texto_excel.h"
#include "utilities/ansi_style.h"

#include <cmath>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <iterator>
#include <optional>
#include <stdexcept>
#include <string>
#include <string_view>
#include <typeindex>
#include <typeinfo>
#include <utility>

namespace {
using pc4texto::Sheet;
using pc4texto::Value;

void check(bool condition, const std::string& message) {
    if (!condition) {
        throw std::runtime_error(message);
    }
}

void expectNumber(const Value& value, double expected) {
    check(value.numeric && value.number == expected,
          "Se esperaba el numero " + Value::num(expected).display() +
          "; se obtuvo " + value.display());
}

void expectText(const Value& value, std::string_view expected) {
    check(!value.numeric && value.text == expected,
          "Se esperaba texto: " + std::string(expected) +
          "; se obtuvo " + value.display() +
          (value.numeric ? " (numero)" : " (texto)"));
}

void expectError(auto action, std::string_view prefix) {
    try {
        action();
    } catch (const std::runtime_error& error) {
        check(std::string_view(error.what()).starts_with(prefix),
              "Se esperaba un error con prefijo " + std::string(prefix) +
              "; se obtuvo " + error.what());
        return;
    }
    throw std::runtime_error("Se esperaba un error con prefijo " + std::string(prefix));
}

Value calculateFormula(const std::string& formula) {
    Sheet sheet;
    sheet.set("A1", formula);
    return sheet.calculate("A1");
}

class TemporaryDirectory {
    std::filesystem::path path_;

public:
    TemporaryDirectory() {
        auto pattern = (std::filesystem::temp_directory_path() / "pc4_texto_XXXXXX").string();
        if (!::mkdtemp(pattern.data())) {
            throw std::runtime_error("No se pudo crear el directorio temporal");
        }
        path_ = std::move(pattern);
    }

    ~TemporaryDirectory() {
        std::error_code error;
        std::filesystem::remove_all(path_, error);
    }

    const std::filesystem::path& path() const { return path_; }
};

struct Test {
    std::string_view name;
    void (*run)();
};

void expectExpression(const std::string& body, const Value& expected) {
    const auto actual = calculateFormula("=" + body);
    check(actual.numeric == expected.numeric && actual.number == expected.number &&
          actual.text == expected.text && std::signbit(actual.number) == std::signbit(expected.number),
          "Tipo o valor inesperado en " + body);
}

void expectExpressionError(const std::string& body, std::string_view expected) {
    try {
        calculateFormula("=" + body);
    } catch (const std::runtime_error& error) {
        check(typeid(error) == typeid(std::runtime_error) && error.what() == expected,
              "Tipo o mensaje inesperado en " + body + ": " + error.what());
        return;
    }
    throw std::runtime_error("Se esperaba error en " + body);
}

// ---------- funciones basicas ----------
const Test basicas[] = {
    {"promedio-escalar", [] {
        Sheet sheet;
        sheet.set("A1", "10");
        sheet.set("A2", "20");
        sheet.set("B2", "=PROMEDIO(A1,A2,30)");
        expectNumber(sheet.calculate("B2"), 20);
    }},
    {"agregaciones-numericas", [] {
        Sheet sheet;
        sheet.set("A1", "8");
        sheet.set("A2", "-2");
        sheet.set("A3", "6");
        sheet.set("B1", "=MIN(A1:A3)");
        sheet.set("B2", "=MAX(A1:A3)");
        sheet.set("B3", "=PROMEDIO(A1:A3,4)");
        expectNumber(sheet.calculate("B1"), -2);
        expectNumber(sheet.calculate("B2"), 8);
        expectNumber(sheet.calculate("B3"), 4);
    }},
    {"funciones-anidadas", [] {
        expectNumber(calculateFormula("=MAX(1,MIN(8,4))"), 4);
        expectNumber(calculateFormula("=SUMA(1,MAX(2,MIN(8,4)),3)"), 8);
    }},
    {"concat", [] {
        expectText(calculateFormula("=CONCAT(\"Hola, \",\"mundo\")"), "Hola, mundo");
    }},
    {"largo", [] {
        Sheet sheet;
        sheet.set("C1", "=CONCAT(\"Hola, \",\"mundo\")");
        sheet.set("C2", "=LARGO(C1)");
        expectNumber(sheet.calculate("C2"), 11);
    }},
    {"mayusc", [] {
        expectText(calculateFormula("=MAYUSC(\"hola\")"), "HOLA");
    }},
    {"operador-concatenacion", [] {
        expectText(calculateFormula("=\"Hola\" & \" mundo\""), "Hola mundo");
    }},
    {"funciones-vacias-y-aridad", [] {
        expectNumber(calculateFormula("=SUMA()"), 0);
        expectText(calculateFormula("=CONCAT()"), "");
        expectError([] { calculateFormula("=MIN()"); }, "Faltan argumentos");
        for (const std::string name : {"LARGO", "MAYUSC", "MINUSC"}) {
            const auto error = name + " requiere un argumento";
            expectError([&] { calculateFormula("=" + name + "()"); }, error);
            expectError([&] { calculateFormula("=" + name + "(1,2)"); }, error);
        }
    }},
    {"funciones-de-texto-anidadas", [] {
        const std::string nested = R"(MINUSC(CONCAT(MAYUSC("ab"),"CD")))";
        expectText(calculateFormula("=" + nested), "abcd");
        expectNumber(calculateFormula("=LARGO(" + nested + ")"), 4);
    }},
    {"contar", [] {
        Sheet sheet;
        sheet.set("A1", "8");
        sheet.set("A3", "0");
        sheet.set("A4", "texto");
        sheet.set("B1", "=CONTAR(A1:A4,5)");
        expectNumber(sheet.calculate("B1"), 3);
    }},
    {"count", [] {
        Sheet sheet;
        sheet.set("A1", "0");
        sheet.set("A2", "texto");
        sheet.set("A3", "8");
        sheet.set("B1", "=COUNT(A1:A4,5)");
        expectNumber(sheet.calculate("B1"), 3);
    }},
    {"contar-literales-numericos", [] {
        expectNumber(calculateFormula("=CONTAR(0,-2,1.5)"), 3);
    }},
    {"contar-texto-numerico", [] {
        Sheet sheet;
        sheet.set("A1", R"(="8")");
        sheet.set("B1", R"(=CONTAR("8",A1,A1:A1,8))");
        expectNumber(sheet.calculate("B1"), 1);
    }},
    {"contar-funciones-anidadas", [] {
        expectNumber(calculateFormula(R"(=CONTAR(SUMA(1,2),CONCAT("1","2"),CONTAR()))"), 2);
    }},
    {"contar-sin-argumentos", [] {
        expectNumber(calculateFormula("=CONTAR()"), 0);
        expectNumber(calculateFormula("=COUNT()"), 0);
    }},
    {"funciones-escalares", [] {
        expectExpression("SUMA(2,3)", Value::num(5));
        expectExpression("PROMEDIO(2,4)", Value::num(3));
        expectExpression(R"(CONTAR(2,0,"texto"))", Value::num(2));
        expectExpression(R"(CONCAT("A","B"))", Value::str("AB"));
        expectExpression("sUm(2;3)+AVERAGE(2,4)+COUNT(0)", Value::num(9));
        expectExpression("MAX(2,3)-MIN(2,3)", Value::num(1));
        expectExpression(R"(LEN(UPPER(LOWER("Ab")))+LARGO(MAYUSC(MINUSC("Cd"))))", Value::num(4));
        expectExpression(R"(CONCATENAR("A";"B"))", Value::str("AB"));
    }},
    {"funciones-anidadas-y-aritmetica", [] {
        expectExpression("SUMA(2,PROMEDIO(4,6))", Value::num(7));
        expectExpression("SUMA(2,3)*2", Value::num(10));
        expectExpression("SUMA(1;2,3)", Value::num(6));
        expectExpression("SUMA()+COUNT()", Value::num(0));
        expectExpression("CONCAT()", Value::str(""));
        const auto nested = [](int depth) {
            std::string formula = "=";
            for (int i = 0; i < depth; ++i) formula += "SUMA(";
            return formula + "1" + std::string(depth, ')');
        };
        expectNumber(calculateFormula(nested(1000)), 1);
        expectError([&] { calculateFormula(nested(1001)); }, "Formula demasiado anidada");
    }},
    {"crecimiento-de-argumentos", [] {
        expectNumber(calculateFormula(
            "=SUMA(1,2,3,4,5,6,7,8,9,10,11,12,13,14,15,16,17,18,19,20,21)"), 231);
    }},
    {"referencias-y-operaciones-binarias", [] {
        Sheet sheet;
        sheet.set("A1", "10");
        sheet.set("A2", "= a1 + 1 ");
        sheet.set("A3", "=A2+1");
        sheet.set("A4", "=A3");
        sheet.set("A5", "=A4*2");
        sheet.set("A6", "=A5-Z1");
        sheet.set("A7", "=A6/3");
        expectNumber(sheet.calculate("A3"), 12);
        expectNumber(sheet.calculate("A7"), 8);
    }},
    {"formas-equivalentes-de-sumar-uno", [] {
        Sheet sheet;
        sheet.set("A1", "10");
        sheet.set("A2", "=A1+1");
        sheet.set("A3", "=(A1+1)");
        sheet.set("A4", "=SUMA(A1,1)");
        sheet.set("A5", "=A1+2*3");
        sheet.set("A6", "=-A1");
        sheet.set("A7", "=SUMA(A1:A1,1)");
        sheet.set("A8", "=A1+6");
        sheet.set("A9", "=0-A1");
        for (const char* address : {"A2", "A3", "A4", "A7"}) {
            expectNumber(sheet.calculate(address), 11);
        }
        expectNumber(sheet.calculate("A5"), 16);
        expectNumber(sheet.calculate("A8"), 16);
        expectNumber(sheet.calculate("A6"), -10);
        expectNumber(sheet.calculate("A9"), -10);
    }},
    {"unarios-y-decimales", [] {
        expectExpression(" - + - .5e1 + 2. ", Value::num(7));
        expectExpression("1E+2 / 2e1", Value::num(5));
        expectExpression("0x10", Value::num(16));
    }},
    {"concatenacion-y-comillas", [] {
        expectExpression(R"("a""b" & 2*3 & "")", Value::str("a\"b6"));
        expectExpression(R"("1"&2+3)", Value::num(15));
    }},
    {"compatibilidad-de-conversiones-en-formulas", [] {
        expectNumber(calculateFormula("=1e3-(-.5)*2."), 1001);
        expectNumber(calculateFormula("=0x10"), 16);
        expectNumber(calculateFormula(R"(=SUMA("0x10",1))"), 17);
    }},
};

// ---------- parentesis ----------
const Test parentesis[] = {
    {"aritmetica", [] {
        Sheet sheet;
        sheet.set("A1", "10");
        sheet.set("A2", "20");
        sheet.set("B1", "=(A1+2)*(A2-5)");
        expectNumber(sheet.calculate("B1"), 180);
    }},
    {"parentesis-anidados", [] {
        expectNumber(calculateFormula("=((1+2)*(3+4))/7"), 3);
        expectNumber(calculateFormula("=2*(3+(4-1))"), 12);
    }},
    {"parentesis-signo-negativo", [] {
        expectNumber(calculateFormula("=-(2+3)*-(1+1)"), 10);
        expectNumber(calculateFormula("=-(1-4)"), 3);
    }},
    {"parentesis-sin-cerrar", [] {
        expectError([] { calculateFormula("=((1+2)"); }, "Se esperaba ')'");
        expectError([] { calculateFormula("=SUMA(1,2"); }, "Se esperaba ')'");
    }},
    {"precedencia-y-parentesis", [] {
        expectExpression("2+3*4", Value::num(14));
        expectExpression("(2+3)*4", Value::num(20));
        expectExpression("((2+3)*(4-1))/5", Value::num(3));
        expectExpression("20/2/2-1", Value::num(4));
    }},
    {"aritmetica-equivalente", [] {
        Sheet sheet;
        sheet.set("A1", "2.5");
        sheet.set("A2", "=A1*4");
        sheet.set("A3", "=A2+1");
        sheet.set("A4", "=((+A1)*(-(-4)))+1");
        expectNumber(sheet.calculate("A3"), 11);
        expectNumber(sheet.calculate("A4"), sheet.calculate("A3").number);
        expectNumber(calculateFormula("=2+3*4"), 14);
        expectNumber(calculateFormula("=(2+3)*4"), 20);
    }},
    {"limite-de-parentesis-anidados", [] {
        const auto nested = [](int depth) {
            return "=" + std::string(depth, '(') + "1" + std::string(depth, ')');
        };
        expectNumber(calculateFormula(nested(1000)), 1);
        expectError([&] { calculateFormula(nested(1001)); }, "Formula demasiado anidada");
    }},
};

// ---------- rangos y celdas vacias ----------
const Test rangos[] = {
    {"suma-rango", [] {
        Sheet sheet;
        sheet.set("A1", "10");
        sheet.set("A2", "20");
        sheet.set("A3", "=SUMA(A1:A2)");
        expectNumber(sheet.calculate("A3"), 30);
    }},
    {"basicas-vacias-en-promedio-min-max", [] {
        Sheet sheet;
        sheet.set("A1", "10");
        sheet.set("A2", "20");
        // A3 queda vacia a proposito
        sheet.set("B1", "=PROMEDIO(A1:A3)");
        sheet.set("B2", "=MIN(A1:A3)");
        sheet.set("B3", "=MAX(A1:A3)");
        sheet.set("B4", "=SUMA(A1:A3)");
        sheet.set("B5", "=A9");
        expectNumber(sheet.calculate("B1"), 15);
        expectNumber(sheet.calculate("B2"), 10);
        expectNumber(sheet.calculate("B3"), 20);
        expectNumber(sheet.calculate("B4"), 30);
        expectNumber(sheet.calculate("B5"), 0);
        // con negativos la celda vacia tampoco debe ganar como 0
        sheet.set("A1", "-5");
        sheet.set("A2", "-1");
        expectNumber(sheet.calculate("B3"), -1);
        expectNumber(sheet.calculate("B2"), -5);
    }},
    {"basicas-rango-sin-valores", [] {
        expectError([] { calculateFormula("=PROMEDIO(Z1:Z3)"); }, "Division por cero");
        expectNumber(calculateFormula("=MIN(Z1:Z3)"), 0);
        expectNumber(calculateFormula("=MAX(Z1:Z3)"), 0);
        expectNumber(calculateFormula("=SUMA(Z1:Z3)"), 0);
    }},
    {"basicas-rango-2d", [] {
        Sheet sheet;
        sheet.set("A1", "1");
        sheet.set("B1", "2");
        sheet.set("A2", "3");
        sheet.set("B2", "4");
        sheet.set("C1", "=SUMA(A1:B2)");
        sheet.set("C2", "=PROMEDIO(A1:B2)");
        expectNumber(sheet.calculate("C1"), 10);
        expectNumber(sheet.calculate("C2"), 2.5);
    }},
    {"orden-de-rango-bidimensional", [] {
        Sheet sheet;
        sheet.set("A1", "a");
        sheet.set("B1", "b");
        sheet.set("A2", "c");
        sheet.set("B2", "d");
        sheet.set("C1", R"(=CONCAT("inicio-",A1:B2,"-fin"))");
        expectText(sheet.calculate("C1"), "inicio-abcd-fin");
    }},
    {"formula-reutilizada-en-rango", [] {
        Sheet sheet;
        sheet.set("A1", "1");
        sheet.set("B1", "=A1+1");
        sheet.set("C1", "=SUMA(B1,B1:B1)");
        expectNumber(sheet.calculate("C1"), 4);
    }},
    {"rango-invertido", [] {
        expectError([] { calculateFormula("=SUMA(A3:A1)"); }, "Rango invalido");
        expectError([] { calculateFormula("=SUMA(C1:A1)"); }, "Rango invalido");
    }},
    {"promedio-rango-vacio", [] {
        expectNumber(calculateFormula("=PROMEDIO(Z1:Z3,10)"), 10);
    }},
    {"average-rango-vacio", [] {
        expectNumber(calculateFormula("=AVERAGE(Z1:Z3,10)"), 10);
    }},
    {"promedio-sin-valores", [] {
        expectError([] { calculateFormula("=PROMEDIO(Z1:Z3)"); }, "Division por cero");
    }},
    {"promedio-rango-parcial-con-cero", [] {
        Sheet sheet;
        sheet.set("A1", "8");
        sheet.set("A3", "0");
        sheet.set("B1", "=PROMEDIO(A1:A3,4)");
        expectNumber(sheet.calculate("B1"), 4);
        sheet.set("B1", "=PROMEDIO(A1:A25,4)");
        expectNumber(sheet.calculate("B1"), 4);
    }},
    {"promedio-referencia-escalar-ausente", [] {
        expectNumber(calculateFormula("=PROMEDIO(Z1,10)"), 5);
    }},
    {"promedio-rango-con-formula-que-da-cero", [] {
        Sheet sheet;
        sheet.set("A1", "=Z1");
        sheet.set("B1", "=PROMEDIO(A1:A1,10)");
        expectNumber(sheet.calculate("B1"), 5);
    }},
    {"promedio-rango-con-vacio-explicito-y-texto", [] {
        Sheet sheet;
        sheet.set("B1", "=PROMEDIO(A1:A2,10)");
        sheet.set("A1", "");
        expectError([&] { sheet.calculate("B1"); }, "Se esperaba un numero: ");
        sheet.set("A1", "texto");
        expectError([&] { sheet.calculate("B1"); }, "Se esperaba un numero: texto");
        sheet.set("A1", "=\"6\"");
        expectNumber(sheet.calculate("B1"), 8);
    }},
    {"otras-funciones-con-ausencias", [] {
        Sheet sheet;
        sheet.set("A1", "8");
        sheet.set("B1", "=SUMA(A1:A2)");
        sheet.set("B2", "=MIN(A1:A2)");
        sheet.set("B3", "=MAX(A1:A2)");
        sheet.set("B4", R"(=CONCAT("x",A1:A2,"y"))");
        expectNumber(sheet.calculate("B1"), 8);
        expectNumber(sheet.calculate("B2"), 8);
        expectNumber(sheet.calculate("B3"), 8);
        expectText(sheet.calculate("B4"), "x80y");
        sheet.set("A1", "-8");
        expectNumber(sheet.calculate("B3"), -8);
    }},
    {"promedio-anidado", [] {
        expectNumber(calculateFormula("=SUMA(PROMEDIO(Z1:Z3,10),MIN(Z1:Z3))"), 10);
        expectNumber(calculateFormula("=AVERAGE(SUMA(Z1:Z3),10)"), 5);
    }},
    {"contar-rango-ausente", [] {
        expectNumber(calculateFormula("=CONTAR(Z1:Z25)"), 0);
    }},
    {"contar-texto-y-vacio-explicito", [] {
        Sheet sheet;
        sheet.set("A1", "texto");
        sheet.set("A2", "");
        sheet.set("A3", R"(="")");
        sheet.set("B1", "=CONTAR(A1:A3)");
        expectNumber(sheet.calculate("B1"), 0);
    }},
    {"contar-referencia-ausente", [] {
        expectNumber(calculateFormula("=CONTAR(Z1,5)"), 2);
        expectNumber(calculateFormula("=CONTAR(Z1:Z1,5)"), 1);
    }},
    {"contar-formulas-numerica-y-textual", [] {
        Sheet sheet;
        sheet.set("A1", "=2-2");
        sheet.set("A2", R"(="0")");
        sheet.set("B1", "=CONTAR(A1:A2)");
        expectNumber(sheet.calculate("B1"), 1);
    }},
    {"funciones-anidadas-con-rango-y-referencias-reutilizadas", [] {
        Sheet sheet;
        sheet.set("A1", "=2+4");
        sheet.set("A3", "=A1-A1");
        sheet.set("B1", "=SUMA(PROMEDIO(A1:A3,A1),CONTAR(A1:A3,A3),A1)");
        expectNumber(sheet.calculate("B1"), 13);
    }},
    {"concatenacion-anidada-en-orden-de-rango", [] {
        Sheet sheet;
        sheet.set("A1", R"(=MAYUSC("a"))");
        sheet.set("A2", R"(=CONCAT(A1,"c"))");
        sheet.set("B2", "=A1");
        sheet.set("C1", R"(=CONCAT("[",A1:B2,CONCAT(":",B2,"]")))");
        expectText(sheet.calculate("C1"), "[A0AcA:A]");
    }},
    {"rangos-ausencias-y-ceros", [] {
        Sheet sheet;
        sheet.set("A1", "10");
        sheet.set("A3", "20");
        const auto verify = [&](const char* formula, double expected) {
            sheet.set("B1", formula);
            expectNumber(sheet.calculate("B1"), expected);
        };
        verify("=SUMA(A1:A3)", 30);
        verify("=PROMEDIO(A1:A3)", 15);
        verify("=CONTAR(A1:A3)+COUNT(A1:A3)", 4);
        verify("=CONTAR(Z1:Z25)", 0);
        verify("=CONTAR(Z1,Z1:Z1,COUNT(Z1:Z1,Z1))", 2);
        sheet.set("A4", "0");
        sheet.set("A5", "texto");
        verify("=CONTAR(A1:A5)", 3);
        verify("=COUNT(Z1:Z3)", 0);
        sheet.set("B1", "=PROMEDIO(Z1:Z3)");
        expectError([&] { sheet.calculate("B1"); }, "Division por cero");
    }},
    {"rango-bidimensional-con-formulas", [] {
        Sheet sheet;
        sheet.set("D1", R"(=CONCAT("a","b"))");
        sheet.set("A1", "=D1");
        sheet.set("B1", R"(=D1&"c")");
        sheet.set("B2", "0");
        sheet.set("C1", R"(=CONCAT("[",A1:B2;CONCAT(":",B1:B1),"]"))");
        expectText(sheet.calculate("C1"), "[ababc00:abc]");
    }},
};

// ---------- ciclos y dependencias ----------
const Test ciclos[] = {
    {"ciclo-b1-b2", [] {
        Sheet sheet;
        sheet.set("B1", "=B2");
        sheet.set("B2", "=B1");
        expectError([&] { sheet.calculate("B1"); }, "Referencia circular: B1");
        expectError([&] { sheet.calculate("B2"); }, "Referencia circular: B2");
    }},
    {"autorreferencia", [] {
        expectError([] { calculateFormula("=A1+1"); }, "Referencia circular: A1");
    }},
    {"autorreferencia-con-y-sin-parentesis", [] {
        for (const char* formula : {"=A1", "=(A1)"}) {
            expectError([&] { calculateFormula(formula); }, "Referencia circular: A1");
        }
    }},
    {"ciclo-indirecto-de-tres-celdas", [] {
        Sheet sheet;
        sheet.set("A1", "=B1+1");
        sheet.set("B1", "=C1+1");
        sheet.set("C1", "=SUMA(A1:A1)");
        expectError([&] { sheet.calculate("A1"); }, "Referencia circular: A1");
        expectError([&] { sheet.calculate("B1"); }, "Referencia circular: B1");
        expectError([&] { sheet.calculate("C1"); }, "Referencia circular: C1");
    }},
    {"referencias-compartidas", [] {
        Sheet sheet;
        sheet.set("A1", "10");
        sheet.set("B1", "=A1+1");
        sheet.set("C1", "=A1+B1");
        sheet.set("D1", "=B1+C1");
        expectNumber(sheet.calculate("C1"), 21);
        expectNumber(sheet.calculate("D1"), 32);
    }},
    {"dependencia-compartida-en-diamante", [] {
        Sheet sheet;
        sheet.set("A1", "=2*3");
        sheet.set("B1", "=A1+1");
        sheet.set("C1", "=A1*2");
        sheet.set("D1", "=B1+C1");
        expectNumber(sheet.calculate("D1"), 19);
    }},
    {"ciclo-y-diamante", [] {
        Sheet sheet;
        sheet.set("A1", "=2*3");
        sheet.set("B1", "=A1+1");
        sheet.set("C1", "=A1*2");
        sheet.set("D1", "=B1+C1");
        expectNumber(sheet.calculate("D1"), 19);
        sheet.set("A1", "=D1");
        expectError([&] { sheet.calculate("D1"); }, "Referencia circular: D1");
        expectError([] { calculateFormula("=A1"); }, "Referencia circular: A1");
    }},
    {"funciones-referencias-y-ciclos", [] {
        Sheet sheet;
        sheet.set("A1", "=2*3");
        sheet.set("A2", "=SUMA(1,PROMEDIO(A1,2),A1+1,Z1)");
        expectNumber(sheet.calculate("A2"), 12);
        sheet.set("A1", "=SUMA(A2,1)");
        expectError([&] { sheet.calculate("A2"); }, "Referencia circular: A2");
    }},
    {"recalculo-tras-cambios", [] {
        Sheet sheet;
        sheet.set("A1", "=2*3");
        sheet.set("B1", "=A1+1");
        sheet.set("C1", "=A1*2");
        sheet.set("D1", "=B1+C1");
        expectNumber(sheet.calculate("d1"), 19);
        expectError([&] { sheet.calculate("A0"); }, "Celda invalida");
        sheet.set("A1", "=1/0");
        expectError([&] { sheet.calculate("D1"); }, "Division por cero");
        sheet.set("A1", "4");
        expectNumber(sheet.calculate("D1"), 13);
    }},
    {"error-profundo-y-recalculo", [] {
        Sheet sheet;
        constexpr int depth = 900;
        for (int row = 1; row <= depth; ++row)
            sheet.set("A" + std::to_string(row), "=-(-A" + std::to_string(row + 1) + ")+2*3");
        const auto leaf = "A" + std::to_string(depth + 1);
        sheet.set(leaf, "=1/0");
        expectError([&] { sheet.calculate("A1"); }, "Division por cero");
        sheet.set(leaf, "1");
        expectNumber(sheet.calculate("A1"), 5401);
    }},
    {"limite-de-dependencias", [] {
        // 1000 celdas con formula encadenadas se pueden calcular; 1001 ya no
        const auto chain = [](int length) {
            Sheet sheet;
            for (int row = 1; row <= length; ++row) {
                sheet.set("A" + std::to_string(row), "=A" + std::to_string(row + 1) + "+1");
            }
            sheet.set("A" + std::to_string(length + 1), "0");
            return sheet;
        };
        expectNumber(chain(1000).calculate("A1"), 1000);
        expectError([&] { chain(1001).calculate("A1"); }, "Demasiadas dependencias");
        // al calcular desde mas abajo la cadena es mas corta y si funciona
        expectNumber(chain(1001).calculate("A2"), 1000);
    }},
};

// ---------- errores ----------
const Test errores[] = {
    {"basicas-errores", [] {
        expectError([] { calculateFormula("=FOO(1)"); }, "Funcion desconocida: FOO");
        expectError([] { calculateFormula("=\"hola\"+1"); }, "Se esperaba un numero: hola");
        expectError([] { calculateFormula("=1+\"hola\""); }, "Se esperaba un numero: hola");
    }},
    {"parentesis-sin-cierre", [] {
        expectError([] { calculateFormula("=(2+3"); }, "Se esperaba ')'");
    }},
    {"division-por-cero", [] {
        expectError([] { calculateFormula("=1/0"); }, "Division por cero");
    }},
    {"error-en-dependencia-y-recalculo", [] {
        Sheet sheet;
        sheet.set("A1", "=1/0");
        sheet.set("B1", "=A1+1");
        expectError([&] { sheet.calculate("B1"); }, "Division por cero");
        sheet.set("A1", "=8/2");
        expectNumber(sheet.calculate("B1"), 5);
    }},
    {"contar-propaga-error", [] {
        Sheet sheet;
        sheet.set("A1", "=1/0");
        sheet.set("B1", "=CONTAR(A1:A1)");
        expectError([&] { sheet.calculate("B1"); }, "Division por cero");
    }},
    {"division-antes-de-autorreferencia", [] {
        expectError([] { calculateFormula("=1/0+A1"); }, "Division por cero");
    }},
    {"error-de-dependencia-antes-de-sintaxis-sobrante", [] {
        Sheet sheet;
        sheet.set("A1", "=A2)");
        sheet.set("A2", "=1/0");
        expectError([&] { sheet.calculate("A1"); }, "Division por cero");
        sheet.set("A2", "1");
        expectError([&] { sheet.calculate("A1"); }, "Caracteres sobrantes en formula");
    }},
    {"primer-error-en-rango", [] {
        Sheet sheet;
        sheet.set("B1", "=1/0");
        sheet.set("A2", "=(2+3");
        sheet.set("C1", "=SUMA(A1:B2)");
        expectError([&] { sheet.calculate("C1"); }, "Division por cero");
        sheet.set("B1", "1");
        expectError([&] { sheet.calculate("C1"); }, "Se esperaba ')'");
    }},
    {"errores-en-dependencias-y-hexadecimal", [] {
        Sheet sheet;
        sheet.set("A1", "=1/0");
        sheet.set("A2", "=A1+1");
        expectError([&] { sheet.calculate("A2"); }, "Division por cero");
        sheet.set("A1", "=2/1");
        expectNumber(sheet.calculate("A2"), 3);
        expectError([] { calculateFormula("=1/0+A1"); }, "Division por cero");

        sheet.set("A1", "0x10");
        sheet.set("A2", "=A1");
        sheet.set("A3", "=A2+1");
        expectText(sheet.calculate("A2"), "0x10");
        expectNumber(sheet.calculate("A3"), 17);
    }},
    {"orden-entre-division-y-ciclo", [] {
        Sheet sheet;
        sheet.set("B1", "=1/0");
        sheet.set("C1", "=A1");
        for (const char* formula : {"=B1+C1", "=(B1+C1)"}) {
            sheet.set("A1", formula);
            expectError([&] { sheet.calculate("A1"); }, "Division por cero");
        }
        for (const char* formula : {"=C1+B1", "=(C1+B1)"}) {
            sheet.set("A1", formula);
            expectError([&] { sheet.calculate("A1"); }, "Referencia circular: A1");
        }
    }},
    {"errores-antes-de-caracteres-sobrantes", [] {
        expectError([] { calculateFormula("=1/0)"); }, "Division por cero");
        expectError([] { calculateFormula("=A1)"); }, "Referencia circular: A1");
        expectError([] { calculateFormula("=1/1)"); }, "Caracteres sobrantes en formula");
    }},
    {"orden-de-errores-entre-dependencias", [] {
        Sheet sheet;
        sheet.set("A1", "=B1+C1");
        sheet.set("B1", "=1/0");
        sheet.set("C1", "=A1)");
        expectError([&] { sheet.calculate("A1"); }, "Division por cero");
        sheet.set("B1", "1");
        expectError([&] { sheet.calculate("A1"); }, "Referencia circular: A1");
        sheet.set("C1", "=B1)");
        expectError([&] { sheet.calculate("A1"); }, "Caracteres sobrantes en formula");
    }},
    {"errores-de-sintaxis-y-division", [] {
        expectExpressionError("1/0)", "Division por cero");
        expectExpressionError("(2+3", "Se esperaba ')' en posicion 4");
        expectExpressionError("2+3)", "Caracteres sobrantes en formula");
        expectExpressionError("\"ab", "Cadena sin cerrar");
    }},
    {"orden-de-coercion", [] {
        for (const char op : {'+', '-', '*'}) {
            const std::string body = std::string("\"x\"") + op + "(1/0)";
            expectExpressionError(body, "Se esperaba un numero: x");
        }
        expectExpressionError(R"("x"/0)", "Division por cero");
        expectExpressionError(R"(1+"x")", "Se esperaba un numero: x");
    }},
    {"ciclos-y-prioridad", [] {
        Sheet sheet;
        sheet.set("A1", "=1/0+A1");
        expectError([&] { sheet.calculate("A1"); }, "Division por cero");
        sheet.set("A1", "=A1)");
        expectError([&] { sheet.calculate("A1"); }, "Referencia circular: A1");
        sheet.set("A1", "=(A2)");
        sheet.set("A2", "=-A3");
        sheet.set("A3", "=A1+2*3");
        expectError([&] { sheet.calculate("A2"); }, "Referencia circular: A2");
        sheet.set("A3", "=A4)");
        sheet.set("A4", "=1/0");
        expectError([&] { sheet.calculate("A3"); }, "Division por cero");
        sheet.set("A4", "1");
        expectError([&] { sheet.calculate("A3"); }, "Caracteres sobrantes en formula");
    }},
    {"funciones-orden-de-errores", [] {
        const auto fails = [&](const char* body, const char* message) {
            expectExpressionError(body, message);
        };
        fails("DESCONOCIDA(1/0,.)", "Division por cero");
        fails("SUMA(\"x\"-1,1/0)", "Se esperaba un numero: x");
        fails("SUMA(1/0,\"x\"-1)", "Division por cero");
        fails("DESCONOCIDA(2)", "Funcion desconocida: DESCONOCIDA");
        fails("LARGO(1,2)", "LARGO requiere un argumento");
        fails("PROMEDIO()", "Faltan argumentos");
        fails("SUMA(1,)", "Expresion invalida en posicion 7");
        fails("DESCONOCIDA(1", "Se esperaba ')' en posicion 13");
        fails("SUMA(1 2)", "Se esperaba ')' en posicion 7");
        fails("SUMA(1,", "Expresion invalida en posicion 7");
    }},
    {"rangos-orden-de-errores", [] {
        Sheet sheet;
        sheet.set("A1", "10");
        sheet.set("A2", "=1/0");
        sheet.set("A3", "=B1");
        sheet.set("B1", "=SUMA(A1:A3)");
        const auto fails = [&](const char* error) {
            expectError([&] { sheet.calculate("B1"); }, error);
        };
        fails("Division por cero");
        sheet.set("A2", "20");
        fails("Referencia circular: B1");
        sheet.set("B1", "=SUMA(A3:A1)");
        fails("Rango invalido o demasiado grande");
    }},
    {"rangos-validacion-y-cierre", [] {
        const auto fails = [&](const char* formula, const char* error) {
            expectExpressionError(formula, error);
        };
        fails("SUMA(A1:A0)", "Rango invalido");
        fails("SUMA(A1:XFE1)", "Columna fuera de rango");
        fails("SUMA(A1:A1048577)", "Fila fuera de rango");
        fails("SUMA(A1:B50001)", "Rango invalido o demasiado grande");
        Sheet sheet;
        sheet.set("B1", "=SUMA(A1:A1");
        expectError([&] { sheet.calculate("B1"); }, "Se esperaba ')' en posicion 10");
    }},
    {"tabla-de-resultados-y-errores", [] {
        struct Error {
            std::type_index type;
            std::string message;
            bool operator==(const Error&) const = default;
        };
        struct Outcome {
            std::optional<Value> value;
            std::optional<Error> error;
        };
        const auto capture = [](auto evaluate) -> Outcome {
            try { return {evaluate(), std::nullopt}; }
            catch (const std::exception& e) {
                return {std::nullopt, Error{typeid(e), e.what()}};
            }
        };
        Sheet sheet;
        sheet.set("A1", "10");
        sheet.set("B1", "=A1+1");
        sheet.set("B2", "0");
        sheet.set("C1", "=1/0");
        sheet.set("C2", "=C3");
        sheet.set("C3", "=C2");
        // resultados fijados antes de cambiar el parser; deben seguir igual
        const std::pair<const char*, Outcome> cases[] = {
            {"010", {Value::num(10), std::nullopt}},
            {"0x10", {Value::str("0x10"), std::nullopt}},
            {"texto", {Value::str("texto"), std::nullopt}},
            {"", {Value::str(""), std::nullopt}},
            {" -0", {Value::num(-0.0), std::nullopt}},
            {"=--.5e1", {Value::num(5), std::nullopt}},
            {"=0x10", {Value::num(16), std::nullopt}},
            {"=.", {std::nullopt, Error{typeid(std::invalid_argument), "stod"}}},
            {"=1e9999", {std::nullopt, Error{typeid(std::out_of_range), "stod"}}},
            {"=2+3*4", {Value::num(14), std::nullopt}},
            {"=(2+3)*4", {Value::num(20), std::nullopt}},
            {"=20/2/2-1", {Value::num(4), std::nullopt}},
            {"=8-3-2", {Value::num(3), std::nullopt}},
            {"=\"1\"&2+3", {Value::num(15), std::nullopt}},
            {"=\"a\"\"b\"&0", {Value::str("a\"b0"), std::nullopt}},
            {"=(A1", {std::nullopt, Error{typeid(std::runtime_error), "Se esperaba ')' en posicion 3"}}},
            {"=B1)", {std::nullopt, Error{typeid(std::runtime_error), "Caracteres sobrantes en formula"}}},
            {"=SUMA(2;PROMEDIO(4,6),B1)", {Value::num(18), std::nullopt}},
            {"=SUM(1,2)+AVERAGE(2,4)", {Value::num(6), std::nullopt}},
            {"=MIN(8,2)+MAX(4,6)+CONTAR(0,\"8\",2)+COUNT(B2)", {Value::num(11), std::nullopt}},
            {"=CONCAT(A1:B2,CONCATENAR(\"x\",\"y\"))", {Value::str("101100xy"), std::nullopt}},
            {"=LEN(UPPER(LOWER(\"Ab\")))+LARGO(MAYUSC(MINUSC(\"Cd\")))", {Value::num(4), std::nullopt}},
            {"=PROMEDIO(A1:A2,20)", {Value::num(15), std::nullopt}},
            {"=CONTAR(A1:B2,Z1)+COUNT(Z1:Z2)", {Value::num(4), std::nullopt}},
            {"=SUMA()+COUNT()", {Value::num(0), std::nullopt}},
            {"=CONCAT()", {Value::str(""), std::nullopt}},
            {"=PROMEDIO()", {std::nullopt, Error{typeid(std::runtime_error), "Faltan argumentos"}}},
            {"=LARGO(1,2)", {std::nullopt, Error{typeid(std::runtime_error), "LARGO requiere un argumento"}}},
            {"=DESCONOCIDA(1)", {std::nullopt, Error{typeid(std::runtime_error), "Funcion desconocida: DESCONOCIDA"}}},
            {"=DESCONOCIDA(C1)", {std::nullopt, Error{typeid(std::runtime_error), "Division por cero"}}},
            {"=SUMA(1,)", {std::nullopt, Error{typeid(std::runtime_error), "Expresion invalida en posicion 7"}}},
            {"=SUMA(1", {std::nullopt, Error{typeid(std::runtime_error), "Se esperaba ')' en posicion 6"}}},
            {"=SUMA(A2:A1)", {std::nullopt, Error{typeid(std::runtime_error), "Rango invalido o demasiado grande"}}},
            {"=SUMA(A1:A0)", {std::nullopt, Error{typeid(std::runtime_error), "Rango invalido"}}},
            {"=SUMA(A1:XFE1)", {std::nullopt, Error{typeid(std::runtime_error), "Columna fuera de rango"}}},
            {"=SUMA(A1:B50001)", {std::nullopt, Error{typeid(std::runtime_error), "Rango invalido o demasiado grande"}}},
            {"=SUMA(A1:Z99,)", {std::nullopt, Error{typeid(std::runtime_error), "Division por cero"}}},
            {"=SUMA(C1:C3)", {std::nullopt, Error{typeid(std::runtime_error), "Division por cero"}}},
            {"=SUMA(D1:D1)", {std::nullopt, Error{typeid(std::runtime_error), "Referencia circular: D1"}}},
            {"=C2", {std::nullopt, Error{typeid(std::runtime_error), "Referencia circular: C2"}}},
            {"=D1", {std::nullopt, Error{typeid(std::runtime_error), "Referencia circular: D1"}}},
            {"=1/0+D1", {std::nullopt, Error{typeid(std::runtime_error), "Division por cero"}}},
            {"=C1)", {std::nullopt, Error{typeid(std::runtime_error), "Division por cero"}}},
            {"=D1)", {std::nullopt, Error{typeid(std::runtime_error), "Referencia circular: D1"}}},
            {"=1/1)", {std::nullopt, Error{typeid(std::runtime_error), "Caracteres sobrantes en formula"}}},
            {"=\"x\"+C1", {std::nullopt, Error{typeid(std::runtime_error), "Se esperaba un numero: x"}}},
            {"=\"x\"-C1", {std::nullopt, Error{typeid(std::runtime_error), "Se esperaba un numero: x"}}},
            {"=\"x\"*C1", {std::nullopt, Error{typeid(std::runtime_error), "Se esperaba un numero: x"}}},
            {"=\"x\"/C1", {std::nullopt, Error{typeid(std::runtime_error), "Division por cero"}}},
            {"=Z0", {std::nullopt, Error{typeid(std::runtime_error), "Referencia o identificador desconocido: Z0"}}},
            {"=identificador", {std::nullopt, Error{typeid(std::runtime_error), "Referencia o identificador desconocido: IDENTIFICADOR"}}},
        };
        for (const auto& [raw, old] : cases) {
            sheet.set("D1", raw);
            const auto next = capture([&] { return sheet.calculate("D1"); });
            check(old.error == next.error && old.value.has_value() == next.value.has_value(),
                  std::string("Diferencia de error en ") + raw +
                  " | anterior: " + (old.error ? old.error->message : "valor") +
                  " | ahora: " + (next.error ? next.error->message : "valor"));
            if (old.value) {
                const auto& a = *old.value;
                const auto& b = *next.value;
                check(a.numeric == b.numeric && a.number == b.number && a.text == b.text &&
                      std::signbit(a.number) == std::signbit(b.number),
                      std::string("Diferencia de tipo o valor en ") + raw);
            }
        }
    }},
};

// ---------- celdas y literales ----------
const Test celdas[] = {
    {"direccion-z0", [] {
        Sheet sheet;
        expectError([&] { sheet.set("Z0", "10"); }, "Celda invalida: Z0");
        expectError([&] { sheet.calculate("Z0"); }, "Celda invalida");
        expectError([] { calculateFormula("=Z0"); },
                    "Referencia o identificador desconocido: Z0");
        check(sheet.size() == 0, "Se guardo una celda invalida");
    }},
    {"actualizacion-de-clave", [] {
        Sheet sheet;
        sheet.set("a1", "2");
        sheet.set("B1", "=A1+1");
        sheet.set("A1", "7");
        check(sheet.size() == 2 && sheet.get("a1") == "7",
              "Actualizar una clave existente debe conservar el numero de celdas");
        expectNumber(sheet.calculate("B1"), 8);
    }},
    {"lectura-ausente", [] {
        Sheet sheet;
        sheet.set("A1", "=B1+1");
        sheet.set("C1", "");
        const Sheet& view = sheet;
        check(view.get("b1").empty(), "Una celda ausente debe devolver contenido vacio");
        expectNumber(view.calculate("B1"), 0);
        expectNumber(view.calculate("A1"), 1);
        expectText(view.calculate("C1"), "");
        check(view.size() == 2, "Las consultas no deben agregar celdas");
        sheet.set("B1", "3");
        check(view.size() == 3, "La celda ausente debe contar como nueva al guardarla");
        expectNumber(view.calculate("A1"), 4);
    }},
    {"copia-independiente", [] {
        Sheet original;
        original.set("A1", "4");
        original.set("B1", "=A1+1");
        Sheet copied(original);
        copied.set("A1", "8");
        expectNumber(copied.calculate("B1"), 9);
        expectNumber(original.calculate("B1"), 5);

        Sheet assigned;
        assigned.set("C1", "anterior");
        assigned = original;
        check(assigned.size() == 2 && assigned.get("C1").empty(),
              "Asignar una copia debe reemplazar el contenido previo");
        original.set("A1", "10");
        expectNumber(assigned.calculate("B1"), 5);
        expectNumber(original.calculate("B1"), 11);
    }},
    {"movimiento-de-hoja", [] {
        Sheet source;
        source.set("A1", "4");
        source.set("B1", "=A1+1");
        Sheet moved(std::move(source));
        check(moved.size() == 2 && source.size() == 0,
              "La construccion por movimiento debe transferir las celdas");
        source.set("A1", "20");
        expectNumber(source.calculate("A1"), 20);
        expectNumber(moved.calculate("B1"), 5);

        Sheet assigned;
        assigned.set("C1", "anterior");
        assigned = std::move(moved);
        check(assigned.size() == 2 && assigned.get("C1").empty() && moved.size() == 0,
              "La asignacion por movimiento debe reemplazar el destino y vaciar el origen");
        moved.set("A1", "30");
        expectNumber(moved.calculate("A1"), 30);
        expectNumber(assigned.calculate("B1"), 5);
    }},
    {"hexadecimal-como-texto", [] {
        Sheet sheet;
        for (const char* raw : {"0x10", "0XFF", "-0x10"}) {
            sheet.set("A1", raw);
            expectText(sheet.calculate("A1"), raw);
        }
    }},
    {"literales-enteros-y-negativos", [] {
        Sheet sheet;
        sheet.set("A1", "10");
        sheet.set("A2", "-2.5");
        expectNumber(sheet.calculate("A1"), 10);
        expectNumber(sheet.calculate("A2"), -2.5);
    }},
    {"literales-fraccionarios", [] {
        Sheet sheet;
        sheet.set("A1", ".5");
        sheet.set("A2", "2.");
        expectNumber(sheet.calculate("A1"), 0.5);
        expectNumber(sheet.calculate("A2"), 2);
    }},
    {"literales-con-exponente", [] {
        Sheet sheet;
        sheet.set("A1", "1e3");
        sheet.set("A2", "+1.25E-2");
        expectNumber(sheet.calculate("A1"), 1000);
        expectNumber(sheet.calculate("A2"), 0.0125);
    }},
    {"literales-cero-y-cero-inicial", [] {
        Sheet sheet;
        sheet.set("A1", "0");
        sheet.set("A2", "010");
        expectNumber(sheet.calculate("A1"), 0);
        expectNumber(sheet.calculate("A2"), 10);
    }},
    {"literales-no-finitos-y-fuera-de-rango", [] {
        Sheet sheet;
        for (const char* raw : {"inf", "nan", "1e9999"}) {
            sheet.set("A1", raw);
            expectText(sheet.calculate("A1"), raw);
        }
    }},
    {"literales-decimales-malformados", [] {
        Sheet sheet;
        for (const char* raw : {"1e", "1.2.3", ".", "+.", "1e+"}) {
            sheet.set("A1", raw);
            expectText(sheet.calculate("A1"), raw);
        }
    }},
    {"literales-con-espacios", [] {
        Sheet sheet;
        sheet.set("A1", " \t10");
        expectNumber(sheet.calculate("A1"), 10);
        for (const char* raw : {"10 ", "1 0", "- 2"}) {
            sheet.set("A1", raw);
            expectText(sheet.calculate("A1"), raw);
        }
    }},
    {"contar-ignora-hexadecimal-literal", [] {
        Sheet sheet;
        sheet.set("A1", "0x10");
        sheet.set("A2", "0");
        sheet.set("B1", "=CONTAR(A1:A2)");
        sheet.set("B2", "=COUNT(A1,A2)");
        expectNumber(sheet.calculate("B1"), 1);
        expectNumber(sheet.calculate("B2"), 1);
    }},
    {"referencias-con-espacios-y-ceros", [] {
        Sheet sheet;
        sheet.set("B1", "2.5");
        sheet.set("A1", "= b1 + 000000000000001 ");
        expectNumber(sheet.calculate("A1"), 3.5);
        sheet.set("A1", "=B1+0000000000000001");
        expectNumber(sheet.calculate("A1"), 3.5);
        sheet.set("A1", "=B1+1e0");
        expectNumber(sheet.calculate("A1"), 3.5);
        sheet.set("B1", "0x10");
        expectNumber(sheet.calculate("A1"), 17);
    }},
    {"referencias-y-literales", [] {
        Sheet sheet;
        sheet.set("A1", "10");
        sheet.set("A2", "=A1+1");
        sheet.set("A3", "=(A2+1)");
        sheet.set("A4", "0x10");
        sheet.set("A5", R"(=A4&Z1)");
        expectNumber(sheet.calculate("a3"), 12);
        expectText(sheet.calculate("A4"), "0x10");
        expectText(sheet.calculate("A5"), "0x100");
    }},
};

// ---------- persistencia ----------
const Test persistencia[] = {
    {"persistencia", [] {
        TemporaryDirectory directory;
        const auto path = (directory.path() / "hoja.txt").string();
        Sheet sheet;
        const std::string text = R"(  Hola "UNI" \  )";
        const std::string formula = R"(=CONCAT(A2," + ","""listo"""))";
        sheet.set("A1", "010");
        sheet.set("A2", text);
        sheet.set("B1", "=A1*2");
        sheet.set("B2", formula);
        sheet.set("C1", "");
        sheet.save(path);

        Sheet loaded;
        loaded.set("D1", "anterior");
        loaded.load(path);

        check(loaded.size() == 5 && loaded.get("D1").empty(),
              "La carga debe reemplazar la hoja anterior");
        check(loaded.get("A1") == "010" && loaded.get("A2") == text &&
              loaded.get("B1") == "=A1*2" && loaded.get("B2") == formula,
              "La carga debe conservar el contenido original");
        expectNumber(loaded.calculate("B1"), 20);
        expectText(loaded.calculate("B2"), text + " + \"listo\"");
        expectText(loaded.calculate("C1"), "");
        loaded.set("A1", "12");
        expectNumber(loaded.calculate("B1"), 24);
    }},
    {"carga-corrupta-transaccional", [] {
        TemporaryDirectory directory;
        const auto path = (directory.path() / "hoja.txt").string();
        {
            std::ofstream out(path);
            out << "PC4TEXTO2\n[((\"A1\":\"99\"),0),((\"C1\":\"nueva\"),0),((\"D1\":\"sin cerrar\n";
            out.close();
            check(static_cast<bool>(out), "No se pudo escribir el archivo de prueba");
        }
        Sheet sheet;
        sheet.set("A1", "4");
        sheet.set("B1", "=A1+1");
        expectError([&] { sheet.load(path); }, "Archivo corrupto");
        check(sheet.size() == 2 && sheet.get("A1") == "4" &&
              sheet.get("B1") == "=A1+1" && sheet.get("C1").empty(),
              "Una fila corrupta debe preservar toda la hoja anterior");
        expectNumber(sheet.calculate("B1"), 5);
    }},
    {"persistencia-de-literales-decimales-y-hexadecimales", [] {
        TemporaryDirectory directory;
        const auto path = (directory.path() / "hoja.txt").string();
        Sheet sheet;
        sheet.set("A1", "0x10");
        sheet.set("A2", "010");
        sheet.save(path);

        Sheet loaded;
        loaded.load(path);
        check(loaded.get("A1") == "0x10" && loaded.get("A2") == "010",
              "La carga debe conservar la escritura original de ambos literales");
        expectText(loaded.calculate("A1"), "0x10");
        expectNumber(loaded.calculate("A2"), 10);
    }},
    {"persistencia-recalcula-al-cargar", [] {
        Sheet sheet;
        sheet.set("A1", "010");
        sheet.set("A3", "0");
        sheet.set("B1", "=SUMA(A1:A3)+1");
        sheet.set("C1", "=1/0");
        sheet.set("D1", "=B1+C1");
        expectError([&] { sheet.calculate("D1"); }, "Division por cero");
        sheet.set("C1", "=COUNT(A1:A3,A2)");
        TemporaryDirectory dir;
        const auto path = (dir.path() / "audit.txt").string();
        sheet.save(path);
        std::ifstream input(path);
        std::string header;
        std::getline(input, header);
        check(header == "PC4TEXTO2", "Cabecera de persistencia inesperada");
        Sheet loaded;
        loaded.load(path);
        check(loaded.get("A1") == "010" && loaded.get("C1") == "=COUNT(A1:A3,A2)",
              "La carga debe preservar contenido original");
        expectNumber(loaded.calculate("D1"), 14);
    }},
    {"persistencia-load-corrupto-no-borra", [] {
        TemporaryDirectory directory;
        const auto badHeader = (directory.path() / "cabecera.txt").string();
        const auto extraData = (directory.path() / "extra.txt").string();
        const auto badCell = (directory.path() / "celda.txt").string();
        const auto noTable = (directory.path() / "sin_tabla.txt").string();
        {
            std::ofstream out(badHeader);
            out << "OTRO_FORMATO\n[((\"A1\":\"1\"),0)]\n";
        }
        {
            std::ofstream out(extraData);
            out << "PC4TEXTO2\n[((\"A1\":\"1\"),0)] basura\n";
        }
        {
            std::ofstream out(badCell);
            out << "PC4TEXTO2\n[((\"A1\":\"1\"),0),((\"ZZ\":\"2\"),0)]\n";
        }
        {
            std::ofstream out(noTable);
            out << "PC4TEXTO2\n\"A1\" \"1\"\n";
        }
        Sheet sheet;
        sheet.set("A1", "4");
        sheet.set("B1", "=A1*2");
        expectError([&] { sheet.load(badHeader); }, "Formato de archivo invalido");
        expectError([&] { sheet.load(extraData); }, "Datos sobrantes al final del archivo");
        expectError([&] { sheet.load(badCell); }, "Celda corrupta: ZZ");
        expectError([&] { sheet.load(noTable); }, "Archivo corrupto");
        expectError([&] { sheet.load((directory.path() / "no_existe.txt").string()); },
                    "No se pudo abrir");
        check(sheet.size() == 2 && sheet.get("A1") == "4" && sheet.get("B1") == "=A1*2",
              "Un LOAD fallido no debe cambiar la hoja");
        expectNumber(sheet.calculate("B1"), 8);
    }},
    {"persistencia-comillas-dobles", [] {
        TemporaryDirectory directory;
        const auto path = (directory.path() / "comillas.txt").string();
        Sheet sheet;
        sheet.set("A1", R"(dijo "hola")");
        sheet.set("A2", R"(="a""b")");
        sheet.save(path);

        Sheet loaded;
        loaded.load(path);
        check(loaded.get("A1") == R"(dijo "hola")", "Se perdieron las comillas del texto");
        check(loaded.get("A2") == R"(="a""b")", "Se perdieron las comillas de la formula");
        expectText(loaded.calculate("A1"), R"(dijo "hola")");
        expectText(loaded.calculate("A2"), R"(a"b)");
    }},
    {"persistencia-espacios-con-hashtable", [] {
        TemporaryDirectory directory;
        const auto path = (directory.path() / "espacios.txt").string();
        Sheet sheet;
        const std::string spaces = R"(  dos  espacios "y comillas"  )";
        sheet.set("A1", spaces);
        sheet.set("A2", "");
        sheet.set("B1", R"(=CONCAT(A1,"|"))");
        sheet.save(path);

        // el archivo debe tener la cabecera y la tabla escrita con operator<< del HashTable
        std::ifstream input(path);
        std::string header;
        std::string table;
        std::getline(input, header);
        std::getline(input, table);
        check(header == "PC4TEXTO2", "Cabecera inesperada: " + header);
        check(table.starts_with("[((") && table.ends_with(")]"),
              "La tabla no tiene el formato de HashTable: " + table);

        Sheet loaded;
        loaded.load(path);
        check(loaded.size() == 3, "Se esperaban 3 celdas despues de LOAD");
        check(loaded.get("A1") == spaces, "Se perdieron espacios o comillas");
        check(loaded.get("A2").empty(), "La celda vacia debe seguir vacia");
        expectText(loaded.calculate("B1"), spaces + "|");
    }},
    {"persistencia-formato-exacto", [] {
        TemporaryDirectory directory;
        const auto path = (directory.path() / "formato.txt").string();
        Sheet sheet;
        sheet.set("A1", "10");
        sheet.set("B1", R"(=CONCAT("a b",A1))");
        sheet.save(path);
        std::ifstream input(path);
        const std::string content((std::istreambuf_iterator<char>(input)),
                                  std::istreambuf_iterator<char>());
        const std::string expected =
            "PC4TEXTO2\n[((\"B1\":\"=CONCAT(\\\"a b\\\",A1)\"),0),((\"A1\":\"10\"),0)]\n";
        check(content == expected, "Formato de archivo inesperado: " + content);
    }},
    {"persistencia-hoja-vacia", [] {
        TemporaryDirectory directory;
        const auto path = (directory.path() / "vacia.txt").string();
        Sheet empty;
        empty.save(path);
        Sheet loaded;
        loaded.set("A1", "1");
        loaded.load(path);
        check(loaded.size() == 0 && loaded.get("A1").empty(),
              "Cargar una hoja vacia debe dejar la hoja vacia");
    }},
};

struct TestGroup {
    std::string_view name;
    const Test* tests;
    std::size_t count;
};

const TestGroup groups[] = {
    {"funciones basicas", basicas, std::size(basicas)},
    {"parentesis", parentesis, std::size(parentesis)},
    {"rangos y celdas vacias", rangos, std::size(rangos)},
    {"ciclos y dependencias", ciclos, std::size(ciclos)},
    {"errores", errores, std::size(errores)},
    {"celdas y literales", celdas, std::size(celdas)},
    {"persistencia", persistencia, std::size(persistencia)},
};

// ---------- pruebas de profundidad (make test-depth) ----------

// formulas que encadenan cada celda con la siguiente
struct DepthFormula {
    std::string_view name;
    std::string (*formula)(const std::string& next);
    double (*expected)(int dependencies);
};

const DepthFormula depthFormulas[] = {
    {"binaria", [](const std::string& a) { return "=" + a + "+1"; },
        [](int n) { return n + 1.0; }},
    {"parentesis", [](const std::string& a) { return "=(" + a + "+1)"; },
        [](int n) { return n + 1.0; }},
    {"suma-escalar", [](const std::string& a) { return "=SUMA(" + a + ",1)"; },
        [](int n) { return n + 1.0; }},
    {"precedencia", [](const std::string& a) { return "=" + a + "+2*3"; },
        [](int n) { return 6.0 * n + 1; }},
    {"unaria", [](const std::string& a) { return "=-" + a; },
        [](int n) { return n % 2 == 0 ? 1.0 : -1.0; }},
    {"rango", [](const std::string& a) { return "=SUMA(" + a + ":" + a + ",1)"; },
        [](int n) { return n + 1.0; }},
};

// arma una cadena A1 -> A2 -> ... y revisa el resultado; mas de 1000 da error
void testDepth(int dependencies, const DepthFormula& family) {
    Sheet sheet;
    for (int row = 1; row <= dependencies; ++row) {
        sheet.set("A" + std::to_string(row), family.formula("A" + std::to_string(row + 1)));
    }
    sheet.set("A" + std::to_string(dependencies + 1), "1");
    if (dependencies <= 1000) {
        expectNumber(sheet.calculate("A1"), family.expected(dependencies));
    } else {
        expectError([&] { sheet.calculate("A1"); }, "Demasiadas dependencias");
    }
}

// con -v se muestra cada prueba; si no, solo los fallos y el resumen
bool verbose = false;

// corre una prueba; solo imprime si falla (o si estamos en modo -v)
bool run(std::string_view name, auto action) {
    try {
        action();
        if (verbose) {
            std::cout << "  " << styled("OK").bright_green().bold() << ' ' << name << '\n';
        }
        return true;
    } catch (const std::exception& error) {
        std::cout << "  " << styled("FALLO").bright_red().bold() << ' ' << name << ": "
                  << error.what() << '\n';
        return false;
    }
}

// una linea por grupo: "✔ grupo (23/23)" o "✘ grupo (22/23)"
void printSummary(std::string_view label, std::size_t passed, std::size_t total) {
    if (passed == total) {
        std::cout << styled("✔").bright_green().bold();
    } else {
        std::cout << styled("✘").bright_red().bold();
    }
    std::cout << ' ' << label << " (" << passed << '/' << total << ")\n";
}

// linea final con el total, en verde si todo paso y en rojo si no
void printTotal(std::size_t passed, std::size_t total, std::string_view what) {
    const std::string text = std::to_string(passed) + "/" + std::to_string(total) + " " +
                             std::string(what);
    if (passed == total) {
        std::cout << styled(text).bright_green().bold() << '\n';
    } else {
        std::cout << styled(text).bright_red().bold() << '\n';
    }
}

// nombre del grupo en modo -v
void printGroupName(std::string_view name) {
    if (verbose) {
        std::cout << styled(name).bright_cyan().bold() << '\n';
    }
}

// corre todas las familias con 100, 1000 y 5000 celdas encadenadas
int runDepthTests() {
    std::size_t passed = 0;
    std::size_t total = 0;
    for (const auto& family : depthFormulas) {
        printGroupName(family.name);
        std::size_t familyPassed = 0;
        std::size_t familyTotal = 0;
        for (const int depth : {100, 1000, 5000}) {
            const std::string name = std::string(family.name) + " " + std::to_string(depth);
            familyPassed += run(name, [&] { testDepth(depth, family); });
            ++familyTotal;
        }
        printSummary(family.name, familyPassed, familyTotal);
        passed += familyPassed;
        total += familyTotal;
    }
    printTotal(passed, total, "pruebas de profundidad aprobadas");
    return passed == total ? 0 : 1;
}
} // namespace

int main(int argc, char* argv[]) {
    // argumentos: "-v" muestra cada prueba y "profundidad" corre las cadenas largas
    bool depth = false;
    for (int i = 1; i < argc; ++i) {
        const std::string_view arg = argv[i];
        if (arg == "-v") {
            verbose = true;
        } else if (arg == "profundidad") {
            depth = true;
        } else {
            std::cerr << "Uso: " << argv[0] << " [-v] [profundidad]\n";
            return 2;
        }
    }

    // titulo con el mismo estilo de las demos
    std::cout << styled(" █ Pruebas PC4 Excel Texto ").pure_black().bg_bright_yellow() << "\n\n";
    if (depth) {
        return runDepthTests();
    }
    std::cout << "Funciones: SUMA, PROMEDIO, MIN, MAX, CONTAR, CONCAT, LARGO, MAYUSC, MINUSC\n";
    std::cout << "Operadores: + - * / & y parentesis\n\n";

    std::size_t passed = 0;
    std::size_t total = 0;
    for (const auto& group : groups) {
        printGroupName(group.name);
        std::size_t groupPassed = 0;
        for (std::size_t i = 0; i < group.count; ++i) {
            groupPassed += run(group.tests[i].name, group.tests[i].run);
        }
        printSummary(group.name, groupPassed, group.count);
        passed += groupPassed;
        total += group.count;
    }
    std::cout << '\n';
    printTotal(passed, total, "pruebas aprobadas");
    return passed == total ? 0 : 1;
}
