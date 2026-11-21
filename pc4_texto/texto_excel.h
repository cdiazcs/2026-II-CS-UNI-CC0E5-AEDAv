#ifndef PC4_TEXTO_EXCEL_H
#define PC4_TEXTO_EXCEL_H

// el warning de parametro sin usar viene de containers/ (codigo compartido), no de este modulo
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wunused-parameter"
#include "containers/hash_table.h"
#pragma GCC diagnostic pop

#include <algorithm>
#include <cctype>
#include <cmath>
#include <fstream>
#include <iomanip>
#include <memory>
#include <optional>
#include <regex>
#include <sstream>
#include <stdexcept>
#include <string>
#include <utility>
#include <vector>

namespace pc4texto {
using std::string;

// valor de una celda: puede ser numero o texto
struct Value {
    string text;
    bool numeric = false;
    double number = 0;

    // crea un valor numerico (no acepta infinito ni NaN)
    static Value num(double n) {
        if (!std::isfinite(n)) throw std::runtime_error("Resultado no finito");
        return {"", true, n};
    }

    // crea un valor de texto
    static Value str(string s) {
        return {std::move(s), false, 0};
    }

    // texto que se muestra al usuario
    string display() const {
        if (!numeric) return text;
        std::ostringstream out;
        out << std::setprecision(14) << number;
        return out.str();
    }

    // convierte a numero; si es texto que no es numero lanza error
    double asNumber() const {
        if (numeric) return number;
        try {
            size_t used = 0;
            const double n = std::stod(text, &used);
            if (used == text.size() && std::isfinite(n)) return n;
        } catch (const std::logic_error&) {
        }
        throw std::runtime_error("Se esperaba un numero: " + text);
    }
};

// la hoja de calculo: guarda el texto de cada celda y evalua formulas
class Sheet {
public:
    Sheet() = default;

    // copia: pasamos celda por celda a una tabla nueva
    Sheet(const Sheet& other) {
        for (const auto& node : other.state_->cells) {
            const auto entry = node.getValue();
            state_->set(entry.getKey(), entry.getValue());
        }
    }

    // movimiento: intercambiamos las tablas (la otra hoja queda vacia)
    Sheet(Sheet&& other) {
        state_.swap(other.state_);
    }

    Sheet& operator=(const Sheet& other) {
        Sheet copy(other);
        state_.swap(copy.state_);
        return *this;
    }

    Sheet& operator=(Sheet&& other) {
        Sheet moved(std::move(other));
        state_.swap(moved.state_);
        return *this;
    }

    // guarda el contenido de una celda (texto, numero o formula con '=')
    void set(string cell, const string& raw) {
        cell = upper(cell);
        if (!validCell(cell)) throw std::runtime_error("Celda invalida: " + cell);
        coords(cell);  // solo para revisar que este dentro de los limites
        state_->set(cell, raw);
    }

    // devuelve el contenido tal cual se escribio (sin calcular)
    string get(const string& cell) const {
        const string* raw = findRaw(upper(cell));
        return raw ? *raw : "";
    }

    // calcula el valor de una celda
    Value calculate(const string& cell) const {
        const string name = upper(cell);
        if (!validCell(name)) throw std::runtime_error("Celda invalida");
        Evaluator evaluator(*this);
        return evaluator.cellValue(name);
    }

    // cantidad de celdas guardadas
    size_t size() const {
        return state_->count;
    }

    // guarda la hoja: una cabecera y la tabla con nuestro operator<<
    void save(const string& path) const {
        std::ofstream out(path, std::ios::binary);
        if (!out) throw std::runtime_error("No se pudo guardar: " + path);
        // queda como [(("A1":"10"),0),...]; los textos van entre comillas con std::quoted
        out << "PC4TEXTO2\n" << state_->cells << '\n';
        if (!out) throw std::runtime_error("Error durante escritura");
    }

