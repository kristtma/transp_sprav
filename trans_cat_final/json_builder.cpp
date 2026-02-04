#include "json_builder.h"

namespace json {

using namespace std::literals;

Node Builder::Build() {
    if (nodes_stack_.empty() && !root_.IsNull()) {
        return root_;
    }
    throw std::logic_error("JSON is not complete");
}

Builder::DictItemContext Builder::StartDict() {
    if (nodes_stack_.empty() && root_.IsNull()) {
        root_ = Node(Dict{});
        nodes_stack_.push_back(&root_);
        return DictItemContext(*this);
    }

    if (!nodes_stack_.empty() && (nodes_stack_.back()->IsArray() ||
                                  (nodes_stack_.back()->IsMap() && key_expected_))) {
        Node new_dict(Dict{});
        if (nodes_stack_.back()->IsArray()) {
            Array& arr = const_cast<Array&>(nodes_stack_.back()->AsArray());
            arr.push_back(new_dict);
            nodes_stack_.push_back(&arr.back());
        } else {
            Dict& dict = const_cast<Dict&>(nodes_stack_.back()->AsMap());
            dict[last_key_] = new_dict;
            nodes_stack_.push_back(&dict[last_key_]);
            last_key_.clear();
            key_expected_ = false;
        }
        return DictItemContext(*this);
    }

    throw std::logic_error("StartDict in invalid context");
}

Builder::ArrayItemContext Builder::StartArray() {
    if (nodes_stack_.empty() && root_.IsNull()) {
        root_ = Node(Array{});
        nodes_stack_.push_back(&root_);
        return ArrayItemContext(*this);
    }

    if (!nodes_stack_.empty() && (nodes_stack_.back()->IsArray() ||
                                  (nodes_stack_.back()->IsMap() && key_expected_))) {
        Node new_array(Array{});
        if (nodes_stack_.back()->IsArray()) {
            Array& arr = const_cast<Array&>(nodes_stack_.back()->AsArray());
            arr.push_back(new_array);
            nodes_stack_.push_back(&arr.back());
        } else {
            Dict& dict = const_cast<Dict&>(nodes_stack_.back()->AsMap());
            dict[last_key_] = new_array;
            nodes_stack_.push_back(&dict[last_key_]);
            last_key_.clear();
            key_expected_ = false;
        }
        return ArrayItemContext(*this);
    }

    throw std::logic_error("StartArray in invalid context");
}

Builder& Builder::Value(Node value) {
    if (nodes_stack_.empty()) {
        if (root_.IsNull()) {
            root_ = std::move(value);
            return *this;
        }
        throw std::logic_error("Multiple root values");
    }

    if (nodes_stack_.back()->IsArray()) {
        Array& arr = const_cast<Array&>(nodes_stack_.back()->AsArray());
        arr.push_back(std::move(value));
    } else if (nodes_stack_.back()->IsMap()) {
        if (!key_expected_) {
            throw std::logic_error("Value without key in dictionary");
        }
        Dict& dict = const_cast<Dict&>(nodes_stack_.back()->AsMap());
        dict[last_key_] = std::move(value);
        last_key_.clear();
        key_expected_ = false;
    } else {
        throw std::logic_error("Value in invalid context");
    }
    return *this;
}

Builder::DictValueContext Builder::Key(std::string key) {
    if (nodes_stack_.empty() || !nodes_stack_.back()->IsMap()) {
        throw std::logic_error("Key outside of dictionary");
    }
    if (key_expected_) {
        throw std::logic_error("Key already set");
    }
    last_key_ = std::move(key);
    key_expected_ = true;
    return DictValueContext(*this);
}

Builder& Builder::EndDict() {
    if (nodes_stack_.empty() || !nodes_stack_.back()->IsMap()) {
        throw std::logic_error("EndDict without StartDict");
    }
    if (key_expected_) {
        throw std::logic_error("Unfinished key-value pair");
    }
    nodes_stack_.pop_back();
    return *this;
}

Builder& Builder::EndArray() {
    if (nodes_stack_.empty() || !nodes_stack_.back()->IsArray()) {
        throw std::logic_error("EndArray without StartArray");
    }
    nodes_stack_.pop_back();
    return *this;
}

Builder::DictValueContext Builder::DictItemContext::Key(std::string key) {
    return builder_.Key(std::move(key));
}

Builder& Builder::DictItemContext::EndDict() {
    return builder_.EndDict();
}

Builder::DictItemContext Builder::DictValueContext::Value(Node value) {
    builder_.Value(std::move(value));
    return DictItemContext(builder_);
}

Builder::DictItemContext Builder::BaseContext::StartDict() {
    return builder_.StartDict();
}

Builder::ArrayItemContext Builder::BaseContext::StartArray() {
    return builder_.StartArray();
}

Builder::ArrayItemContext Builder::ArrayItemContext::Value(Node value) {
    builder_.Value(std::move(value));
    return ArrayItemContext(builder_);
}



Builder& Builder::BaseContext::EndArray() {
    return builder_.EndArray();
}
} // namespace json