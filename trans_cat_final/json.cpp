#include "json.h"
#include <cctype>
#include <sstream>
#include <cassert>
#include <iostream>
#include <string>

namespace json {
bool operator==(const Node& lhs, const Array& rhs) {
    if (!lhs.IsArray()) {
        return false;
    }
    try {
        return lhs.AsArray() == rhs;
    } catch (const std::exception&) {
        return false;
    }
}

bool operator==(const Array& lhs, const Node& rhs) {
    return rhs == lhs;
}

bool operator!=(const Node& lhs, const Array& rhs) {
    return !(lhs == rhs);
}

bool operator!=(const Array& lhs, const Node& rhs) {
    return !(lhs == rhs);
}
namespace {

Node LoadNode(std::istream& input);

using Number = std::variant<int, double>;

Number LoadNumber(std::istream& input) {
    using namespace std::literals;
    std::string parsed_num;
    auto read_char = [&parsed_num, &input] {
        parsed_num += static_cast<char>(input.get());
        if (!input) {
            throw ParsingError("Failed to read number from stream"s);
        }
    };

    auto read_digits = [&input, read_char] {
        if (!std::isdigit(input.peek())) {
            throw ParsingError("A digit is expected"s);
        }
        while (std::isdigit(input.peek())) {
            read_char();
        }
    };

    if (input.peek() == '-') {
        read_char();
    }

    if (input.peek() == '0') {
        read_char();
    } else {
        read_digits();
    }

    bool is_int = true;
    if (input.peek() == '.') {
        read_char();
        read_digits();
        is_int = false;
    }

    if (int ch = input.peek(); ch == 'e' || ch == 'E') {
        read_char();
        if (ch = input.peek(); ch == '+' || ch == '-') {
            read_char();
        }
        read_digits();
        is_int = false;
    }

    try {
        if (is_int) {
            try {
                return std::stoi(parsed_num);
            } catch (...) {
                // Fall through to double parsing
            }
        }
        return std::stod(parsed_num);
    } catch (...) {
        throw ParsingError("Failed to convert "s + parsed_num + " to number"s);
    }
}

Node LoadArray(std::istream& input) {
    std::vector<Node> result;

    for (char c; input >> c && c != ']';) {
        if (c != ',') {
            input.putback(c);
        }
        result.push_back(LoadNode(input));
    }
    if (!input) {
        throw ParsingError("Array parsing error");
    }
    return Node(std::move(result));
}

Node LoadNumberNode(std::istream& input) {
    auto number = LoadNumber(input);
    if (std::holds_alternative<int>(number)) {
        return Node(std::get<int>(number));
    } else {
        return Node(std::get<double>(number));
    }
}
std::string LString(std::istream& input) {
    using namespace std::literals;
    auto it = std::istreambuf_iterator<char>(input);
    auto end = std::istreambuf_iterator<char>();
    std::string s;
    while (true) {
        if (it == end) {
            throw ParsingError("String parsing error");
        }
        const char ch = *it;
        if (ch == '"') {
            ++it;
            break;
        } else if (ch == '\\') {
            ++it;
            if (it == end) {
                throw ParsingError("String parsing error");
            }
            const char escaped_char = *(it);
            switch (escaped_char) {
            case 'n':
                s.push_back('\n');
                break;
            case 't':
                s.push_back('\t');
                break;
            case 'r':
                s.push_back('\r');
                break;
            case '"':
                s.push_back('"');
                break;
            case '\\':
                s.push_back('\\');
                break;
            default:
                throw ParsingError("Unrecognized escape sequence \\"s + escaped_char);
            }
        } else if (ch == '\n' || ch == '\r') {
            throw ParsingError("Unexpected end of line"s);
        } else {
            s.push_back(ch);
        }
        ++it;
    }
    return s;
}
Node LoadString(std::istream& input) {
    return Node(LString(input));
}
Node LoadDict(std::istream& input) {
    Dict result;

    for (char c; input >> c && c != '}';) {
        if (c == ',') {
            input >> c;
        }

        std::string key = LoadString(input).AsString();
        input >> c;
        result.insert({move(key), LoadNode(input)});
    }
    if (!input) {
        throw ParsingError("Map parsing error");
    }
    return Node(move(result));
}

Node LoadBool(std::istream& input) {
    std::string boolStr;
    while (std::isalpha(input.peek())) {
        boolStr += input.get();
    }
    if (boolStr == "true") {
        return Node(true);
    } else if (boolStr == "false") {
        return Node(false);
    } else {
        throw ParsingError("Invalid boolean value");
    }
}

Node LoadNull(std::istream& input) {
    std::string nullStr;
    while (std::isalpha(input.peek())) {
        nullStr += input.get();
    }
    if (nullStr == "null") {
        return Node(nullptr);
    } else {
        throw ParsingError("Invalid null value");
    }
}


Node LoadNode(std::istream& input) {
    input >> std::ws;
    char c;
    input >> c;

    if (!input) {
        throw ParsingError("Unexpected end of input");
    }

    if (c == '[') {
        return LoadArray(input);
    } else if (c == '{') {
        return LoadDict(input);
    } else if (c == '"') {
        return LoadString(input);
    } else if (c == 't' || c == 'f') {
        input.putback(c);
        return LoadBool(input);
    } else if (c == 'n') {
        input.putback(c);
        return LoadNull(input);
    } else if (c == '-' || std::isdigit(c)) {
        input.putback(c);
        return LoadNumberNode(input);
    } else {
        throw ParsingError(std::string("Unexpected character: ") + c);
    }
}

} // namespace

int Node::AsInt() const {
    if (std::holds_alternative<int>(value_)) {
        return std::get<int>(value_);
    }
    throw std::logic_error("Node does not contain an int");
}

double Node::AsDouble() const {
    if (std::holds_alternative<double>(value_)) {
        return std::get<double>(value_);
    } else if (std::holds_alternative<int>(value_)) {
        return static_cast<double>(std::get<int>(value_));
    }
    throw std::logic_error("Node does not contain a double");
}

bool Node::AsBool() const {
    if (std::holds_alternative<bool>(value_)) {
        return std::get<bool>(value_);
    }
    throw std::logic_error("Node does not contain a bool");
}

const std::string& Node::AsString() const {
    if (std::holds_alternative<std::string>(value_)) {
        return std::get<std::string>(value_);
    }
    throw std::logic_error("Node does not contain a string");
}

const Array& Node::AsArray() const {
    if (std::holds_alternative<Array>(value_)) {
        return std::get<Array>(value_);
    }
    throw std::logic_error("Node does not contain an array");
}

const Dict& Node::AsMap() const {
    if (std::holds_alternative<Dict>(value_)) {
        return std::get<Dict>(value_);
    }
    throw std::logic_error("Node does not contain a map");
}

bool Node::operator==(const Node& other) const {
    if (value_.index() != other.value_.index()) {
        return false;
    }

    return std::visit([&other](const auto& value) {
        using T = std::decay_t<decltype(value)>;
        if constexpr (std::is_same_v<T, int> && std::is_same_v<T, double>) {
            // Special handling for int/double comparison
            return value == other.AsDouble();
        } else {
            return value == std::get<T>(other.value_);
        }
    }, value_);
}
bool Node::operator!=(const Node& other) const {
    return !(*this == other);
}

Document Load(std::istream& input) {
    return Document{LoadNode(input)};
}

template <typename Value>
void PrintValue(const Value& value, std::ostream& out) {
    out << value;
}

void PrintValue(std::nullptr_t, std::ostream& out) {
    out << "null";
}

void PrintValue(bool value, std::ostream& out) {
    out << (value ? "true" : "false");
}

void PrintValue(const std::string& value, std::ostream& out) {
    out << '"';
    for (char c : value) {
        switch (c) {
        case '"':  out << "\\\""; break;
        case '\\': out << "\\\\"; break;
        case '\n': out << "\\n";  break;
        case '\r': out << "\\r";  break;
        case '\t': out << "\\t";  break;
        default:   out << c;
        }
    }
    out << '"';
}

void PrintNode(const Node& node, std::ostream& out) {
    std::visit([&out](const auto& value) { PrintValue(value, out); }, node.GetValue());
}

void PrintValue(const Array& array, std::ostream& out) {
    out << '[';
    bool first = true;
    for (const auto& item : array) {
        if (!first) {
            out << ',';
        }
        first = false;
        PrintNode(item, out);
    }
    out << ']';
}

void PrintValue(const Dict& dict, std::ostream& out) {
    out << '{';
    bool first = true;
    for (const auto& [key, value] : dict) {
        if (!first) {
            out << ',';
        }
        first = false;
        PrintValue(key, out);
        out << ':';
        PrintNode(value, out);
    }
    out << '}';
}

void Print(const Document& doc, std::ostream& output) {
    PrintNode(doc.GetRoot(), output);
}

} // namespace json
