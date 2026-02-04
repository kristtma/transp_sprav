// json_builder.h

#pragma once

#include "json.h"

#include <stack>
#include <stdexcept>
#include <string>
#include <vector>

namespace json {

class Builder {
public:
    class DictItemContext;
    class ArrayItemContext;
    class DictValueContext;
    class BaseContext {
    public:
        BaseContext(Builder& builder) : builder_(builder) {}
        DictItemContext StartDict();
        ArrayItemContext StartArray();
        Node Build();

        Builder& Value(Node value);
        DictValueContext Key(std::string key);
        Builder& EndDict();
        Builder& EndArray();
    protected:
        Builder& builder_;
    };

    class DictItemContext : public BaseContext {
    public:
        using BaseContext::BaseContext;
        DictValueContext Key(std::string key);
        Builder& EndDict();

        DictItemContext StartDict() = delete;
        ArrayItemContext StartArray() = delete;
        Node Build() = delete;
        Builder& Value(Node value) = delete;
        Builder& EndArray() = delete;
    };

    class DictValueContext : public BaseContext {
    public:
        using BaseContext::BaseContext;
        DictItemContext Value(Node value);
        //
        Builder& EndArray() = delete;
        Builder& EndDict() = delete;
        Node Build() = delete;
        DictValueContext Key(std::string key) = delete;
    };

    class ArrayItemContext : public BaseContext {
    public:
        using BaseContext::BaseContext;
        ArrayItemContext Value(Node value);
        //
        Builder& EndDict() = delete;
        DictValueContext Key(std::string key) = delete;
        Node Build() = delete;
    };

    Builder() = default;

    DictItemContext StartDict();
    ArrayItemContext StartArray();
    Node Build();

    // Methods for internal use
    Builder& Value(Node value);
    DictValueContext Key(std::string key);
    Builder& EndDict();
    Builder& EndArray();

private:
    Node root_;
    std::vector<Node*> nodes_stack_;
    std::string last_key_;
    bool key_expected_ = false;
};

} // namespace json