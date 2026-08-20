#pragma once
// compiler.h — Compilation Pipeline Orchestrator for Ume

#include "ast.h"
#include "llvm_codegen.h"
#include <string>
#include <vector>

namespace Ume {

struct CompilerOptions {
    std::string inputFile;
    std::string outputFile;       // default: derived from input
    std::string target  = "windows"; // windows | linux | macos
    std::string backend = "cpp";     // "cpp" | "llvm"
    bool        optimize = false;
    bool        debugInfo = true;
    bool        runAfterCompile = false;
    std::vector<std::string> includePaths;
    std::vector<std::string> libPaths;
    std::vector<std::string> libs;
};

struct CompilerResult {
    bool        success = false;
    std::string errorMessage;
    std::string outputFile;
    int         exitCode = 0;
};

class Preprocessor {
public:
    explicit Preprocessor(std::vector<std::string> includePaths = {});

    // Returns the fully preprocessed source string
    std::string process(const std::string& source, const std::string& filename);

private:
    std::vector<std::string>  includePaths_;
    std::vector<std::string>  includeStack_; // for cycle detection

    std::string resolveInclude(const std::string& path,
                               const std::string& currentDir);
    std::string readFile(const std::string& path);
    std::string processSource(const std::string& source,
                              const std::string& filename);
    bool        alreadyIncluded(const std::string& path) const;
    std::vector<std::string> included_; // processed include guard list
};

class Compiler {
public:
    explicit Compiler(CompilerOptions opts);

    CompilerResult compile();

private:
    CompilerOptions opts_;

    // Reads source, preprocesses, lexes, parses, analyses, generates
    std::string buildCppSource(const std::string& umeSource,
                               const std::string& filename);

    // Invokes the host C++ compiler (cl / g++ / clang++) on generated code
    int invokeCppCompiler(const std::string& cppFile,
                          const std::string& outFile,
                          std::string& compilerOutput);

    std::string derivedOutputName() const;
    std::string findCppCompiler()   const;
    std::string findClang()         const;

    // LLVM pipeline: lex/parse/semantic/llvm_codegen → .ll → clang
    CompilerResult compileLLVM(const std::string& source);
    int            invokeLLVMCompiler(const std::string& llFile,
                                     const std::string& outFile);
};

}
