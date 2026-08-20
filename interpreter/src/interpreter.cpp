// interpreter.cpp — Ume Interpreter Entry Point
#include "../include/interpreter.h"
#include "../../compiler/include/lexer.h"
#include "../../compiler/include/parser.h"
#include "../../compiler/include/package_resolver.h"
#include "../../compiler/include/compiler.h"
#include "../../compiler/include/diagnostics.h"
#include "../../compiler/include/semantic.h"
#include "../include/evaluator.h"
#include <fstream>
#include <sstream>
#include <iostream>
#include <filesystem>
#include <algorithm>

namespace Ume {

// Minimal TOML reader used by the CLI and interpreter for project entry discovery
static std::string readTomlValueLocal(const std::string& toml, const std::string& key) {
    std::istringstream stream(toml);
    std::string line;
    while (std::getline(stream, line)) {
        size_t start = 0;
        while (start < line.size() && (line[start] == ' ' || line[start] == '\t')) start++;
        if (start >= line.size() || line[start] == '#') continue;

        size_t eqPos = line.find('=', start);
        if (eqPos == std::string::npos) continue;

        std::string k = line.substr(start, eqPos - start);
        while (!k.empty() && (k.back() == ' ' || k.back() == '\t')) k.pop_back();

        if (k == key) {
            std::string v = line.substr(eqPos + 1);
            size_t vStart = 0;
            while (vStart < v.size() && (v[vStart] == ' ' || v[vStart] == '\t')) vStart++;
            v = v.substr(vStart);

            if (!v.empty() && (v[0] == '"' || v[0] == '\'')) {
                char quote = v[0];
                size_t qEnd = v.find(quote, 1);
                if (qEnd != std::string::npos) {
                    return v.substr(1, qEnd - 1);
                }
            }

            size_t commentPos = v.find('#');
            if (commentPos != std::string::npos) v = v.substr(0, commentPos);
            while (!v.empty() && (v.back() == ' ' || v.back() == '\t' || v.back() == '\r' || v.back() == '\n')) {
                v.pop_back();
            }
            return v;
        }
    }
    return "";
}

Interpreter::Interpreter(InterpreterOptions opts) : opts_(std::move(opts)) {}

// Load and parse a file into an existing Evaluator (library load, no main())
static void loadLibraryFile(const std::string& path, Evaluator& evaluator,
                             const std::string& stdlibPath) {
    std::ifstream f(path);
    if (!f.good()) return;
    std::ostringstream buf; buf << f.rdbuf();
    
    std::vector<std::string> searchPaths;
    if (!stdlibPath.empty()) searchPaths.push_back(stdlibPath);
    searchPaths.push_back(".");

    Preprocessor prep(searchPaths);
    std::string src = prep.process(buf.str(), path);

    Lexer lex(src, path);
    Parser par(lex.tokenize(), path);
    auto prog = par.parse();

    // NOTE: skip resolveImports for prelude — it strips declarations
    // PackageResolver resolver(searchPaths);
    // resolver.resolveImports(*prog, path);

    evaluator.loadLibrary(*prog);
}

InterpreterResult Interpreter::runSource(const std::string& source,
                                          const std::string& filename) {
    InterpreterResult result;
    std::string processed;
    try {
        std::vector<std::string> searchPaths;
        // Add file's own directory first so local packages win
        if (!filename.empty()) {
            std::error_code ec;
            auto parentDir = std::filesystem::path(filename).parent_path();
            if (!parentDir.empty() && std::filesystem::exists(parentDir, ec))
                searchPaths.push_back(parentDir.string());
        }
        if (!opts_.stdlibPath.empty()) searchPaths.push_back(opts_.stdlibPath);
        // Only add CWD as fallback if no file-relative path was added
        if (searchPaths.empty()) searchPaths.push_back(".");

        Preprocessor prep(searchPaths);
        processed = prep.process(source, filename);

        Lexer lexer(processed, filename);
        std::vector<Token> tokens = lexer.tokenize();

        Parser parser(std::move(tokens), filename);
        auto program = parser.parse();

        // Resolve package imports for user code
        PackageResolver resolver(searchPaths);
        resolver.resolveImports(*program, filename);

        Evaluator evaluator;
        evaluator.setArgs(opts_.args);

        // Auto-load prelude if stdlibPath is set
        if (!opts_.stdlibPath.empty()) {
            auto preludePath = (std::filesystem::path(opts_.stdlibPath) / "prelude.ume").string();
            loadLibraryFile(preludePath, evaluator, opts_.stdlibPath);
        }

        int exitCode = evaluator.run(*program);
        result.success  = true;
        result.exitCode = exitCode;
    } catch (const ParseError& e) {
        result.success = false;
        Diagnostic diag;
        diag.level = DiagnosticLevel::Error;
        diag.message = e.what();
        diag.location.filename = e.filename.empty() ? filename : e.filename;
        diag.location.line = e.line;
        diag.location.column = e.column;
        if (!e.foundToken.empty() && e.foundToken.rfind("Unexpected character:", 0) == 0) {
            diag.label = e.foundToken;
        } else if (!e.expectedToken.empty() && !e.foundToken.empty()) {
            diag.label = "expected '" + e.expectedToken + "' here, found '" + e.foundToken + "'";
        } else if (!e.foundToken.empty()) {
            diag.label = "unexpected token '" + e.foundToken + "'";
        }
        diag.help = e.hint;
        result.errorMessage = DiagnosticEngine::format(diag, source);
    } catch (const SemanticError& e) {
        result.success = false;
        Diagnostic diag;
        diag.level = DiagnosticLevel::Error;
        diag.message = e.what();
        diag.location.filename = e.filename.empty() ? filename : e.filename;
        diag.location.line = e.line;
        diag.location.column = e.column;
        result.errorMessage = DiagnosticEngine::format(diag, source);
    } catch (const UmeRuntimeException& e) {
        result.success = false;
        Diagnostic diag;
        diag.level = DiagnosticLevel::Error;
        diag.message = "Runtime error: " + e.value.toString();
        diag.location.filename = e.filename.empty() ? filename : e.filename;
        diag.location.line = e.line;
        diag.location.column = e.column;
        if (!e.callStack.empty()) {
            diag.help = "Call stack:\n" + e.callStack;
        }
        result.errorMessage = DiagnosticEngine::format(diag, source);
    } catch (const std::exception& e) {
        result.success = false;
        result.errorMessage = e.what();
    }
    return result;
}

// ─────────────────────────────────────────────────────────────
// Interpreter::runSourceCombined
// Like runSource but skips PackageResolver since all project
// files are already inlined into one combined source string.
// ─────────────────────────────────────────────────────────────
InterpreterResult Interpreter::runSourceCombined(const std::string& source,
                                                  const std::string& filename) {
    InterpreterResult result;
    std::string processed;
    try {
        std::vector<std::string> searchPaths;
        if (!opts_.stdlibPath.empty()) searchPaths.push_back(opts_.stdlibPath);

        Preprocessor prep(searchPaths);
        processed = prep.process(source, filename);

        Lexer lexer(processed, filename);
        std::vector<Token> tokens = lexer.tokenize();

        Parser parser(std::move(tokens), filename);
        auto program = parser.parse();

        Evaluator evaluator;
        evaluator.setArgs(opts_.args);

        if (!opts_.stdlibPath.empty()) {
            auto preludePath = (std::filesystem::path(opts_.stdlibPath) / "prelude.ume").string();
            loadLibraryFile(preludePath, evaluator, opts_.stdlibPath);
        }

        int exitCode = evaluator.run(*program);
        result.success  = true;
        result.exitCode = exitCode;
    } catch (const ParseError& e) {
        result.success = false;
        Diagnostic diag;
        diag.level = DiagnosticLevel::Error;
        diag.message = e.what();
        diag.location.filename = e.filename.empty() ? filename : e.filename;
        diag.location.line = e.line;
        diag.location.column = e.column;
        if (!e.foundToken.empty() && e.foundToken.rfind("Unexpected character:", 0) == 0)
            diag.label = e.foundToken;
        else if (!e.expectedToken.empty() && !e.foundToken.empty())
            diag.label = "expected '" + e.expectedToken + "' here, found '" + e.foundToken + "'";
        else if (!e.foundToken.empty())
            diag.label = "unexpected token '" + e.foundToken + "'";
        diag.help = e.hint;
        result.errorMessage = DiagnosticEngine::format(diag, source);
    } catch (const SemanticError& e) {
        result.success = false;
        Diagnostic diag;
        diag.level = DiagnosticLevel::Error;
        diag.message = e.what();
        diag.location.filename = e.filename.empty() ? filename : e.filename;
        diag.location.line = e.line;
        diag.location.column = e.column;
        result.errorMessage = DiagnosticEngine::format(diag, source);
    } catch (const UmeRuntimeException& e) {
        result.success = false;
        Diagnostic diag;
        diag.level = DiagnosticLevel::Error;
        diag.message = "Runtime error: " + e.value.toString();
        diag.location.filename = e.filename.empty() ? filename : e.filename;
        diag.location.line = e.line;
        diag.location.column = e.column;
        if (!e.callStack.empty()) diag.help = "Call stack:\n" + e.callStack;
        result.errorMessage = DiagnosticEngine::format(diag, source);
    } catch (const std::exception& e) {
        result.success = false;
        result.errorMessage = e.what();
    }
    return result;
}

InterpreterResult Interpreter::run() {
    InterpreterResult result;

    if (opts_.filename.empty()) {
        result.success = false;
        result.errorMessage = "No input file specified";
        return result;
    }
    // If a directory was provided, try to resolve it to the project's entry file & combine source files
    try {
        std::filesystem::path p(opts_.filename);
        if (std::filesystem::exists(p) && std::filesystem::is_directory(p)) {
            std::filesystem::path projectDir = p;
            std::filesystem::path tomlPath = projectDir / "ume.toml";
            std::string entryRel = "src/main.ume";

            if (std::filesystem::exists(tomlPath)) {
                std::ifstream tf(tomlPath.string());
                std::ostringstream tbuf; tbuf << tf.rdbuf();
                std::string entryVal = readTomlValueLocal(tbuf.str(), "entry");
                if (!entryVal.empty()) {
                    entryRel = entryVal;
                }
            }

            std::filesystem::path entryAbsPath = projectDir / entryRel;
            
            if (std::filesystem::exists(entryAbsPath)) {
                opts_.filename = entryAbsPath.string();
            } else {
                opts_.filename = (projectDir / "src" / "main.ume").string();
            }
        }
    } catch (...) {}

    std::ifstream file(opts_.filename);
    if (!file.good()) {
        result.success = false;
        result.errorMessage = "Cannot open file: " + opts_.filename;
        return result;
    }

    std::ostringstream buf;
    buf << file.rdbuf();
    return runSource(buf.str(), opts_.filename);
}

InterpreterResult Interpreter::runREPL() {
    std::cout << "Ume Language 1.0.0 \xe2\x80\x94 Interactive Mode\n";
    std::cout << "Enter code. Empty line runs it. Type 'exit' to quit.\n\n" << std::flush;

    while (true) {
        // Collect input until an empty line
        std::string input;
        bool hasInput = false;
        while (true) {
            std::cout << (hasInput ? "... " : ">>> ") << std::flush;
            std::string line;
            if (!std::getline(std::cin, line)) goto done;
            if (!hasInput && (line == "exit" || line == "quit")) goto done;
            if (line.empty()) { if (hasInput) break; else continue; }
            input += line + "\n";
            hasInput = true;
        }

        {
            // Wrap bare statements/expressions in an implicit function
            std::string wrapped = "public func void __repl__() {\n" + input + "}\n";
            InterpreterResult r = runSource(wrapped, "<repl>");
            if (!r.success)
                std::cerr << "Error: " << r.errorMessage << '\n' << std::flush;
        }
    }
    done:
    std::cout << "\nBye!\n" << std::flush;
    return {true, 0, ""};
}

}