    // carga una hoja con nuestro operator>>; solo reemplaza la hoja si todo salio bien
    void load(const string& path) {
        std::ifstream in(path, std::ios::binary);
        if (!in) throw std::runtime_error("No se pudo abrir: " + path);
        string header;
        std::getline(in, header);
        if (header != "PC4TEXTO2") throw std::runtime_error("Formato de archivo invalido");

        // leemos en una tabla aparte; si algo falla la hoja actual no cambia
        in >> std::ws;
        if (in.peek() != '[') throw std::runtime_error("Archivo corrupto");
        auto loaded = std::make_unique<CellState>();
        in >> loaded->cells;
        if (!in) throw std::runtime_error("Archivo corrupto");
        in >> std::ws;
        if (!in.eof()) throw std::runtime_error("Datos sobrantes al final del archivo");

        // revisamos que todas las claves sean celdas validas y las contamos
        for (const auto& node : loaded->cells) {
            const string cell = node.getValue().getKey();
            if (!validCell(cell)) throw std::runtime_error("Celda corrupta: " + cell);
            coords(cell);
            ++loaded->count;
        }
        state_.swap(loaded);
    }

private:
    using CellTable = HashTable<HashTableTraits<string, string>>;

    // guardamos las celdas en nuestro HashTable (por dentro es un AVL): "A1" -> contenido
    struct CellState {
        // mutable porque contains y operator[] no son const (bloquean el mutex)
        // y los usamos tambien en metodos const como get y calculate
        mutable CellTable cells;
        size_t count = 0;

        void set(const string& cell, const string& raw) {
            if (!cells.contains(cell)) ++count;
            cells[cell] = raw;
        }
    };

    // usamos un puntero para cambiar toda la tabla de una vez (en LOAD y al mover)
    std::unique_ptr<CellState> state_ = std::make_unique<CellState>();

    // busca el contenido de una celda; nullptr si no existe
    const string* findRaw(const string& cell) const {
        const CellState& state = *state_;
        if (!state.cells.contains(cell)) return nullptr;
        return &state.cells[cell];
    }

    static string upper(string s) {
        for (char& c : s) c = static_cast<char>(std::toupper(static_cast<unsigned char>(c)));
        return s;
    }

    static string lower(string s) {
        for (char& c : s) c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
        return s;
    }

    // revisa que el nombre sea letras mayusculas y luego numeros, como "A1" o "BC12"
    static bool validCell(const string& name) {
        static const std::regex pattern("[A-Z]+[1-9][0-9]*");
        return std::regex_match(name, pattern);
    }

    // convierte "B3" en (columna 2, fila 3)
    static std::pair<int, int> coords(const string& name) {
        int col = 0;
        size_t i = 0;
        while (i < name.size() && std::isalpha(static_cast<unsigned char>(name[i]))) {
            col = col * 26 + (name[i] - 'A' + 1);
            ++i;
            if (col > 16384) throw std::runtime_error("Columna fuera de rango");
        }
        const long row = std::stol(name.substr(i));
        if (row < 1 || row > 1048576) throw std::runtime_error("Fila fuera de rango");
        return {col, static_cast<int>(row)};
    }

    // convierte (columna 2, fila 3) en "B3"
    static string cellName(int col, int row) {
        string letters;
        while (col > 0) {
            --col;
            letters.insert(letters.begin(), static_cast<char>('A' + col % 26));
            col /= 26;
        }
        return letters + std::to_string(row);
    }

    // valor de una celda sin formula: numero decimal como "12", "-3.5" o "1e3"; si no, texto
    static Value literalValue(const string& raw) {
        static const std::regex decimal(R"(\s*[+-]?([0-9]+\.?[0-9]*|\.[0-9]+)([eE][+-]?[0-9]+)?)");
        if (std::regex_match(raw, decimal)) {
            try {
                size_t used = 0;
                const double n = std::stod(raw, &used);
                if (used == raw.size() && std::isfinite(n)) return Value::num(n);
            } catch (const std::logic_error&) {
                // fuera de rango: se queda como texto
            }
        }
        return Value::str(raw);
    }

