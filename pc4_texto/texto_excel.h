#ifndef PC4_TEXTO_EXCEL_H
#define PC4_TEXTO_EXCEL_H

// Evita warnings ajenos a este módulo.
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

// Valor numérico o texto.
struct Value {
    string text;
    bool numeric = false;
    double number = 0;

    // Crea un número finito.
    static Value num(double n) {
        if (!std::isfinite(n)) throw std::runtime_error("Resultado no finito");
        return {"", true, n};
    }

    // Crea texto.
    static Value str(string s) {
        return {std::move(s), false, 0};
    }

    // Convierte el valor a texto visible.
    string display() const {
        if (!numeric) return text;
        std::ostringstream out;
        out << std::setprecision(14) << number;
        return out.str();
    }

    // Convierte el valor a número.
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

// Hoja de cálculo.
class Sheet {
public:
    Sheet() = default;

    // Copia las celdas.
    Sheet(const Sheet& other) {
        for (const auto& node : other.state_->cells) {
            const auto entry = node.getValue();
            state_->set(entry.getKey(), entry.getValue());
        }
    }

    // Intercambia el estado interno.
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

    // Guarda una celda.
    void set(string cell, const string& raw) {
        cell = upper(cell);
        if (!validCell(cell)) throw std::runtime_error("Celda invalida: " + cell);
        coords(cell);  // Comprueba los límites
        state_->set(cell, raw);
    }

    // Devuelve el contenido original.
    string get(const string& cell) const {
        const string* raw = findRaw(upper(cell));
        return raw ? *raw : "";
    }

    // Calcula una celda.
    Value calculate(const string& cell) const {
        const string name = upper(cell);
        if (!validCell(name)) throw std::runtime_error("Celda invalida");
        Evaluator evaluator(*this);
        return evaluator.cellValue(name);
    }

    // Cantidad de celdas guardadas.
    size_t size() const {
        return state_->count;
    }

    // Guarda la hoja.
    void save(const string& path) const {
        std::ofstream out(path, std::ios::binary);
        if (!out) throw std::runtime_error("No se pudo guardar: " + path);
        // Los textos se escriben con comillas.
        out << "PC4TEXTO2\n" << state_->cells << '\n';
        if (!out) throw std::runtime_error("Error durante escritura");
    }

    // Carga la hoja.
    void load(const string& path) {
        std::ifstream in(path, std::ios::binary);
        if (!in) throw std::runtime_error("No se pudo abrir: " + path);
        string header;
        std::getline(in, header);
        if (header != "PC4TEXTO2") throw std::runtime_error("Formato de archivo invalido");

        // Lee en una tabla temporal.
        in >> std::ws;
        if (in.peek() != '[') throw std::runtime_error("Archivo corrupto");
        auto loaded = std::make_unique<CellState>();
        in >> loaded->cells;
        if (!in) throw std::runtime_error("Archivo corrupto");
        in >> std::ws;
        if (!in.eof()) throw std::runtime_error("Datos sobrantes al final del archivo");

        // Valida las celdas cargadas.
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

    // Estado de las celdas.
    struct CellState {
        // Necesario para usar métodos de la tabla desde métodos const.
        mutable CellTable cells;
        size_t count = 0;

        void set(const string& cell, const string& raw) {
            if (!cells.contains(cell)) ++count;
            cells[cell] = raw;
        }
    };

    // Estado reemplazable con carga o movimiento.
    std::unique_ptr<CellState> state_ = std::make_unique<CellState>();

    // Busca una celda; devuelve nullptr si no existe.
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

    // Valida el nombre de una celda.
    static bool validCell(const string& name) {
        static const std::regex pattern("[A-Z]+[1-9][0-9]*");
        return std::regex_match(name, pattern);
    }

    // Convierte un nombre a columna y fila.
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

    // Convierte columna y fila a nombre.
    static string cellName(int col, int row) {
        string letters;
        while (col > 0) {
            --col;
            letters.insert(letters.begin(), static_cast<char>('A' + col % 26));
            col /= 26;
        }
        return letters + std::to_string(row);
    }

    // Convierte números literales; los demás valores son texto.
    static Value literalValue(const string& raw) {
        static const std::regex decimal(R"(\s*[+-]?([0-9]+\.?[0-9]*|\.[0-9]+)([eE][+-]?[0-9]+)?)");
        if (std::regex_match(raw, decimal)) {
            try {
                size_t used = 0;
                const double n = std::stod(raw, &used);
                if (used == raw.size() && std::isfinite(n)) return Value::num(n);
            } catch (const std::logic_error&) {
                // Conserva el texto si no es número válido.
            }
        }
        return Value::str(raw);
    }

    // Argumento de una función.
    struct Argument {
        Value value;
        bool emptyCell = false;
    };
    using Arguments = std::vector<Argument>;

    // Funciones agregadas.
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
        // Promedio sin valores es error; otros resultados quedan en cero.
        if (used == 0 && average) throw std::runtime_error("Division por cero");
        if (average) result /= used;
        return Value::num(result);
    }

