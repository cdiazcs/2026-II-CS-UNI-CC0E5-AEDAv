#include "../texto_excel.h"

#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <iterator>
#include <stdexcept>
#include <string>
#include <string_view>
#include <typeindex>
#include <utility>

namespace pc4texto {
struct SheetParserTestAccess {
    static Value calculate(const Sheet& sheet, const string& address) {
        detail::EvaluationContext context;
        return SheetParserTestAccess::calculate(sheet, address, context);
    }

    static Value calculate(const Sheet& sheet, const string& address, detail::EvaluationContext& context) {
        return sheet.evaluateWithContinuations(address, context);
    }
};
} // namespace pc4texto

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

const Test stable[] = {
    {"suma-rango", [] {
        Sheet sheet;
        sheet.set("A1", "10");
        sheet.set("A2", "20");
        sheet.set("A3", "=SUMA(A1:A2)");
        expectNumber(sheet.calculate("A3"), 30);
    }},
    {"aritmetica", [] {
        Sheet sheet;
        sheet.set("A1", "10");
        sheet.set("A2", "20");
        sheet.set("B1", "=(A1+2)*(A2-5)");
        expectNumber(sheet.calculate("B1"), 180);
    }},
    {"promedio-escalar", [] {
        Sheet sheet;
        sheet.set("A1", "10");
        sheet.set("A2", "20");
        sheet.set("B2", "=PROMEDIO(A1,A2,30)");
        expectNumber(sheet.calculate("B2"), 20);
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
    {"parentesis-sin-cierre", [] {
        expectError([] { calculateFormula("=(2+3"); }, "Se esperaba ')'");
    }},
    {"division-por-cero", [] {
        expectError([] { calculateFormula("=1/0"); }, "Division por cero");
    }},
    {"ciclo-b1-b2", [] {
        Sheet sheet;
        sheet.set("B1", "=B2");
        sheet.set("B2", "=B1");
        expectError([&] { sheet.calculate("B1"); }, "Referencia circular: B1");
        expectError([&] { sheet.calculate("B2"); }, "Referencia circular: B2");
    }},
    {"direccion-z0", [] {
        Sheet sheet;
        expectError([&] { sheet.set("Z0", "10"); }, "Celda invalida: Z0");
        expectError([&] { sheet.calculate("Z0"); }, "Celda invalida");
        expectError([] { calculateFormula("=Z0"); },
                    "Referencia o identificador desconocido: Z0");
        check(sheet.size() == 0, "Se guardo una celda invalida");
    }},
    {"rango-invertido", [] {
        expectError([] { calculateFormula("=SUMA(A3:A1)"); }, "Rango invalido");
        expectError([] { calculateFormula("=SUMA(C1:A1)"); }, "Rango invalido");
    }},
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
    {"carga-corrupta-transaccional", [] {
        TemporaryDirectory directory;
        const auto path = (directory.path() / "hoja.txt").string();
        {
            std::ofstream out(path);
            out << "PC4TEXTO1\n\"A1\" \"99\"\n\"C1\" \"nueva\"\n\"D1\" \"sin cerrar\n";
            out.close();
            check(static_cast<bool>(out), "No se pudo escribir el archivo de prueba");
        }
        Sheet sheet;
        sheet.set("A1", "4");
        sheet.set("B1", "=A1+1");
        expectError([&] { sheet.load(path); }, "Fila corrupta");
        check(sheet.size() == 2 && sheet.get("A1") == "4" &&
              sheet.get("B1") == "=A1+1" && sheet.get("C1").empty(),
              "Una fila corrupta debe preservar toda la hoja anterior");
        expectNumber(sheet.calculate("B1"), 5);
    }},
    {"crecimiento-de-argumentos", [] {
        expectNumber(calculateFormula(
            "=SUMA(1,2,3,4,5,6,7,8,9,10,11,12,13,14,15,16,17,18,19,20,21)"), 231);
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
    {"referencias-compartidas", [] {
        Sheet sheet;
        sheet.set("A1", "10");
        sheet.set("B1", "=A1+1");
        sheet.set("C1", "=A1+B1");
        sheet.set("D1", "=B1+C1");
        expectNumber(sheet.calculate("C1"), 21);
        expectNumber(sheet.calculate("D1"), 32);
    }},
    {"formula-reutilizada-en-rango", [] {
        Sheet sheet;
        sheet.set("A1", "1");
        sheet.set("B1", "=A1+1");
        sheet.set("C1", "=SUMA(B1,B1:B1)");
        expectNumber(sheet.calculate("C1"), 4);
    }},
    {"autorreferencia", [] {
        expectError([] { calculateFormula("=A1+1"); }, "Referencia circular: A1");
    }},
    {"error-en-dependencia-y-recalculo", [] {
        Sheet sheet;
        sheet.set("A1", "=1/0");
        sheet.set("B1", "=A1+1");
        expectError([&] { sheet.calculate("B1"); }, "Division por cero");
        sheet.set("A1", "=8/2");
        expectNumber(sheet.calculate("B1"), 5);
    }},
    {"promedio-rango-vacio", [] {
        expectNumber(calculateFormula("=PROMEDIO(Z1:Z3,10)"), 10);
    }},
    {"average-rango-vacio", [] {
        expectNumber(calculateFormula("=AVERAGE(Z1:Z3,10)"), 10);
    }},
    {"promedio-sin-valores", [] {
        expectError([] { calculateFormula("=PROMEDIO(Z1:Z3)"); }, "Faltan argumentos");
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
        expectNumber(sheet.calculate("B2"), 0);
        expectNumber(sheet.calculate("B3"), 8);
        expectText(sheet.calculate("B4"), "x80y");
        sheet.set("A1", "-8");
        expectNumber(sheet.calculate("B3"), 0);
    }},
    {"promedio-anidado", [] {
        expectNumber(calculateFormula("=SUMA(PROMEDIO(Z1:Z3,10),MIN(Z1:Z3))"), 10);
        expectNumber(calculateFormula("=AVERAGE(SUMA(Z1:Z3),10)"), 5);
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
    {"contar-texto-numerico", [] {
        Sheet sheet;
        sheet.set("A1", R"(="8")");
        sheet.set("B1", R"(=CONTAR("8",A1,A1:A1,8))");
        expectNumber(sheet.calculate("B1"), 1);
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
    {"contar-funciones-anidadas", [] {
        expectNumber(calculateFormula(R"(=CONTAR(SUMA(1,2),CONCAT("1","2"),CONTAR()))"), 2);
    }},
    {"contar-sin-argumentos", [] {
        expectNumber(calculateFormula("=CONTAR()"), 0);
        expectNumber(calculateFormula("=COUNT()"), 0);
    }},
    {"contar-propaga-error", [] {
        Sheet sheet;
        sheet.set("A1", "=1/0");
        sheet.set("B1", "=CONTAR(A1:A1)");
        expectError([&] { sheet.calculate("B1"); }, "Division por cero");
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
    {"compatibilidad-de-conversiones-en-formulas", [] {
        expectNumber(calculateFormula("=1e3-(-.5)*2."), 1001);
        expectNumber(calculateFormula("=0x10"), 16);
        expectNumber(calculateFormula(R"(=SUMA("0x10",1))"), 17);
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
    {"ciclo-indirecto-de-tres-celdas", [] {
        Sheet sheet;
        sheet.set("A1", "=B1+1");
        sheet.set("B1", "=C1+1");
        sheet.set("C1", "=SUMA(A1:A1)");
        expectError([&] { sheet.calculate("A1"); }, "Referencia circular: A1");
        expectError([&] { sheet.calculate("B1"); }, "Referencia circular: B1");
        expectError([&] { sheet.calculate("C1"); }, "Referencia circular: C1");
    }},
    {"dependencia-compartida-en-diamante", [] {
        Sheet sheet;
        sheet.set("A1", "=2*3");
        sheet.set("B1", "=A1+1");
        sheet.set("C1", "=A1*2");
        sheet.set("D1", "=B1+C1");
        expectNumber(sheet.calculate("D1"), 19);
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
    {"primer-error-en-rango", [] {
        Sheet sheet;
        sheet.set("B1", "=1/0");
        sheet.set("A2", "=(2+3");
        sheet.set("C1", "=SUMA(A1:B2)");
        expectError([&] { sheet.calculate("C1"); }, "Division por cero");
        sheet.set("B1", "1");
        expectError([&] { sheet.calculate("C1"); }, "Se esperaba ')'");
    }},
    {"contexto-y-estados-de-evaluacion", [] {
        using namespace pc4texto::detail;
        EvaluationContext context;
        auto& record = context.cell("A1");
        check(record.state == EvaluationState::NoVisitada && !record.result,
              "Una celda nueva debe quedar sin visitar y sin resultado");
        record.state = EvaluationState::Activa;
        for (int row = 2; row <= 64; ++row) {
            context.cell("A" + std::to_string(row));
        }
        check(&context.cell("A1") == &record && record.state == EvaluationState::Activa,
              "El registro activo debe conservar su dirección al crecer el contexto");
        record.result = Value::num(0);
        record.state = EvaluationState::Completada;
        const auto& reused = context.cell("A1");
        check(reused.state == EvaluationState::Completada && reused.result.has_value(),
              "El registro completado debe conservar su resultado");
        expectNumber(*reused.result, 0);
    }},
    {"construccion-y-transferencia-del-marco", [] {
        using namespace pc4texto::detail;
        EvaluationContext context;
        auto owned = std::make_unique<FormulaFrame>("A1", "B1+1");
        auto* frame = owned.get();
        check(frame->address == "A1" && frame->formula == "B1+1" &&
              frame->position == 0 && frame->expression.empty() && !frame->result,
              "El marco debe comenzar en la posición inicial");
        frame->position = 2;
        context.frames.push_back(std::move(owned));
        for (int row = 1; row <= 64; ++row) {
            context.frames.push_back(std::make_unique<FormulaFrame>(
                "B" + std::to_string(row), "1"));
        }
        check(!owned && context.frames.front().get() == frame && frame->position == 2,
              "Mover el propietario y crecer la pila debe preservar el marco");
        context.frames.resize(1);
        auto released = std::move(context.frames.back());
        context.frames.pop_back();
        released->result = Value::num(7);
        check(context.frames.empty(), "La extracción debe vaciar la pila");
        expectNumber(*released->result, 7);
    }},
    {"continuaciones-con-argumentos-y-rango", [] {
        using namespace pc4texto::detail;
        FormulaFrame frame("A1", "SUMA(B1:B2,MAX(1,2))");
        auto& outer = frame.expression.emplace_back(Rule::Primary);
        outer.step = Step::RangeValue;
        outer.left = Value::num(1);
        outer.op = '+';
        outer.call = std::make_unique<FunctionCall>();
        outer.call->name = "SUMA";
        outer.call->range = RangeCursor{2, 2, 2, 2, 1};
        outer.call->arguments.push_back(Value::num(0), 1);
        const auto* function = outer.call.get();
        for (int i = 0; i < 20; ++i) {
            auto& nested = frame.expression.emplace_back(Rule::Primary);
            nested.call = std::make_unique<FunctionCall>();
            nested.call->name = "MAX";
            nested.call->arguments.push_back(Value::num(2));
        }
        const auto& saved = frame.expression.front();
        check(saved.call.get() == function && saved.step == Step::RangeValue &&
              saved.call->range->column == 2 && saved.call->range->row == 1 &&
              saved.call->arguments[0].getRef() == 1 && saved.op == '+',
              "Las continuaciones anidadas deben conservar argumentos, procedencia y cursor");
        expectNumber(saved.left, 1);
        expectNumber(frame.expression.back().call->arguments[0].getValue(), 2);
    }},
    {"limpieza-del-contexto-tras-excepcion", [] {
        using namespace pc4texto::detail;
        expectError([] {
            EvaluationContext context;
            for (int row = 1; row <= 128; ++row) {
                const auto address = "A" + std::to_string(row);
                context.cell(address).state = EvaluationState::Activa;
                auto frame = std::make_unique<FormulaFrame>(address, "SUMA(B1)");
                auto& pending = frame->expression.emplace_back(Rule::Primary);
                pending.call = std::make_unique<FunctionCall>();
                pending.call->arguments.push_back(Value::str(std::string(128, 'x')));
                context.frames.push_back(std::move(frame));
            }
            throw std::runtime_error("Interrupcion de prueba");
        }, "Interrupcion de prueba");
        EvaluationContext next;
        check(next.frames.empty() && next.cell("A1").state == EvaluationState::NoVisitada,
              "Un contexto nuevo debe quedar aislado del cálculo interrumpido");
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
    {"rutas-aritmeticas-equivalentes", [] {
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
    {"ciclo-y-diamante-solo-con-binarias", [] {
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
    {"errores-y-seleccion-de-ruta", [] {
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
    {"equivalencia-de-familias-auditadas", [] {
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
    {"autorreferencia-en-ambas-rutas", [] {
        for (const char* formula : {"=A1", "=(A1)"}) {
            expectError([&] { calculateFormula(formula); }, "Referencia circular: A1");
        }
    }},
    {"prioridad-entre-division-y-ciclo-en-ambas-rutas", [] {
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
    {"prioridad-tras-rechazo-de-dependencia", [] {
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
    {"bordes-del-subconjunto-iterativo", [] {
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
    {"continuaciones-precedencia-y-parentesis", [] {
        expectExpression("2+3*4", Value::num(14));
        expectExpression("(2+3)*4", Value::num(20));
        expectExpression("((2+3)*(4-1))/5", Value::num(3));
        expectExpression("20/2/2-1", Value::num(4));
    }},
    {"continuaciones-unarios-y-decimales", [] {
        expectExpression(" - + - .5e1 + 2. ", Value::num(7));
        expectExpression("1E+2 / 2e1", Value::num(5));
        expectExpression("0x10", Value::num(16));
    }},
    {"continuaciones-concatenacion-y-comillas", [] {
        expectExpression(R"("a""b" & 2*3 & "")", Value::str("a\"b6"));
        expectExpression(R"("1"&2+3)", Value::num(15));
    }},
    {"continuaciones-errores-de-sintaxis-y-division", [] {
        expectExpressionError("1/0)", "Division por cero");
        expectExpressionError("(2+3", "Se esperaba ')' en posicion 4");
        expectExpressionError("2+3)", "Caracteres sobrantes en formula");
        expectExpressionError("\"ab", "Cadena sin cerrar");
    }},
    {"continuaciones-orden-de-coercion", [] {
        for (const char op : {'+', '-', '*'}) {
            const std::string body = std::string("\"x\"") + op + "(1/0)";
            expectExpressionError(body, "Se esperaba un numero: x");
        }
        expectExpressionError(R"("x"/0)", "Division por cero");
        expectExpressionError(R"(1+"x")", "Se esperaba un numero: x");
    }},
    {"continuaciones-anidamiento-profundo", [] {
        const std::string body = std::string(20000, '(') + "1" + std::string(20000, ')');
        expectNumber(calculateFormula("=" + body), 1);
    }},
    {"continuaciones-referencias-y-literales", [] {
        using pc4texto::SheetParserTestAccess;
        Sheet sheet;
        sheet.set("A1", "10");
        sheet.set("A2", "=A1+1");
        sheet.set("A3", "=(A2+1)");
        sheet.set("A4", "0x10");
        sheet.set("A5", R"(=A4&Z1)");
        expectNumber(SheetParserTestAccess::calculate(sheet, "a3"), 12);
        expectText(SheetParserTestAccess::calculate(sheet, "A4"), "0x10");
        expectText(SheetParserTestAccess::calculate(sheet, "A5"), "0x100");
    }},
    {"continuaciones-ciclos-y-prioridad", [] {
        using pc4texto::SheetParserTestAccess;
        Sheet sheet;
        sheet.set("A1", "=1/0+A1");
        expectError([&] { SheetParserTestAccess::calculate(sheet, "A1"); }, "Division por cero");
        sheet.set("A1", "=A1)");
        expectError([&] { SheetParserTestAccess::calculate(sheet, "A1"); }, "Referencia circular: A1");
        sheet.set("A1", "=(A2)");
        sheet.set("A2", "=-A3");
        sheet.set("A3", "=A1+2*3");
        expectError([&] { SheetParserTestAccess::calculate(sheet, "A2"); }, "Referencia circular: A2");
        sheet.set("A3", "=A4)");
        sheet.set("A4", "=1/0");
        expectError([&] { SheetParserTestAccess::calculate(sheet, "A3"); }, "Division por cero");
        sheet.set("A4", "1");
        expectError([&] { SheetParserTestAccess::calculate(sheet, "A3"); }, "Caracteres sobrantes en formula");
    }},
    {"continuaciones-diamante-y-cache", [] {
        using pc4texto::SheetParserTestAccess;
        using namespace pc4texto::detail;
        Sheet sheet;
        sheet.set("A1", "=2*3");
        sheet.set("B1", "=A1+1");
        sheet.set("C1", "=A1*2");
        sheet.set("D1", "=B1+C1");
        expectNumber(SheetParserTestAccess::calculate(sheet, "D1"), 19);
        // Un registro ya completado permite observar si ambos caminos reutilizan el valor.
        EvaluationContext context;
        auto& shared = context.cell("A1");
        shared.result = Value::num(4);
        shared.state = EvaluationState::Completada;
        expectNumber(SheetParserTestAccess::calculate(sheet, "D1", context), 13);
        expectNumber(*shared.result, 4);
    }},
    {"continuaciones-error-profundo-y-recalculo", [] {
        using pc4texto::SheetParserTestAccess;
        Sheet sheet;
        constexpr int depth = 5000;
        for (int row = 1; row <= depth; ++row)
            sheet.set("A" + std::to_string(row), "=-(-A" + std::to_string(row + 1) + ")+2*3");
        const auto leaf = "A" + std::to_string(depth + 1);
        sheet.set(leaf, "=1/0");
        expectError([&] { SheetParserTestAccess::calculate(sheet, "A1"); }, "Division por cero");
        sheet.set(leaf, "1");
        expectNumber(SheetParserTestAccess::calculate(sheet, "A1"), 30001);
    }},
    {"continuaciones-funciones-escalares", [] {
        expectExpression("SUMA(2,3)", Value::num(5));
        expectExpression("PROMEDIO(2,4)", Value::num(3));
        expectExpression(R"(CONTAR(2,0,"texto"))", Value::num(2));
        expectExpression(R"(CONCAT("A","B"))", Value::str("AB"));
        expectExpression("sUm(2;3)+AVERAGE(2,4)+COUNT(0)", Value::num(9));
        expectExpression("MAX(2,3)-MIN(2,3)", Value::num(1));
        expectExpression(R"(LEN(UPPER(LOWER("Ab")))+LARGO(MAYUSC(MINUSC("Cd"))))", Value::num(4));
        expectExpression(R"(CONCATENAR("A";"B"))", Value::str("AB"));
    }},
    {"continuaciones-funciones-anidadas-y-aritmetica", [] {
        expectExpression("SUMA(2,PROMEDIO(4,6))", Value::num(7));
        expectExpression("SUMA(2,3)*2", Value::num(10));
        expectExpression("SUMA(1;2,3)", Value::num(6));
        expectExpression("SUMA()+COUNT()", Value::num(0));
        expectExpression("CONCAT()", Value::str(""));
        constexpr int depth = 20000;
        std::string nested;
        for (int i = 0; i < depth; ++i) nested += "SUMA(";
        nested += "1";
        nested.append(depth, ')');
        expectNumber(calculateFormula("=" + nested), 1);
    }},
    {"continuaciones-funciones-referencias-y-ciclos", [] {
        using pc4texto::SheetParserTestAccess;
        Sheet sheet;
        sheet.set("A1", "=2*3");
        sheet.set("A2", "=SUMA(1,PROMEDIO(A1,2),A1+1,Z1)");
        expectNumber(SheetParserTestAccess::calculate(sheet, "A2"), 12);
        expectNumber(sheet.calculate("A2"), 12);
        sheet.set("A1", "=SUMA(A2,1)");
        expectError([&] { SheetParserTestAccess::calculate(sheet, "A2"); }, "Referencia circular: A2");
        expectError([&] { sheet.calculate("A2"); }, "Referencia circular: A2");
    }},
    {"continuaciones-funciones-orden-de-errores", [] {
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
    {"continuaciones-rangos-ausencias-y-ceros", [] {
        using pc4texto::SheetParserTestAccess;
        Sheet sheet;
        sheet.set("A1", "10");
        sheet.set("A3", "20");
        const auto verify = [&](const char* formula, double expected) {
            sheet.set("B1", formula);
            expectNumber(sheet.calculate("B1"), expected);
            expectNumber(SheetParserTestAccess::calculate(sheet, "B1"), expected);
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
        expectError([&] { sheet.calculate("B1"); }, "Faltan argumentos");
        expectError([&] { SheetParserTestAccess::calculate(sheet, "B1"); }, "Faltan argumentos");
    }},
    {"continuaciones-rango-bidimensional-y-reanudacion", [] {
        using pc4texto::SheetParserTestAccess;
        Sheet sheet;
        sheet.set("D1", R"(=CONCAT("a","b"))");
        sheet.set("A1", "=D1");
        sheet.set("B1", R"(=D1&"c")");
        sheet.set("B2", "0");
        sheet.set("C1", R"(=CONCAT("[",A1:B2;CONCAT(":",B1:B1),"]"))");
        expectText(sheet.calculate("C1"), "[ababc00:abc]");
        expectText(SheetParserTestAccess::calculate(sheet, "C1"), "[ababc00:abc]");
    }},
    {"continuaciones-rangos-orden-de-errores", [] {
        using pc4texto::SheetParserTestAccess;
        Sheet sheet;
        sheet.set("A1", "10");
        sheet.set("A2", "=1/0");
        sheet.set("A3", "=B1");
        sheet.set("B1", "=SUMA(A1:A3)");
        const auto fails = [&](const char* error) {
            expectError([&] { sheet.calculate("B1"); }, error);
            expectError([&] { SheetParserTestAccess::calculate(sheet, "B1"); }, error);
        };
        fails("Division por cero");
        sheet.set("A2", "20");
        fails("Referencia circular: B1");
        sheet.set("B1", "=SUMA(A3:A1)");
        fails("Rango invalido o demasiado grande");
    }},
    {"continuaciones-rangos-validacion-y-cierre", [] {
        using pc4texto::SheetParserTestAccess;
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
        expectError([&] { SheetParserTestAccess::calculate(sheet, "B1"); }, "Se esperaba ')' en posicion 10");
    }},
    {"auditoria-regresion-de-formulas", [] {
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
        // Capturado de calculateLegacy con GCC y -O2 antes de retirar el oráculo vivo.
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
                  " | continuaciones: " + (next.error ? next.error->message : "valor"));
            if (old.value) {
                const auto& a = *old.value;
                const auto& b = *next.value;
                check(a.numeric == b.numeric && a.number == b.number && a.text == b.text &&
                      std::signbit(a.number) == std::signbit(b.number),
                      std::string("Diferencia de tipo o valor en ") + raw);
            }
        }
    }},
    {"auditoria-contexto-fallido-y-persistencia", [] {
        using pc4texto::SheetParserTestAccess;
        using namespace pc4texto::detail;
        Sheet sheet;
        sheet.set("A1", "010");
        sheet.set("A3", "0");
        sheet.set("B1", "=SUMA(A1:A3)+1");
        sheet.set("C1", "=1/0");
        sheet.set("D1", "=B1+C1");
        {
            EvaluationContext context;
            expectError([&] { SheetParserTestAccess::calculate(sheet, "D1", context); }, "Division por cero");
            check(!context.cell("C1").result && !context.cell("D1").result,
                  "No se debe publicar un resultado fallido");
        }
        sheet.set("C1", "=COUNT(A1:A3,A2)");
        TemporaryDirectory dir;
        const auto path = (dir.path() / "audit.txt").string();
        sheet.save(path);
        std::ifstream input(path);
        std::string header;
        std::getline(input, header);
        check(header == "PC4TEXTO1", "Cabecera de persistencia inesperada");
        Sheet loaded;
        loaded.load(path);
        check(loaded.get("A1") == "010" && loaded.get("C1") == "=COUNT(A1:A3,A2)",
              "La carga debe preservar contenido original");
        expectNumber(loaded.calculate("D1"), 14);
    }},
    {"calculate-cache-local-y-recalculo", [] {
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
};

struct DepthFormula {
    std::string_view name;
    std::string (*formula)(const std::string& address);
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

void testDepth(int dependencies, const DepthFormula& family, bool continuations = false) {
    Sheet sheet;
    for (int row = 1; row <= dependencies; ++row) {
        sheet.set("A" + std::to_string(row), family.formula("A" + std::to_string(row + 1)));
    }
    sheet.set("A" + std::to_string(dependencies + 1), "1");
    expectNumber(continuations ? pc4texto::SheetParserTestAccess::calculate(sheet, "A1")
                               : sheet.calculate("A1"), family.expected(dependencies));
}

template <int dependencies>
void testBinaryDepth() {
    testDepth(dependencies, depthFormulas[0]);
}

template <bool cycle>
void testDeepFailure() {
    Sheet sheet;
    for (int row = 1; row <= 20000; ++row)
        sheet.set("A" + std::to_string(row), "=(A" + std::to_string(row + 1) + "+1)");
    sheet.set("A20001", cycle ? "=A1" : "=1/0");
    expectError([&] { sheet.calculate("A1"); },
                cycle ? "Referencia circular: A1" : "Division por cero");
}

const Test isolatedDepths[] = {
    {"100-dependencias", testBinaryDepth<100>},
    {"1000-dependencias", testBinaryDepth<1000>},
    {"5000-dependencias", testBinaryDepth<5000>},
    {"20000-dependencias", testBinaryDepth<20000>},
    {"20000-ciclo", testDeepFailure<true>},
    {"20000-excepcion", testDeepFailure<false>},
};

bool run(std::string_view name, auto action) {
    try {
        action();
        std::cout << "OK " << name << '\n';
        return true;
    } catch (const std::exception& error) {
        std::cerr << "FALLO " << name << ": " << error.what() << '\n';
        return false;
    }
}
} // namespace

int main(int argc, char* argv[]) {
    if (argc == 3 || (argc == 4 && std::string_view(argv[3]) == "continuaciones")) {
        for (const auto& family : depthFormulas) {
            if (family.name != argv[1]) continue;
            for (const int depth : {100, 1000, 5000, 20000}) {
                if (std::to_string(depth) != argv[2]) continue;
                return run(family.name, [&] { testDepth(depth, family, argc == 4); }) ? 0 : 1;
            }
        }
    }
    if (argc == 2) {
        for (const auto& test : isolatedDepths) {
            if (test.name == argv[1]) {
                return run(test.name, test.run) ? 0 : 1;
            }
        }
    }
    if (argc != 1) {
        std::cerr << "Caso de profundidad desconocido o numero de argumentos incorrecto\n";
        return 2;
    }

    std::size_t passed = 0;
    for (const auto& test : stable) {
        passed += run(test.name, test.run);
    }
    std::cout << passed << '/' << std::size(stable) << " pruebas estables aprobadas\n";
    return passed == std::size(stable) ? 0 : 1;
}
