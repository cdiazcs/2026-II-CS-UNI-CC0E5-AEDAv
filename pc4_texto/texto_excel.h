#ifndef PC4_TEXTO_EXCEL_H
#define PC4_TEXTO_EXCEL_H
#include "../containers/hash_table.h"
#include "../containers/vector.h"
#include <algorithm>
#include <cctype>
#include <cmath>
#include <deque>
#include <fstream>
#include <iomanip>
#include <memory>
#include <optional>
#include <sstream>
#include <stdexcept>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace pc4texto {
using std::string;
struct Value {
    string text;
    bool numeric = false;
    double number = 0;
    static Value num(double n) { if(!std::isfinite(n)) throw std::runtime_error("Resultado no finito"); return {"", true, n}; }
    static Value str(string s) { return {std::move(s), false, 0}; }
    string display() const {
        if (!numeric) return text;
        std::ostringstream out; out << std::setprecision(14) << number; return out.str();
    }
    double asNumber() const {
        if(numeric) return number;
        size_t used=0;
        try { double n=std::stod(text,&used); if(used==text.size() && std::isfinite(n)) return n; } catch(...) {}
        throw std::runtime_error("Se esperaba un numero: " + text);
    }
};

namespace detail {
using FormulaArguments = Vector<VectorAscTraits<Value>>;

enum class EvaluationState { NoVisitada, Activa, Completada };

struct CellEvaluation {
    EvaluationState state = EvaluationState::NoVisitada;
    std::optional<Value> result;
    bool absent = false;
};

struct RangeCursor {
    int firstColumn;
    int lastColumn;
    int lastRow;
    int column;
    int row;
};

struct FunctionCall {
    string name;
    FormulaArguments arguments;
    std::optional<RangeCursor> range;
};

enum class Rule { Expr, Term, Unary, Primary };
enum class Step { Enter, FirstValue, Operator, RightValue, Close, Reference,
                  CallArgument, CallValue, CallSeparator, CallFinish, RangeNext, RangeValue };
struct ExpressionContinuation {
    Rule rule;
    Step step = Step::Enter;
    char op = '\0';
    Value left;
    double leftNumber = 0;
    std::unique_ptr<FunctionCall> call;

    explicit ExpressionContinuation(Rule rule) : rule(rule) {}
};


struct FormulaFrame {
    explicit FormulaFrame(string address, string formula = {})
        : address(std::move(address)), formula(std::move(formula)) {}
    FormulaFrame(const FormulaFrame&) = delete;
    FormulaFrame& operator=(const FormulaFrame&) = delete;

    string address;
    string formula;
    size_t position = 0;
    std::vector<ExpressionContinuation> expression;
    Value expressionValue;
    bool expressionCellAbsent = false;
    std::optional<Value> result;
};

// Contexto local a un cálculo. Los marcos no se poseen entre sí.
class EvaluationContext {
    std::deque<CellEvaluation> cells_;
    HashTable<HashTableTraits<string, CellEvaluation*>> index_;

public:
    EvaluationContext() = default;
    EvaluationContext(const EvaluationContext&) = delete;
    EvaluationContext& operator=(const EvaluationContext&) = delete;

    CellEvaluation& cell(const string& address) {
        auto& entry = index_[address];
        if (!entry) entry = &cells_.emplace_back();
        return *entry;
    }

    std::vector<std::unique_ptr<FormulaFrame>> frames;
};
} // namespace detail

class Sheet {
    friend struct SheetParserTestAccess;
    using FormulaArguments = detail::FormulaArguments;
    static constexpr Ref absentRangeCell = 1;
    using CellTable = HashTable<HashTableTraits<string, string>>;

    struct CellState {
        CellTable cells;
        size_t count = 0;

        void set(const string& cell, const string& raw) {
            const bool isNew = !cells.contains(cell);
            cells.push_back(CellTable::Pair(cell, raw));
            if (isNew) ++count;
        }
    };

    std::unique_ptr<CellState> state_ = std::make_unique<CellState>();

    const string* findRaw(const string& cell) const {
        // La API de consulta de HashTable requiere una tabla mutable.
        auto& cells = state_->cells;
        if (!cells.contains(cell)) return nullptr;
        return &cells[cell];
    }