    // un argumento de funcion; emptyCell es true si vino de una celda vacia de un rango
    struct Argument {
        Value value;
        bool emptyCell = false;
    };
    using Arguments = std::vector<Argument>;

    // SUMA, PROMEDIO, MIN y MAX; menos SUMA, todas ignoran las celdas vacias del rango
    static Value aggregate(const string& name, const Arguments& args) {
        const bool sum = name == "SUMA" || name == "SUM";
        const bool average = name == "PROMEDIO" || name == "AVERAGE";
        if (args.empty() && !sum) throw std::runtime_error("Faltan argumentos");
        double result = 0;
        size_t used = 0;
        for (const Argument& arg : args) {
            if (!sum && arg.emptyCell) continue;
            const double x = arg.value.asNumber();
            if (used == 0) {
                result = x;
            } else if (name == "MIN") {
                result = std::min(result, x);
            } else if (name == "MAX") {
                result = std::max(result, x);
            } else {
                result += x;
            }
            ++used;
        }
        // si no quedo ningun valor: PROMEDIO da error y las demas dan 0
        if (used == 0 && average) throw std::runtime_error("Division por cero");
        if (average) result /= used;
        return Value::num(result);
    }

    // CONTAR: cuenta solo los numeros; las celdas vacias no cuentan
    static Value count(const Arguments& args) {
        size_t total = 0;
        for (const Argument& arg : args) {
            if (!arg.emptyCell && arg.value.numeric) ++total;
        }
        return Value::num(static_cast<double>(total));
    }

    // para LARGO, MAYUSC y MINUSC: revisa que haya un solo argumento y lo da como texto
    static string onlyText(const string& name, const Arguments& args) {
        if (args.size() != 1) throw std::runtime_error(name + " requiere un argumento");
        return args[0].value.display();
    }

    // ejecuta una funcion con sus argumentos ya calculados
    static Value callFunction(const string& name, const Arguments& args) {
        if (name == "SUMA" || name == "SUM" || name == "PROMEDIO" || name == "AVERAGE" ||
            name == "MIN" || name == "MAX") {
            return aggregate(name, args);
        }
        if (name == "CONTAR" || name == "COUNT") return count(args);
        if (name == "CONCAT" || name == "CONCATENAR") {
            string joined;
            for (const Argument& arg : args) joined += arg.value.display();
            return Value::str(joined);
        }
        if (name == "LARGO" || name == "LEN") {
            return Value::num(static_cast<double>(onlyText("LARGO", args).size()));
        }
        if (name == "MAYUSC" || name == "UPPER") return Value::str(upper(onlyText("MAYUSC", args)));
        if (name == "MINUSC" || name == "LOWER") return Value::str(lower(onlyText("MINUSC", args)));
        throw std::runtime_error("Funcion desconocida: " + name);
    }

    // limites para que la recursion no reviente la pila del programa
    static constexpr int maxDependencies = 1000;  // celdas con formula encadenadas
    static constexpr int maxNesting = 1000;       // parentesis y funciones abiertos a la vez

    // estado de una celda durante un calculo (como los colores de un DFS)
    enum class State { Unvisited, Active, Done };

    struct CellInfo {
        State state = State::Unvisited;
        Value result;
        bool emptyCell = false;  // true si la celda no existe en la hoja
    };

    // hace un calculo: recuerda que celdas ya calculo y cuales estan en curso
    class Evaluator {
    public:
        explicit Evaluator(const Sheet& sheet) : sheet_(sheet) {}

        // valor de una celda; si es formula la evaluamos (DFS sobre las referencias)
        Value cellValue(const string& cell, bool* emptyCell = nullptr) {
            const State state = visited_[cell].state;
            if (state == State::Active) throw std::runtime_error("Referencia circular: " + cell);
            if (state == State::Unvisited) compute(cell);
            // volvemos a buscar porque la tabla pudo cambiar durante la recursion
            const CellInfo& info = visited_[cell];
            if (emptyCell) *emptyCell = info.emptyCell;
            return info.result;
        }