    // Cuenta números.
    static Value count(const Arguments& args) {
        size_t total = 0;
        for (const Argument& arg : args) {
            if (!arg.emptyCell && arg.value.numeric) ++total;
        }
        return Value::num(static_cast<double>(total));
    }

    // Verifica un único argumento de texto.
    static string onlyText(const string& name, const Arguments& args) {
        if (args.size() != 1) throw std::runtime_error(name + " requiere un argumento");
        return args[0].value.display();
    }

    // Ejecuta una función.
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

    // Límites de profundidad.
    static constexpr int maxDependencies = 1000;
    static constexpr int maxNesting = 1000;

    // Estado durante el cálculo.
    enum class State { Unvisited, Active, Done };

    // Información calculada de una celda.
    struct CellInfo {
        State state = State::Unvisited;
        Value result;
        bool emptyCell = false;  // true si la celda no existe en la hoja
    };

    // Evalúa fórmulas y referencias.
    class Evaluator {
    public:
        explicit Evaluator(const Sheet& sheet) : sheet_(sheet) {}

        // Obtiene el valor de una celda.
        Value cellValue(const string& cell, bool* emptyCell = nullptr) {
            const State state = visited_[cell].state;
            if (state == State::Active) throw std::runtime_error("Referencia circular: " + cell);
            if (state == State::Unvisited) compute(cell);
            // Busca otra vez tras la recursión.
            const CellInfo& info = visited_[cell];
            if (emptyCell) *emptyCell = info.emptyCell;
            return info.result;
        }

        // Controla el anidamiento.
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

        // Calcula una celda nueva.
        void compute(const string& cell) {
            const string* raw = sheet_.findRaw(cell);
            if (!raw || raw->empty() || raw->front() != '=') {
                // Celda vacía o sin fórmula vale cero.
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

    // Analiza fórmulas con prioridad de operadores.
    class Parser {
    public:
        Parser(const string& text, Evaluator& evaluator) : s_(text), eval_(evaluator) {}

        // Evalúa toda la fórmula.
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

        // Suma, resta y concatenación.
        Value expr() {
            Value left = term();
            while (const char op = nextOperator("+-&")) {
                if (op == '&') {
                    const Value right = term();
                    left = Value::str(left.display() + right.display());
                } else {
                    // Convierte primero el lado izquierdo.
                    const double a = left.asNumber();
                    const double b = term().asNumber();
                    left = Value::num(op == '+' ? a + b : a - b);
                }
            }
            return left;
        }

        // Multiplicación y división.
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

        // Aplica signos iniciales.
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

        // Lee valores y expresiones básicas.
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

        // Lee y ejecuta una función.
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

        // Lee una expresión o un rango.
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

        // Agrega las celdas de un rango.
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

        // Lee texto o número.
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

        // Lee una cadena entre comillas.
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

        // Consume un operador permitido.
        char nextOperator(const string& ops) {
            spaces();
            if (p_ < s_.size() && ops.find(s_[p_]) != string::npos) return s_[p_++];
            return 0;
        }

        void spaces() {
            while (p_ < s_.size() && std::isspace(static_cast<unsigned char>(s_[p_]))) ++p_;
        }

        // Consume un carácter si coincide.
        bool take(char c) {
            spaces();
            if (p_ < s_.size() && s_[p_] == c) {
                ++p_;
                return true;
            }
            return false;
        }

        // Exige un carácter.
        void expect(char c) {
            if (!take(c)) {
                throw std::runtime_error(string("Se esperaba '") + c + "' en posicion " +
                                         std::to_string(p_));
            }
        }

        // Lee un identificador y lo convierte a mayúsculas.
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