    static string upper(string s) { for(char& c:s) c=static_cast<char>(std::toupper(static_cast<unsigned char>(c))); return s; }
    static bool validCell(const string& c) {
        size_t i=0; while(i<c.size() && c[i]>='A' && c[i]<='Z') ++i;
        if(i==0 || i==c.size() || c[i]=='0') return false;
        for(;i<c.size();++i) if(!std::isdigit(static_cast<unsigned char>(c[i]))) return false;
        return true;
    }
    static std::pair<int,int> coords(const string& c) {
        int col=0; size_t i=0;
        while(i<c.size() && std::isalpha(static_cast<unsigned char>(c[i]))) { col=col*26+c[i++]-'A'+1; if(col>16384) throw std::runtime_error("Columna fuera de rango"); }
        long row=std::stol(c.substr(i)); if(row<1 || row>1048576) throw std::runtime_error("Fila fuera de rango");
        return {col,static_cast<int>(row)};
    }
    static string addr(int col,int row) { string s; while(col) { --col; s.insert(s.begin(),char('A'+col%26)); col/=26; } return s+std::to_string(row); }
    static Value function(string name,const FormulaArguments& a) {
        name=upper(name);
        if (name == "CONTAR" || name == "COUNT") {
            size_t count = 0;
            for (const auto& node : a) {
                if (node.getRef() != absentRangeCell && node.getValue().numeric) {
                    ++count;
                }
            }
            return Value::num(static_cast<double>(count));
        }
        const bool average = name == "PROMEDIO" || name == "AVERAGE";
        if(name=="SUMA" || name=="SUM" || average || name=="MIN" || name=="MAX") {
            double r = 0;
            size_t included = 0;
            for (const auto& node : a) {
                if (average && node.getRef() == absentRangeCell) continue;
                const double x = node.getValue().asNumber();
                if (included == 0) r = x;
                else if (name == "MIN") r = std::min(r, x);
                else if (name == "MAX") r = std::max(r, x);
                else r += x;
                ++included;
            }
            if (included == 0 && name != "SUMA" && name != "SUM")
                throw std::runtime_error("Faltan argumentos");
            if (average) r /= included;
            return Value::num(r);
        }
        if(name=="CONCAT" || name=="CONCATENAR") { string s; for(const auto& node:a) s+=node.getValue().display(); return Value::str(s); }
        if(name=="LARGO" || name=="LEN") { if(a.size()!=1) throw std::runtime_error("LARGO requiere un argumento"); return Value::num(static_cast<double>(a[0].getValue().display().size())); }
        if(name=="MAYUSC" || name=="UPPER") { if(a.size()!=1) throw std::runtime_error("MAYUSC requiere un argumento"); return Value::str(upper(a[0].getValue().display())); }
        if(name=="MINUSC" || name=="LOWER") { if(a.size()!=1) throw std::runtime_error("MINUSC requiere un argumento"); string s=a[0].getValue().display(); for(char& c:s)c=static_cast<char>(std::tolower(static_cast<unsigned char>(c))); return Value::str(s); }
        throw std::runtime_error("Funcion desconocida: "+name);
    }
    class Parser {
        const string& s_; size_t p_=0;
        void spaces() { while(p_<s_.size() && std::isspace(static_cast<unsigned char>(s_[p_]))) ++p_; }
        bool take(char c) { spaces(); if(p_<s_.size() && s_[p_]==c) {++p_;return true;}return false; }
        void expect(char c) { if(!take(c)) throw std::runtime_error(string("Se esperaba '")+c+"' en posicion "+std::to_string(p_)); }
        string ident() { spaces(); size_t start=p_; while(p_<s_.size() && (std::isalnum(static_cast<unsigned char>(s_[p_])) || s_[p_]=='_')) ++p_; return upper(s_.substr(start,p_-start)); }
        detail::RangeCursor readRange(const string& first) {
            const string last = ident();
            if (!validCell(last)) throw std::runtime_error("Rango invalido");
            const auto [c1, r1] = coords(first);
            const auto [c2, r2] = coords(last);
            if (c1 > c2 || r1 > r2 || (c2-c1+1LL)*(r2-r1+1LL) > 100000)
                throw std::runtime_error("Rango invalido o demasiado grande");
            return {c1, c2, r2, c1, r1};
        }
        std::optional<Value> literal() {
            if(take('"')) { string v; bool closed=false; while(p_<s_.size()) {char c=s_[p_++]; if(c=='"') { if(p_<s_.size() && s_[p_]=='"') {v+='"';++p_;}else{closed=true;break;} } else v+=c;} if(!closed)throw std::runtime_error("Cadena sin cerrar");return Value::str(v); }
            spaces();
            if(p_<s_.size() && (std::isdigit(static_cast<unsigned char>(s_[p_])) || s_[p_]=='.')) {
                size_t used=0; double x=std::stod(s_.substr(p_),&used);p_+=used;return Value::num(x);
            }
            return std::nullopt;
        }
    public:
        explicit Parser(const string& s) : s_(s) {}