        // cuenta los parentesis y funciones abiertos (ver maxNesting)
        void openNesting() {
            if (++nesting_ > maxNesting) throw std::runtime_error("Formula demasiado anidada");
        }

        void closeNesting() {
            --nesting_;
        }

    private:
        const Sheet& sheet_;
        HashTable<HashTableTraits<string, CellInfo>> visited_;
        int dependencies_ = 0;
        int nesting_ = 0;

        // calcula una celda que todavia no se visito
        void compute(const string& cell) {
            const string* raw = sheet_.findRaw(cell);
            if (!raw || raw->empty() || raw->front() != '=') {
                // celda sin formula; si no existe vale 0
                CellInfo& info = visited_[cell];
                info.result = raw ? literalValue(*raw) : Value::num(0);
                info.emptyCell = (raw == nullptr);
                info.state = State::Done;
                return;
            }
            if (++dependencies_ > maxDependencies) throw std::runtime_error("Demasiadas dependencias");
            visited_[cell].state = State::Active;
            const string formula = raw->substr(1);
            Value result = Parser(formula, *this).parse();
            CellInfo& info = visited_[cell];
            info.result = std::move(result);
            info.state = State::Done;
            --dependencies_;
        }
    };

    // Parser recursivo descendente, una funcion por regla:
    //   expr maneja + - &, term maneja * /, unary los signos de adelante y
    //   primary los numeros, textos, celdas, funciones y parentesis.
    // Asi se respeta la prioridad de operadores. Va evaluando mientras lee.
    class Parser {
    public:
        Parser(const string& text, Evaluator& evaluator) : s_(text), eval_(evaluator) {}

        // evalua toda la formula y revisa que no sobre nada
        Value parse() {
            Value result = expr();
            spaces();
            if (p_ != s_.size()) throw std::runtime_error("Caracteres sobrantes en formula");
            return result;
        }

    private:
        const string& s_;
        Evaluator& eval_;
        size_t p_ = 0;

        // expr: term (+ term | - term | & term)*
        Value expr() {
            Value left = term();
            while (const char op = nextOperator("+-&")) {
                if (op == '&') {
                    const Value right = term();
                    left = Value::str(left.display() + right.display());
                } else {
                    // el lado izquierdo se convierte primero, asi su error sale antes
                    const double a = left.asNumber();
                    const double b = term().asNumber();
                    left = Value::num(op == '+' ? a + b : a - b);
                }
            }
            return left;
        }

        // term: unary (* unary | / unary)*
        Value term() {
            Value left = unary();
            while (const char op = nextOperator("*/")) {
                if (op == '*') {
                    const double a = left.asNumber();
                    const double b = unary().asNumber();
                    left = Value::num(a * b);
                } else {
                    const double divisor = unary().asNumber();
                    if (divisor == 0) throw std::runtime_error("Division por cero");
                    left = Value::num(left.asNumber() / divisor);
                }
            }
            return left;
        }

        // unary: signos + y - de adelante; se aplican del mas cercano al valor hacia afuera
        Value unary() {
            string signs;
            while (true) {
                if (take('+')) {
                    signs += '+';
                } else if (take('-')) {
                    signs += '-';
                } else {
                    break;
                }
            }
            Value value = primary();
            for (size_t i = signs.size(); i > 0; --i) {
                const double n = value.asNumber();
                value = Value::num(signs[i - 1] == '-' ? -n : n);
            }
            return value;
        }

        // primary: (expr), texto, numero, funcion o celda
        Value primary() {
            if (take('(')) {
                eval_.openNesting();
                Value inside = expr();
                expect(')');
                eval_.closeNesting();
                return inside;
            }
            if (std::optional<Value> atom = literal()) return std::move(*atom);
            if (p_ < s_.size() && std::isalpha(static_cast<unsigned char>(s_[p_]))) {
                const string name = ident();
                if (take('(')) return function(name);
                if (!validCell(name)) {
                    throw std::runtime_error("Referencia o identificador desconocido: " + name);
                }
                return eval_.cellValue(name);
            }
            throw std::runtime_error("Expresion invalida en posicion " + std::to_string(p_));
        }

