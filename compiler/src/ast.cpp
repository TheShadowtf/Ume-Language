// ast.cpp — AST utility implementations
#include "../include/ast.h"
#include <sstream>

namespace Ume {

std::string TypeAnnotation::toString() const {
    std::ostringstream oss;
    oss << name;
    if (!typeArgs.empty()) {
        oss << '<';
        for (size_t i = 0; i < typeArgs.size(); ++i) {
            if (i > 0) oss << ", ";
            oss << typeArgs[i].toString();
        }
        oss << '>';
    }
    if (isFunc && !funcParams.empty()) {
        oss << '<';
        for (size_t i = 0; i < funcParams.size(); ++i) {
            if (i > 0) oss << ", ";
            oss << funcParams[i].toString();
        }
        oss << '>';
    }
    if (nullable) oss << '?';
    if (isArray) {
        oss << '[';
        if (arraySize > 0) oss << arraySize;
        oss << ']';
    }
    return oss.str();
}

}