        Value parseWithContinuations() {
            detail::FormulaFrame frame("");
            if (advanceExpression(frame))
                throw std::runtime_error("Referencia requiere un contexto de evaluacion");
            return std::move(*frame.result);
        }

        std::optional<string> advanceExpression(detail::FormulaFrame& formula) {
            using detail::Rule;
            using detail::Step;
            auto& pending = formula.expression;
            if (pending.empty()) pending.emplace_back(Rule::Expr);
            p_ = formula.position;
            auto& value = formula.expressionValue;
            while (!pending.empty()) {
                auto& frame = pending.back();
                switch (frame.step) {
                    case Step::Enter:
                        if (frame.rule == Rule::Primary) {
                            if (take('(')) {
                                frame.step = Step::Close;
                                pending.emplace_back(Rule::Expr);
                            } else {
                                auto atom = literal();
                                if (!atom && p_ < s_.size() &&
                                    std::isalpha(static_cast<unsigned char>(s_[p_]))) {
                                    string address = ident();
                                    if (take('(')) {
                                        frame.call = std::make_unique<detail::FunctionCall>();
                                        frame.call->name = std::move(address);
                                        frame.step = take(')') ? Step::CallFinish : Step::CallArgument;
                                        break;
                                    }
                                    if (!validCell(address))
                                        throw std::runtime_error("Referencia o identificador desconocido: " + address);
                                    frame.step = Step::Reference;
                                    formula.position = p_;
                                    return address;
                                }
                                if (!atom)
                                    throw std::runtime_error("Expresion invalida en posicion " +
                                                             std::to_string(p_));
                                value = std::move(*atom);
                                pending.pop_back();
                            }
                        } else if (frame.rule == Rule::Unary) {
                            if (take('+')) frame.op = '+';
                            else if (take('-')) frame.op = '-';
                            const auto child = frame.op ? Rule::Unary : Rule::Primary;
                            frame.step = Step::FirstValue;
                            pending.emplace_back(child);
                        } else {
                            const auto child = frame.rule == Rule::Expr ? Rule::Term : Rule::Unary;
                            frame.step = Step::FirstValue;
                            pending.emplace_back(child);
                        }
                        break;

                    case Step::FirstValue:
                        if (frame.rule == Rule::Unary) {
                            if (frame.op == '+') value = Value::num(value.asNumber());
                            if (frame.op == '-') value = Value::num(-value.asNumber());
                            pending.pop_back();
                        } else {
                            frame.left = std::move(value);
                            frame.step = Step::Operator;
                        }
                        break;

                    case Step::Operator: {
                        const std::string_view operators = frame.rule == Rule::Expr ? "+-&" : "*/";
                        spaces();
                        if (p_ == s_.size() || operators.find(s_[p_]) == std::string_view::npos) {
                            value = std::move(frame.left);
                            pending.pop_back();
                            break;
                        }
                        frame.op = s_[p_++];
                        // GCC evaluó primero la coerción izquierda de +, - y * en el
                        // parser anterior. C++ no fija ese orden entre los operandos.
                        if (frame.op != '/' && frame.op != '&')
                            frame.leftNumber = frame.left.asNumber();
                        const auto child = frame.rule == Rule::Expr ? Rule::Term : Rule::Unary;
                        frame.step = Step::RightValue;
                        pending.emplace_back(child);
                        break;
                    }

                    case Step::RightValue:
                        switch (frame.op) {
                            case '+': frame.left = Value::num(frame.leftNumber + value.asNumber()); break;
                            case '-': frame.left = Value::num(frame.leftNumber - value.asNumber()); break;
                            case '*': frame.left = Value::num(frame.leftNumber * value.asNumber()); break;
                            case '/': {
                                const double divisor = value.asNumber();
                                if (divisor == 0) throw std::runtime_error("Division por cero");
                                frame.left = Value::num(frame.left.asNumber() / divisor);
                                break;
                            }
                            case '&': frame.left = Value::str(frame.left.display() + value.display()); break;
                        }
                        frame.step = Step::Operator;
                        break;

                    case Step::Close:
                        expect(')');
                        pending.pop_back();
                        break;

                    case Step::Reference:
                        pending.pop_back();
                        break;

                    case Step::CallArgument: {
                        spaces();
                        const size_t start = p_;
                        const string first = ident();
                        if (validCell(first) && take(':')) {
                            frame.call->range = readRange(first);
                            frame.step = Step::RangeNext;
                            break;
                        }
                        p_ = start;
                        frame.step = Step::CallValue;
                        pending.emplace_back(Rule::Expr);
                        break;
                    }

                    case Step::CallValue:
                        frame.call->arguments.push_back(value);
                        frame.step = Step::CallSeparator;
                        break;

                    case Step::RangeNext: {
                        const auto& range = *frame.call->range;
                        frame.step = Step::RangeValue;
                        formula.position = p_;
                        return addr(range.column, range.row);
                    }

                    case Step::RangeValue: {
                        frame.call->arguments.push_back(value, formula.expressionCellAbsent ? absentRangeCell : 0);
                        auto& range = *frame.call->range;
                        if (range.column < range.lastColumn) {
                            ++range.column;
                            frame.step = Step::RangeNext;
                        } else if (range.row < range.lastRow) {
                            ++range.row;
                            range.column = range.firstColumn;
                            frame.step = Step::RangeNext;
                        } else {
                            frame.call->range.reset();
                            frame.step = Step::CallSeparator;
                        }
                        break;
                    }

                    case Step::CallSeparator:
                        if (take(',') || take(';')) {
                            frame.step = Step::CallArgument;
                        } else {
                            expect(')');
                            frame.step = Step::CallFinish;
                        }
                        break;

                    case Step::CallFinish:
                        value = function(frame.call->name, frame.call->arguments);
                        pending.pop_back();
                        break;
                }
            }
            spaces();
            if (p_ != s_.size()) throw std::runtime_error("Caracteres sobrantes en formula");
            formula.result = std::move(value);
            return std::nullopt;
        }
    };
    // Solo clasifica celdas literales. asNumber y los números en fórmulas aún admiten hexadecimales.
    static bool isDecimalLiteral(const string& raw) {
        size_t pos = 0;
        while (pos < raw.size() && std::isspace(static_cast<unsigned char>(raw[pos]))) ++pos;
        const auto skipSign = [&] {
            if (pos < raw.size() && (raw[pos] == '+' || raw[pos] == '-')) ++pos;
        };
        const auto consumeDigits = [&] {
            const size_t start = pos;
            while (pos < raw.size() && raw[pos] >= '0' && raw[pos] <= '9') ++pos;
            return pos != start;
        };

        skipSign();
        const bool integerDigits = consumeDigits();
        bool fractionDigits = false;
        if (pos < raw.size() && raw[pos] == '.') {
            ++pos;
            fractionDigits = consumeDigits();
        }
        if (!integerDigits && !fractionDigits) return false;
        if (pos < raw.size() && (raw[pos] == 'e' || raw[pos] == 'E')) {
            ++pos;
            skipSign();
            if (!consumeDigits()) return false;
        }
        return pos == raw.size();
    }

