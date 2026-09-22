#pragma once
// environment.h — Scope and Variable Resolution for the Ume Interpreter

#include <string>
#include <unordered_map>
#include <memory>
#include <optional>
#include <stdexcept>
#include <mutex>

namespace Ume {

// Forward declare Value (defined in evaluator.h)
struct Value;

class Environment : public std::enable_shared_from_this<Environment> {
public:
    // Create a new top-level environment
    Environment();
    // Create a child scope
    explicit Environment(std::shared_ptr<Environment> parent);

    // Declare a new variable in the current scope
    void declare(const std::string& name, Value value);

    // Assign to an existing variable (walks up the scope chain)
    void assign(const std::string& name, Value value);

    // Lookup a variable (walks up the scope chain)
    Value& get(const std::string& name);
    const Value& get(const std::string& name) const;

    // Check existence without throwing
    bool has(const std::string& name) const;

    // Single-pass lookup returning pointer (nullptr if not found)
    Value* lookup(const std::string& name);
    const Value* lookup(const std::string& name) const;

    // Create a child scope
    std::shared_ptr<Environment> child();

    std::shared_ptr<Environment> parent() const { return parent_; }

private:
    std::unordered_map<std::string, Value> vars_;
    std::shared_ptr<Environment>           parent_;
    mutable std::recursive_mutex           mtx_;
};

}