        // lee los argumentos de una funcion (el '(' ya se leyo) y la ejecuta
        Value function(const string& name) {
            eval_.openNesting();
            Arguments args;
            if (!take(')')) {
                do {
                    argument(args);
                } while (take(',') || take(';'));
                expect(')');
            }
            eval_.closeNesting();
            return callFunction(name, args);
        }

        // un argumento puede ser un rango (A1:B3) o cualquier expresion
        void argument(Arguments& args) {
            spaces();
            const size_t start = p_;
            const string first = ident();
            if (validCell(first) && take(':')) {
                range(first, args);
            } else {
                p_ = start;
                args.push_back({expr(), false});
            }
        }

        // agrega todas las celdas del rango, fila por fila
        void range(const string& first, Arguments& args) {
            const string last = ident();
            if (!validCell(last)) throw std::runtime_error("Rango invalido");
            const auto [c1, r1] = coords(first);
            const auto [c2, r2] = coords(last);
            if (c1 > c2 || r1 > r2 || (c2 - c1 + 1LL) * (r2 - r1 + 1LL) > 100000) {
                throw std::runtime_error("Rango invalido o demasiado grande");
            }
            for (int row = r1; row <= r2; ++row) {
                for (int col = c1; col <= c2; ++col) {
                    bool emptyCell = false;
                    Value value = eval_.cellValue(cellName(col, row), &emptyCell);
                    args.push_back({std::move(value), emptyCell});
                }
            }
        }

        // lee un texto entre comillas o un numero; si no hay ninguno devuelve nullopt
        std::optional<Value> literal() {
            if (take('"')) return quotedText();
            spaces();
            if (p_ < s_.size() && (std::isdigit(static_cast<unsigned char>(s_[p_])) || s_[p_] == '.')) {
                size_t used = 0;
                const double x = std::stod(s_.substr(p_), &used);
                p_ += used;
                return Value::num(x);
            }
            return std::nullopt;
        }

        // lee un texto hasta la comilla de cierre; "" adentro es una comilla
        Value quotedText() {
            string text;
            while (p_ < s_.size()) {
                const char c = s_[p_++];
                if (c != '"') {
                    text += c;
                } else if (p_ < s_.size() && s_[p_] == '"') {
                    text += '"';
                    ++p_;
                } else {
                    return Value::str(text);
                }
            }
            throw std::runtime_error("Cadena sin cerrar");
        }

        // si sigue uno de los operadores lo consume y lo devuelve; si no devuelve 0
        char nextOperator(const string& ops) {
            spaces();
            if (p_ < s_.size() && ops.find(s_[p_]) != string::npos) return s_[p_++];
            return 0;
        }

        void spaces() {
            while (p_ < s_.size() && std::isspace(static_cast<unsigned char>(s_[p_]))) ++p_;
        }

        // si el siguiente caracter es c lo consume y devuelve true
        bool take(char c) {
            spaces();
            if (p_ < s_.size() && s_[p_] == c) {
                ++p_;
                return true;
            }
            return false;
        }

        // igual que take pero si no esta lanza error
        void expect(char c) {
            if (!take(c)) {
                throw std::runtime_error(string("Se esperaba '") + c + "' en posicion " +
                                         std::to_string(p_));
            }
        }

        // lee un nombre (de celda o de funcion) y lo pasa a mayusculas
        string ident() {
            spaces();
            const size_t start = p_;
            while (p_ < s_.size() && (std::isalnum(static_cast<unsigned char>(s_[p_])) || s_[p_] == '_')) {
                ++p_;
            }
            return upper(s_.substr(start, p_ - start));
        }
    };
};
} // namespace pc4texto
#endif