    static Value literalValue(const string& raw) {
        if (isDecimalLiteral(raw)) {
            try {
                size_t used = 0;
                const double number = std::stod(raw, &used);
                if (used == raw.size() && std::isfinite(number)) return Value::num(number);
            } catch (const std::logic_error&) {
                // Una conversión inválida o fuera de rango se conserva como texto.
            }
        }
        return Value::str(raw);
    }

    Value evaluateWithContinuations(string root, detail::EvaluationContext& context) const {
        using detail::EvaluationState;
        root = upper(std::move(root));
        if (!validCell(root)) throw std::runtime_error("Celda invalida");

        const auto request = [&](const string& address) -> const detail::CellEvaluation* {
            auto& record = context.cell(address);
            if (record.state == EvaluationState::Completada) return &record;
            if (record.state == EvaluationState::Activa)
                throw std::runtime_error("Referencia circular: " + address);
            const string* raw = findRaw(address);
            if (raw && !raw->empty() && raw->front() == '=') {
                context.frames.push_back(
                    std::make_unique<detail::FormulaFrame>(address, raw->substr(1)));
                record.state = EvaluationState::Activa;
                return nullptr;
            }
            record.result = raw ? literalValue(*raw) : Value::num(0);
            record.absent = !raw;
            record.state = EvaluationState::Completada;
            return &record;
        };

        if (const auto* record = request(root)) return *record->result;
        while (!context.frames.empty()) {
            auto& frame = *context.frames.back();
            Parser parser(frame.formula);
            if (const auto dependency = parser.advanceExpression(frame)) {
                if (const auto* record = request(*dependency)) {
                    frame.expressionValue = *record->result;
                    frame.expressionCellAbsent = record->absent;
                }
                continue;
            }

            auto& record = context.cell(frame.address);
            record.result = std::move(frame.result);
            record.state = EvaluationState::Completada;
            context.frames.pop_back();
            if (context.frames.empty()) return *record.result;
            context.frames.back()->expressionValue = *record.result;
            context.frames.back()->expressionCellAbsent = false;
        }
        throw std::logic_error("Evaluacion sin resultado");
    }

public:
    Sheet() = default;

