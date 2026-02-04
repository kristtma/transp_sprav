#pragma once
#include <iostream>
#include <map>
#include <string>
#include <vector>
#include <variant>
#include <stdexcept>

namespace json {

class Node;
using Array = std::vector<Node>;
using Dict = std::map<std::string, Node>;

class ParsingError : public std::runtime_error {
public:
    using runtime_error::runtime_error;
};

class Node {
public:
    using Value = std::variant<std::nullptr_t, int, double, bool, std::string, Array, Dict>;
    Node() : value_(nullptr) {}
    Node(Array array) : value_(std::move(array)) {}
    Node(Dict map) : value_(std::move(map)) {}
    Node(int value) : value_(value) {}  // Made non-explicit
    Node(double value) : value_(value) {}  // Made non-explicit
    Node(bool value) : value_(value) {}  // Made non-explicit
    Node(std::string value) : value_(std::move(value)) {}  // Made non-explicit
    Node(const char* value) : value_(std::string(value)) {}  // Added for string literals
    Node(std::nullptr_t) : value_(nullptr) {}  // Added for nullptr
    

    bool IsNull() const { return std::holds_alternative<std::nullptr_t>(value_); }
    bool IsInt() const { return std::holds_alternative<int>(value_); }
    bool IsDouble() const { return std::holds_alternative<double>(value_) || std::holds_alternative<int>(value_); }
    bool IsPureDouble() const { return std::holds_alternative<double>(value_); }
    bool IsBool() const { return std::holds_alternative<bool>(value_); }
    bool IsString() const { return std::holds_alternative<std::string>(value_); }
    bool IsArray() const { return std::holds_alternative<Array>(value_); }
    bool IsMap() const { return std::holds_alternative<Dict>(value_); }

    int AsInt() const;
    double AsDouble() const;
    bool AsBool() const;
    const std::string& AsString() const;
    const Array& AsArray() const;
    const Dict& AsMap() const;

    bool operator==(const Node& other) const;
    bool operator!=(const Node& other) const;

    const Value& GetValue() const { return value_; }

private:
    Value value_;
};

bool operator==(const Node& lhs, const Array& rhs);
bool operator==(const Array& lhs, const Node& rhs);
bool operator!=(const Node& lhs, const Array& rhs);
bool operator!=(const Array& lhs, const Node& rhs);
class Document {
public:
    explicit Document(Node root) : root_(std::move(root)) {}
    const Node& GetRoot() const { return root_; }

private:
    Node root_;
};

Document Load(std::istream& input);
void Print(const Document& doc, std::ostream& output);

inline bool operator==(const Document& lhs, const Document& rhs) {
    return lhs.GetRoot() == rhs.GetRoot();
}

} // namespace json
