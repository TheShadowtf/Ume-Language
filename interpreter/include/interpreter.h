#pragma once
// interpreter.h — Interpreter Entry Point for `ume -run`

#include "evaluator.h"
#include <string>
#include <vector>

namespace Ume {

struct InterpreterOptions {
    std::string              filename;
    std::vector<std::string> args;          // program arguments
    std::vector<std::string> includePaths;
    std::string              stdlibPath;    // path to stdlib/ directory
    bool                     strictMode = false;
};

struct InterpreterResult {
    bool        success   = false;
    int         exitCode  = 0;
    std::string errorMessage;
};

// ─────────────────────────────────────────────────────────────
// Interpreter
//   Full pipeline: preprocess → lex → parse → evaluate
// ─────────────────────────────────────────────────────────────
class Interpreter {
public:
    explicit Interpreter(InterpreterOptions opts);

    InterpreterResult run();

    // Run an interactive REPL session
    InterpreterResult runREPL();

    // Run from a source string (for REPL / testing)
    InterpreterResult runSource(const std::string& source,
                                const std::string& filename = "<repl>");

    InterpreterResult runSourceCombined(const std::string& source,
                                        const std::string& filename = "<combined>");

private:
    InterpreterOptions opts_;
};

} // namespace Ume