    Sheet(const Sheet& other) : Sheet() {
        for (const auto& node : other.state_->cells) {
            state_->cells.push_back(node.getValue());
        }
        state_->count = other.state_->count;
    }

    Sheet(Sheet&& other) : Sheet() {
        state_.swap(other.state_);
    }

    Sheet& operator=(const Sheet& other) {
        if (this != &other) {
            Sheet copy(other);
            state_.swap(copy.state_);
        }
        return *this;
    }

    Sheet& operator=(Sheet&& other) {
        if (this != &other) {
            Sheet moved(std::move(other));
            state_.swap(moved.state_);
        }
        return *this;
    }

    void set(string cell, string raw) {
        cell = upper(cell);
        if (!validCell(cell)) throw std::runtime_error("Celda invalida: " + cell);
        coords(cell);
        state_->set(cell, raw);
    }

    string get(string cell) const {
        const string* raw = findRaw(upper(cell));
        return raw ? *raw : "";
    }

    Value calculate(string cell) const {
        detail::EvaluationContext context;
        return evaluateWithContinuations(std::move(cell), context);
    }
    size_t size() const {return state_->count;}
    void save(const string& path) const {
        std::ofstream out(path,std::ios::binary);if(!out)throw std::runtime_error("No se pudo guardar: "+path);
        out<<"PC4TEXTO1\n";
        for (const auto& node : state_->cells) {
            const auto entry = node.getValue();
            out << std::quoted(entry.getKey()) << ' ' << std::quoted(entry.getValue()) << '\n';
        }
        if(!out)throw std::runtime_error("Error durante escritura");
    }
    void load(const string& path) {
        std::ifstream in(path,std::ios::binary);if(!in)throw std::runtime_error("No se pudo abrir: "+path);
        string magic;std::getline(in,magic);if(magic!="PC4TEXTO1")throw std::runtime_error("Formato de archivo invalido");
        auto loaded = std::make_unique<CellState>();
        string line;
        while(std::getline(in,line)) {
            if(line.empty()) continue;
            std::istringstream is(line);
            string cell,raw;
            if(!(is>>std::quoted(cell)>>std::quoted(raw)))throw std::runtime_error("Fila corrupta");
            is>>std::ws;if(!is.eof())throw std::runtime_error("Fila con datos adicionales");
            cell=upper(cell);if(!validCell(cell))throw std::runtime_error("Celda corrupta");coords(cell);
            loaded->set(cell, raw);
        }
        if(in.bad() || !in.eof())throw std::runtime_error("Error de lectura");
        state_.swap(loaded);
    }
};
} // namespace pc4texto
#endif
