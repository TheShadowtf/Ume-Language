// compiler.cpp — Compilation Pipeline Orchestrator for Ume
#include "../include/compiler.h"
#include "../include/lexer.h"
#include "../include/parser.h"
#include "../include/package_resolver.h"
#include "../include/semantic.h"
#include "../include/codegen.h"
#include "../include/llvm_codegen.h"

#include <fstream>
#include <sstream>
#include <iostream>
#include <filesystem>
#include <stdexcept>
#include <cstdlib>
#include <algorithm>
#include <cctype>
#include <tuple>
#include <cstdio>

namespace fs = std::filesystem;

namespace Ume {

// ─────────────────────────────────────────────────────────────
// Preprocessor
// ─────────────────────────────────────────────────────────────
Preprocessor::Preprocessor(std::vector<std::string> includePaths)
    : includePaths_(std::move(includePaths)) {}

std::string Preprocessor::readFile(const std::string& path) {
    std::ifstream f(path);
    if (!f.is_open())
        throw std::runtime_error("Cannot open file: " + path);
    std::ostringstream ss;
    ss << f.rdbuf();
    return ss.str();
}

bool Preprocessor::alreadyIncluded(const std::string& path) const {
    return std::find(included_.begin(), included_.end(), path) != included_.end();
}

std::string Preprocessor::resolveInclude(const std::string& path,
                                          const std::string& currentDir) {
    // Try relative to current file first
    fs::path rel = fs::path(currentDir) / path;
    if (fs::exists(rel)) return rel.string();
    // Try include paths
    for (auto& ip : includePaths_) {
        fs::path abs = fs::path(ip) / path;
        if (fs::exists(abs)) return abs.string();
    }
    return path; // Return as-is; may be a stdlib stub
}

static std::string escapeCppString(const std::string& s) {
    std::string out;
    out.reserve(s.size());
    for (char c : s) {
        switch (c) {
        case '"': out += "\\\""; break;
        case '\\': out += "\\\\"; break;
        case '\n': out += "\\n"; break;
        case '\r': out += "\\r"; break;
        case '\t': out += "\\t"; break;
        default: out += c;
        }
    }
    return out;
}

std::string Preprocessor::processSource(const std::string& source,
                                         const std::string& filename) {
    // Tokenise for include directives only (line by line approach for safety)
    std::istringstream in(source);
    std::ostringstream out;
    std::string line;
    int lineNum = 0;
    std::string currentDir = fs::path(filename).parent_path().string();

    while (std::getline(in, line)) {
        lineNum++;
        // Trim leading whitespace
        size_t start = 0;
        while (start < line.size() && (line[start]==' ' || line[start]=='\t')) start++;
        if (start < line.size() && line[start] == '#') {
            std::string directive = line.substr(start + 1);
            // Remove leading whitespace
            size_t ds = 0;
            while (ds < directive.size() && directive[ds] == ' ') ds++;
            directive = directive.substr(ds);
            if (directive.substr(0, 7) == "include") {
                std::string rest = directive.substr(7);
                // Extract path between "" or <>
                size_t q1 = rest.find_first_of("\"<");
                size_t q2 = rest.find_last_of("\">");
                if (q1 != std::string::npos && q2 != std::string::npos && q2 > q1) {
                    std::string incPath = rest.substr(q1 + 1, q2 - q1 - 1);
                    // Resolve
                    std::string resolved = resolveInclude(incPath, currentDir);
                    if (!alreadyIncluded(resolved)) {
                        if (fs::exists(resolved)) {
                            included_.push_back(resolved);
                            std::string incSrc = readFile(resolved);
                            out << "#line 1 \"" << escapeCppString(resolved) << "\"\n";
                            out << processSource(incSrc, resolved);
                            out << "#line " << (lineNum + 1) << " \"" << escapeCppString(filename) << "\"\n";
                        } else {
                            // stdlib stub — emit comment
                            out << "// #include \"" << incPath << "\" (stdlib)\n";
                            out << "#line " << (lineNum + 1) << " \"" << escapeCppString(filename) << "\"\n";
                        }
                    } else {
                        out << "#line " << (lineNum + 1) << " \"" << escapeCppString(filename) << "\"\n";
                    }
                }
                continue;
            }
        }
        out << line << '\n';
    }
    return out.str();
}

std::string Preprocessor::process(const std::string& source, const std::string& filename) {
    included_.clear();
    included_.push_back(filename); // self-guard
    std::ostringstream out;
    out << "#line 1 \"" << escapeCppString(filename) << "\"\n";
    out << processSource(source, filename);
    return out.str();
}

// ─────────────────────────────────────────────────────────────
// Compiler
// ─────────────────────────────────────────────────────────────
Compiler::Compiler(CompilerOptions opts) : opts_(std::move(opts)) {}

std::string Compiler::derivedOutputName() const {
    if (!opts_.outputFile.empty()) return opts_.outputFile;
    fs::path p(opts_.inputFile);
#ifdef _WIN32
    return (p.parent_path() / p.stem()).string() + ".exe";
#else
    return (p.parent_path() / p.stem()).string();
#endif
}

std::string Compiler::findClang() const {
#ifdef _WIN32
    for (auto& c : {"clang", "clang++"}) {
        std::string test = std::string("where ") + c + " >nul 2>&1";
        if (std::system(test.c_str()) == 0) return c;
    }
    // Check default LLVM install location
    for (auto& root : {"C:\\Program Files\\LLVM\\bin\\clang.exe",
                        "C:\\Program Files (x86)\\LLVM\\bin\\clang.exe"}) {
        if (fs::exists(root)) return root;
    }
    return "clang"; // best-effort
#else
    for (auto& c : {"clang", "clang++"}) {
        std::string test = std::string("which ") + c + " >/dev/null 2>&1";
        if (std::system(test.c_str()) == 0) return c;
    }
    return "clang";
#endif
}

#ifdef _WIN32
static std::tuple<int, int, int> parseMsvcVersion(const std::string& verStr) {
    int major = 0, minor = 0, build = 0;
    std::sscanf(verStr.c_str(), "%d.%d.%d", &major, &minor, &build);
    return {major, minor, build};
}
#endif

std::string Compiler::findCppCompiler() const {
#ifdef _WIN32
    // 1. Check PATH for clang++ or g++ first (always work without special env)
    for (auto& cc : {"clang++", "g++"}) {
        std::string test = std::string("where ") + cc + " >nul 2>&1";
        if (std::system(test.c_str()) == 0) return cc;
    }
    // 2. Check PATH for cl.exe (available in VS Developer Command Prompt)
    if (std::system("where cl >nul 2>&1") == 0) return "cl";
    // 3. Search known MSVC installation paths and pick the highest MSVC toolset version
    {
        std::vector<std::string> vsRoots = {
            "C:\\Program Files\\Microsoft Visual Studio",
            "C:\\Program Files (x86)\\Microsoft Visual Studio",
        };
        std::vector<std::string> editions = {
            "Community", "Professional", "Enterprise", "BuildTools", "Preview",
        };

        std::string bestCl;
        std::tuple<int, int, int> bestVer = {-1, -1, -1};

        for (auto& root : vsRoots) {
            if (!fs::exists(root)) continue;
            std::error_code ec;
            for (auto& verDir : fs::directory_iterator(root, ec)) {
                if (ec || !verDir.is_directory()) continue;
                for (auto& ed : editions) {
                    fs::path vcbase = verDir.path() / ed / "VC" / "Tools" / "MSVC";
                    if (!fs::exists(vcbase)) continue;
                    for (auto& msvcVer : fs::directory_iterator(vcbase, ec)) {
                        if (ec || !msvcVer.is_directory()) continue;
                        for (auto& hostArch : {"Hostx64", "HostX64", "Hostx86", "HostX86"}) {
                            fs::path cl = msvcVer.path() / "bin" / hostArch / "x64" / "cl.exe";
                            if (fs::exists(cl)) {
                                auto verTuple = parseMsvcVersion(msvcVer.path().filename().string());
                                if (verTuple > bestVer) {
                                    bestVer = verTuple;
                                    bestCl = cl.string();
                                }
                                break;
                            }
                        }
                    }
                }
            }
        }
        if (!bestCl.empty()) return bestCl;
    }
    return "cl"; // last resort — will fail with a clear error message
#else
    for (auto& cc : {"clang++", "g++"}) {
        std::string test = std::string("which ") + cc + " >/dev/null 2>&1";
        if (std::system(test.c_str()) == 0) return cc;
    }
    return "g++";
#endif
}

std::string Compiler::buildCppSource(const std::string& umeSource,
                                      const std::string& filename) {
    // 1. Preprocess
    Preprocessor pp(opts_.includePaths);
    std::string processed = pp.process(umeSource, filename);

    // 2. Lex
    Lexer lexer(processed, filename);
    auto tokens = lexer.tokenize();

    // 3. Parse
    Parser parser(std::move(tokens), filename);
    auto program = parser.parse();

    // 3b. Resolve package imports
    PackageResolver resolver(opts_.includePaths);
    resolver.resolveImports(*program, filename);

    // 4. Semantic analysis
    SemanticAnalyzer analyzer;
    analyzer.analyze(*program);
    for (auto& w : analyzer.warnings())
        std::cerr << "[warning] " << w << "\n";

    // 5. Generate C++
    CodeGenerator codegen;
    return codegen.generate(*program, filename);
}

static std::string filterCompilerDiagnostics(const std::string& rawOutput) {
    std::istringstream in(rawOutput);
    std::ostringstream out;
    std::string line;
    bool firstLine = true;

    while (std::getline(in, line)) {
        std::string lower = line;
        std::transform(lower.begin(), lower.end(), lower.begin(), [](unsigned char c) {
            return static_cast<char>(std::tolower(c));
        });

        bool hasError = lower.find("error") != std::string::npos;
        bool hasWarning = lower.find("warning") != std::string::npos;
        bool hasFatal = lower.find("fatal") != std::string::npos;
        if (!hasError && !hasWarning && !hasFatal) {
            continue;
        }

        if (!firstLine) out << '\n';
        out << line;
        firstLine = false;
    }

    std::string filtered = out.str();
    return filtered.empty() ? rawOutput : filtered;
}

int Compiler::invokeCppCompiler(const std::string& cppFile,
                                 const std::string& outFile,
                                 std::string& compilerOutput) {
    std::string cc = findCppCompiler();
    std::string cmd;
    fs::path tmpOutput = fs::temp_directory_path() / "ume_compile_output.txt";
#ifdef _WIN32
    // Determine if this is cl.exe (bare name or full path)
    bool isMsvc = (cc == "cl" || cc.find("cl.exe") != std::string::npos ||
                   cc.find("CL.exe") != std::string::npos);
    if (isMsvc) {
        // Derive vcvarsall.bat from cl.exe path:
        // cl.exe is at: ROOT\VC\Tools\MSVC\VER\bin\Hostx64\x64\cl.exe
        // vcvarsall is:  ROOT\VC\Auxiliary\Build\vcvarsall.bat
        std::string vcvarsall;
        std::string msvcVerStr;
        if (cc != "cl") {
            fs::path clPath(cc);
            // Go up 4 levels from cl.exe to get MSVC toolset version directory
            fs::path verPath = clPath.parent_path().parent_path().parent_path().parent_path();
            if (fs::exists(verPath)) {
                msvcVerStr = verPath.filename().string();
            }
            // Go up 7 levels from cl.exe to get ROOT\VC
            fs::path vcDir = clPath;
            for (int i = 0; i < 7; ++i) vcDir = vcDir.parent_path();
            fs::path candidate = vcDir / "Auxiliary" / "Build" / "vcvarsall.bat";
            if (fs::exists(candidate)) vcvarsall = candidate.string();
        }
        // Write a temp .bat so cmd.exe handles the quoted path with spaces correctly
        fs::path tmpBat = fs::temp_directory_path() / "ume_build.bat";
        {
            std::ofstream bat(tmpBat.string());
            bat << "@echo off\r\n";
            if (!vcvarsall.empty()) {
                bat << "call \"" << vcvarsall << "\" amd64";
                if (!msvcVerStr.empty()) {
                    bat << " -vcvars_ver=" << msvcVerStr;
                }
                bat << " > nul 2>&1\r\n";
            }
            bat << "\"" << cc << "\" /std:c++20 /EHsc ";
            for (const auto& ip : opts_.includePaths) bat << "/I\"" << ip << "\" ";
            bat << "/Fe\"" << outFile << "\" ";
            bat << "\"" << cppFile << "\" /nologo";
            bat << (opts_.optimize ? " /O2" : " /Od");
            
            // Add linking phase options
            bat << " /link ";
            for (const auto& lp : opts_.libPaths) bat << "/LIBPATH:\"" << lp << "\" ";
            for (const auto& l : opts_.libs) bat << l << " ";
            
            bat << "\r\n";
        }
        cmd = "cmd /c \"" + tmpBat.string() + "\" > \"" + tmpOutput.string() + "\" 2>&1";
    } else {
        cmd = "\"" + cc + "\" -std=c++20 -o \"" + outFile + "\" \"" + cppFile + "\"";
        if (opts_.optimize) cmd += " -O2";
        cmd += " > \"" + tmpOutput.string() + "\" 2>&1";
    }
#else
    cmd = "\"" + cc + "\" -std=c++20 -o \"" + outFile + "\" \"" + cppFile + "\"";
    if (opts_.optimize) cmd += " -O2";
    cmd += " > \"" + tmpOutput.string() + "\" 2>&1";
#endif
    int rc = std::system(cmd.c_str());

    std::ifstream captured(tmpOutput.string());
    if (captured.is_open()) {
        std::ostringstream ss;
        ss << captured.rdbuf();
        compilerOutput = filterCompilerDiagnostics(ss.str());
        captured.close();
        std::error_code ec;
        fs::remove(tmpOutput, ec);
    } else {
        compilerOutput.clear();
    }
    return rc;
}

// ─────────────────────────────────────────────────────────────
// LLVM pipeline
// ─────────────────────────────────────────────────────────────
int Compiler::invokeLLVMCompiler(const std::string& llFile,
                                  const std::string& outFile) {
    std::string clang = findClang();
#ifdef _WIN32
    fs::path tmpBat = fs::temp_directory_path() / "ume_llvm_build.bat";
    {
        std::ofstream bat(tmpBat.string());
        bat << "@echo off\r\n";
        bat << "\"" << clang << "\" -O2 \"" << llFile << "\" -o \"" << outFile << "\"\r\n";
    }
    return std::system(("cmd /c \"" + tmpBat.string() + "\"").c_str());
#else
    std::string cmd = "\"" + clang + "\" -O2 \"" + llFile + "\" -o \"" + outFile + "\" -lm";
    return std::system(cmd.c_str());
#endif
}

CompilerResult Compiler::compileLLVM(const std::string& source) {
    CompilerResult result;
    try {
        // 1. Preprocess
        Preprocessor pp(opts_.includePaths);
        std::string processed = pp.process(source, opts_.inputFile);

        // 2. Lex
        Lexer lexer(processed, opts_.inputFile);
        auto tokens = lexer.tokenize();

        // 3. Parse
        Parser parser(std::move(tokens), opts_.inputFile);
        auto program = parser.parse();

        // 3b. Resolve package imports
        PackageResolver resolver(opts_.includePaths);
        resolver.resolveImports(*program, opts_.inputFile);

        // 4. Semantic analysis
        SemanticAnalyzer analyzer;
        analyzer.analyze(*program);
        for (auto& w : analyzer.warnings())
            std::cerr << "[warning] " << w << "\n";

        // 5. Generate LLVM IR
        LLVMCodegen cg;
        auto cgResult = cg.generate(*program);
        if (!cgResult.success) {
            result.errorMessage = "LLVM codegen error: " + cgResult.errorMessage;
            return result;
        }

        // 6. Write .ll file
        fs::path tmpLL = fs::temp_directory_path() / "ume_gen.ll";
        {
            std::ofstream out(tmpLL.string());
            if (!out) {
                result.errorMessage = "Cannot write LLVM IR file: " + tmpLL.string();
                return result;
            }
            out << cgResult.irOutput;
        }

        std::string outFile = derivedOutputName();
        fs::path outDir = fs::path(outFile).parent_path();
        if (!outDir.empty()) fs::create_directories(outDir);

        // 7. Invoke clang to compile IR → native binary
        int rc = invokeLLVMCompiler(tmpLL.string(), outFile);
        if (rc != 0) {
            result.errorMessage = "clang returned non-zero exit code: " + std::to_string(rc);
            result.exitCode     = rc;
            return result;
        }

        result.success    = true;
        result.outputFile = outFile;
        result.exitCode   = 0;
    } catch (const std::exception& e) {
        result.errorMessage = e.what();
    }
    return result;
}

CompilerResult Compiler::compile() {
    CompilerResult result;
    try {
        // Read input
        std::ifstream f(opts_.inputFile);
        if (!f.is_open()) {
            result.errorMessage = "Cannot open input file: " + opts_.inputFile;
            return result;
        }
        std::ostringstream ss; ss << f.rdbuf();
        std::string source = ss.str();

        // Route to LLVM pipeline when requested
        if (opts_.backend == "llvm") return compileLLVM(source);

        // Build C++ source
        std::string cppSrc = buildCppSource(source, opts_.inputFile);

        // Write to temp file
        fs::path tmpCpp = fs::temp_directory_path() / "ume_gen.cpp";
        {
            std::ofstream out(tmpCpp.string());
            if (!out) {
                result.errorMessage = "Cannot write temp file: " + tmpCpp.string();
                return result;
            }
            out << cppSrc;
        }

        std::string outFile = derivedOutputName();

        // Ensure output directory exists
        fs::path outDir = fs::path(outFile).parent_path();
        if (!outDir.empty()) fs::create_directories(outDir);

        // Invoke C++ compiler
        std::string compilerOutput;
        int rc = invokeCppCompiler(tmpCpp.string(), outFile, compilerOutput);
        if (rc != 0) {
            result.errorMessage = "C++ compiler returned non-zero exit code: " + std::to_string(rc);
            if (!compilerOutput.empty()) {
                result.errorMessage += "\n" + compilerOutput;
            }
            result.exitCode     = rc;
            return result;
        }

        result.success    = true;
        result.outputFile = outFile;
        result.exitCode   = 0;

    } catch (const std::exception& e) {
        result.errorMessage = e.what();
    }
    return result;
}

} // namespace Ume
