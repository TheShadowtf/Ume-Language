// environment.cpp — Scope and Variable Resolution for the Ume Interpreter
#include "../include/environment.h"
#include "../include/evaluator.h"  // for Value definition
#include <stdexcept>

namespace Ume {

Environment::Environment() = default;

Environment::Environment(std::shared_ptr<Environment> parent)
    : parent_(std::move(parent)) {}

void Environment::declare(const std::string& name, Value value) {
    std::lock_guard<std::recursive_mutex> lock(mtx_);
    vars_[name] = std::move(value);
}

void Environment::assign(const std::string& name, Value value) {
    std::lock_guard<std::recursive_mutex> lock(mtx_);
    auto it = vars_.find(name);
    if (it != vars_.end()) {
        it->second = std::move(value);
        return;
    }
    if (parent_) {
        parent_->assign(name, std::move(value));
        return;
    }
    throw std::runtime_error("Undefined variable: " + name);
}

Value& Environment::get(const std::string& name) {
    std::lock_guard<std::recursive_mutex> lock(mtx_);
    auto it = vars_.find(name);
    if (it != vars_.end()) return it->second;
    if (parent_) return parent_->get(name);
    throw std::runtime_error("Undefined variable: " + name);
}

const Value& Environment::get(const std::string& name) const {
    std::lock_guard<std::recursive_mutex> lock(mtx_);
    auto it = vars_.find(name);
    if (it != vars_.end()) return it->second;
    if (parent_) return parent_->get(name);
    throw std::runtime_error("Undefined variable: " + name);
}

bool Environment::has(const std::string& name) const {
    std::lock_guard<std::recursive_mutex> lock(mtx_);
    if (vars_.count(name)) return true;
    if (parent_) return parent_->has(name);
    return false;
}

std::shared_ptr<Environment> Environment::child() {
    return std::make_shared<Environment>(shared_from_this());
}

} // namespace Ume
