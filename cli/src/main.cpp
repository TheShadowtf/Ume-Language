#include <iostream>
#include <fstream>
#include <filesystem>
#include <string>
#include <vector>
#include <algorithm>
#include <cstdlib>
#ifdef _WIN32
#include <windows.h>
#else
#include <unistd.h>
#include <limits.h>
#endif
#include "../../compiler/include/compiler.h"
#include "../../interpreter/include/interpreter.h"

namespace fs = std::filesystem;

static std::string detectStdlibPath(const char* argv0) {
    // Prefer explicit environment override if provided.
    if (auto env = std::getenv("UME_STDLIB_PATH")) {
        fs::path candidate(env);
        if (fs::exists(candidate) && fs::is_directory(candidate))
            return candidate.string();
    }

    auto findStdlib = [&](fs::path start) -> std::string {
        fs::path p = start;
        for (int i = 0; i < 6; i++) {
            auto candidate = p / "stdlib";
            if (fs::exists(candidate) && fs::is_directory(candidate))
                return candidate.string();
            if (!p.has_parent_path() || p == p.parent_path()) break;
            p = p.parent_path();
        }
        return "";
    };

    fs::path exePath;
#ifdef _WIN32
    char buffer[MAX_PATH];
    if (GetModuleFileNameA(NULL, buffer, MAX_PATH)) {
        exePath = fs::path(buffer).parent_path();
    } else {
        exePath = fs::current_path();
    }
#else
    char buffer[PATH_MAX];
    ssize_t len = readlink("/proc/self/exe", buffer, sizeof(buffer) - 1);
    if (len > 0) {
        buffer[len] = '\0';
        exePath = fs::path(buffer).parent_path();
    } else {
        std::error_code ec;
        exePath = fs::canonical(fs::absolute(argv0), ec);
        if (ec) exePath = fs::current_path();
        else exePath = exePath.parent_path();
    }
#endif

    if (auto result = findStdlib(exePath); !result.empty())
        return result;
    if (auto result = findStdlib(fs::current_path()); !result.empty())
        return result;
    return "";
}
static void printVersion() {
    std::cout << "Ume Language 1.0.0\n";
}

static void printHelp() {
    std::cout <<
        "Usage: ume <command> [options]\n\n"
        "Commands:\n"
        "  -run    <file.ume> [-- arg...]   Interpret and run a .ume file\n"
        "  -compile <file.ume> [-o <out>]   Compile a .ume file to native binary\n"
        "  new     <ProjectName>            Scaffold a new Ume project\n"
        "  build [<projectDir>]             Build the project in the specified directory\n"
        "  run [<projectDir>]               Run the project in the specified directory\n"
        "  repl                             Start the interactive REPL\n"
        "  version                          Print version information\n"
        "  help                             Show this help message\n\n"
        "Options:\n"
        "  -o <output>   Output path for compiled binary\n"
        "  -- <args>     Pass remaining arguments to the Ume program\n";
}

static std::string readTomlValue(const std::string& toml, const std::string& key) {
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

static int cmdRun(const std::string& inputFile, const std::string& stdlibPath, std::vector<std::string> programArgs = {}) {
    Ume::InterpreterOptions opts;
    opts.filename   = inputFile;
    opts.args       = std::move(programArgs);
    opts.stdlibPath = stdlibPath;
    Ume::Interpreter interpreter(opts);
    Ume::InterpreterResult result = interpreter.run();
    if (!result.success) {
        std::cerr << result.errorMessage;
        if (!result.errorMessage.empty() && result.errorMessage.back() != '\n') std::cerr << '\n';
        return 1;
    }
    return result.exitCode;
}

static int cmdCompile(const std::string& inputFile, const std::string& outputFile,
                      const std::string& backend, const std::string& stdlibPath) {
    Ume::CompilerOptions opts;
    opts.inputFile  = inputFile;
    opts.outputFile = outputFile.empty() ?
        fs::path(inputFile).replace_extension("").string() : outputFile;
    opts.backend    = backend;
    if (!stdlibPath.empty()) {
        fs::path p = fs::path(stdlibPath);
        opts.includePaths.push_back(p.string());
        opts.includePaths.push_back(p.parent_path().string());
        
        // Add bin/release/include for graphics bindings
        fs::path binRel = p.parent_path() / "bin" / "release";
        opts.includePaths.push_back((binRel / "include").string());
        
        // Add library paths
        opts.libPaths.push_back(binRel.string());
        
        // Add standard graphics libraries
        opts.libs.push_back("ume_graphics_lib.lib");
        opts.libs.push_back("glfw3dll.lib");
        opts.libs.push_back("glad.lib");
        opts.libs.push_back("opengl32.lib");
        opts.libs.push_back("User32.lib");
        opts.libs.push_back("Gdi32.lib");
        opts.libs.push_back("Shell32.lib");
    }
    Ume::Compiler compiler(opts);
    Ume::CompilerResult result = compiler.compile();
    if (!result.success) {
        std::cerr << "Compilation failed:\n" << result.errorMessage << '\n';
        return 1;
    }
    std::cout << "Compiled: " << result.outputFile << '\n';
    return 0;
}

static int cmdNew(const std::string& projectName) {
    if (projectName.empty()) {
        std::cerr << "Error: Project name required\n";
        return 1;
    }

    for (char c : projectName) {
        if (!std::isalnum(static_cast<unsigned char>(c)) && c != '_' && c != '-') {
            std::cerr << "Error: Project name contains invalid characters\n";
            return 1;
        }
    }

    fs::path base(projectName);
    if (fs::exists(base)) {
        std::cerr << "Error: Directory '" << projectName << "' already exists\n";
        return 1;
    }

    fs::create_directories(base / "src");
    fs::create_directories(base / "build");

    // ume.toml
    std::ofstream toml((base / "ume.toml").string());
    toml << "name    = \"" << projectName << "\"\n"
         << "version = \"1.0.0\"\n"
         << "\n"
         << "# Entry point: path to the main .ume file (relative to project root)\n"
         << "entry   = \"src/main.ume\"\n"
         << "\n"
         << "# Compiler backend: \"cpp\" (default) or \"llvm\" (requires clang on PATH)\n"
         << "backend = \"cpp\"\n";
    toml.close();

    // src/main.ume Hello World
    std::ofstream main_ume((base / "src" / "main.ume").string());
    main_ume <<
        "package " << projectName << ";\n\n"
        "public class Main {\n"
        "    public static func void main() {\n"
        "        Console.println(\"Hello, World!\");\n"
        "    }\n"
        "}\n";
    main_ume.close();

    // .gitignore
    std::ofstream gitignore((base / ".gitignore").string());
    gitignore << "build/\n*.exe\n*.out\n";
    gitignore.close();

    std::cout << "Created project '" << projectName << "'\n";
    std::cout << "  " << projectName << "/ume.toml\n";
    std::cout << "  " << projectName << "/src/main.ume\n";
    std::cout << "\nRun: cd " << projectName << " && ume run\n";
    return 0;
}

static int cmdBuild(const std::string& projectPath, const std::string& stdlibPath) {
    fs::path projectDir = projectPath.empty() ? fs::path(".") : fs::path(projectPath);
    fs::path tomlPath = projectDir / "ume.toml";
    if (!fs::exists(tomlPath)) {
        std::cerr << "Error: No ume.toml found in project directory '" << projectDir.string() << "'\n";
        return 1;
    }
    std::ifstream f(tomlPath.string());
    std::ostringstream buf; buf << f.rdbuf();
    std::string toml = buf.str();

    std::string entry   = readTomlValue(toml, "entry");
    std::string name    = readTomlValue(toml, "name");
    std::string backend = readTomlValue(toml, "backend");
    std::string assetsStr = readTomlValue(toml, "assets");
    if (entry.empty())   entry   = "src/main.ume";
    if (name.empty())    name    = "out";
    if (backend.empty()) backend = "cpp";

    std::vector<std::pair<std::string, std::string>> assets;
    if (!assetsStr.empty()) {
        std::stringstream ss(assetsStr);
        std::string item;
        while (std::getline(ss, item, ',')) {
            size_t start = item.find_first_not_of(" \t\r\n");
            size_t end = item.find_last_not_of(" \t\r\n");
            if (start != std::string::npos) {
                std::string entry = item.substr(start, end - start + 1);
                size_t arrowPos = entry.find("->");
                if (arrowPos != std::string::npos) {
                    std::string src = entry.substr(0, arrowPos);
                    std::string dst = entry.substr(arrowPos + 2);
                    size_t srcS = src.find_first_not_of(" \t\r\n");
                    size_t srcE = src.find_last_not_of(" \t\r\n");
                    size_t dstS = dst.find_first_not_of(" \t\r\n");
                    size_t dstE = dst.find_last_not_of(" \t\r\n");
                    if (srcS != std::string::npos && dstS != std::string::npos) {
                        assets.push_back({src.substr(srcS, srcE - srcS + 1), dst.substr(dstS, dstE - dstS + 1)});
                    }
                } else {
                    assets.push_back({entry, entry});
                }
            }
        }
    }

    fs::path inputFile = projectDir / entry;
    fs::path outputFile = projectDir / "build" / name;
    fs::create_directories(outputFile.parent_path());
    int rc = cmdCompile(inputFile.string(), outputFile.string(), backend, stdlibPath);
    if (rc == 0) {
        for (const auto& item : assets) {
            fs::path srcPath = projectDir / item.first;
            fs::path dstPath = projectDir / "build" / item.second;
            try {
                fs::create_directories(dstPath.parent_path());
                fs::copy(srcPath, dstPath, fs::copy_options::recursive | fs::copy_options::overwrite_existing);
                std::cout << "Copied asset: " << item.first << (item.first != item.second ? " -> " + item.second : "") << "\n";
            } catch (const std::exception& e) {
                std::cerr << "Warning: Failed to copy asset " << item.first << " - " << e.what() << "\n";
            }
        }
    }
    return rc;
}

static int cmdRunProject(const std::string& projectPath, const std::string& stdlibPath) {
    fs::path projectDir = projectPath.empty() ? fs::path(".") : fs::path(projectPath);
    fs::path tomlPath = projectDir / "ume.toml";
    if (!fs::exists(tomlPath)) {
        std::cerr << "Error: No ume.toml found in project directory '" << projectDir.string() << "'\n";
        return 1;
    }
    std::ifstream f(tomlPath.string());
    std::ostringstream buf; buf << f.rdbuf();
    std::string toml  = buf.str();
    std::string entry = readTomlValue(toml, "entry");
    if (entry.empty()) entry = "src/main.ume";
    fs::path inputFile = projectDir / entry;
    return cmdRun(inputFile.string(), stdlibPath);
}

static int cmdRepl(const std::string& stdlibPath) {
    Ume::InterpreterOptions opts;
    opts.stdlibPath = stdlibPath;
    Ume::Interpreter interpreter(opts);
    Ume::InterpreterResult result = interpreter.runREPL();
    return result.success ? 0 : 1;
}

int main(int argc, char* argv[]) {
    if (argc < 2) {
        printHelp();
        return 1;
    }

    std::string cmd = argv[1];

    std::string stdlibPath = detectStdlibPath(argv[0]);

    bool needsStdlib = (cmd == "build" || cmd == "run" || cmd == "repl" ||
                        cmd == "-run" || cmd == "-compile");
    if (needsStdlib) {
        if (stdlibPath.empty()) {
            std::cerr << "Warning: stdlib/ not found. "
                         "Set UME_STDLIB_PATH or place stdlib/ next to ume.exe or in the project root.\n";
        } else {
            std::cerr << "Using stdlib path: " << stdlibPath << "\n";
        }
    }

    if (cmd == "version" || cmd == "--version" || cmd == "-version" || cmd == "-v") {
        printVersion();
        return 0;
    }
    if (cmd == "help" || cmd == "--help" || cmd == "-help" || cmd == "-h") {
        printHelp();
        return 0;
    }

    if (cmd == "-run") {
        if (argc < 3) { std::cerr << "Error: No input file specified\n"; return 1; }
        std::string inputFile = argv[2];
        // Allow passing a project directory to -run (treat like `run <projectDir>`)
        try {
            fs::path p(inputFile);
            if (fs::exists(p) && fs::is_directory(p)) {
                // Read ume.toml if present to find the entry point
                fs::path tomlPath = p / "ume.toml";
                if (fs::exists(tomlPath)) {
                    std::ifstream f(tomlPath.string());
                    std::ostringstream buf; buf << f.rdbuf();
                    std::string toml = buf.str();
                    std::string entry = readTomlValue(toml, "entry");
                    if (entry.empty()) entry = "src/main.ume";
                    inputFile = (p / entry).string();
                } else {
                    // Fallback to src/main.ume
                    fs::path candidate = p / "src" / "main.ume";
                    if (fs::exists(candidate)) inputFile = candidate.string();
                }
            }
        } catch (...) {}
        // Collect args after "--" separator
        std::vector<std::string> programArgs;
        bool pastSep = false;
        for (int i = 3; i < argc; i++) {
            if (!pastSep && std::string(argv[i]) == "--") { pastSep = true; continue; }
            if (pastSep) programArgs.push_back(argv[i]);
        }
        return cmdRun(inputFile, stdlibPath, std::move(programArgs));
    }

    if (cmd == "-compile") {
        if (argc < 3) { std::cerr << "Error: No input file specified\n"; return 1; }
        std::string inputFile  = argv[2];
        std::string outputFile;
        std::string backend = "cpp";
        for (int i = 3; i < argc; i++) {
            if (std::string(argv[i]) == "-o" && i + 1 < argc) {
                outputFile = argv[++i];
            } else if (std::string(argv[i]) == "--backend" && i + 1 < argc) {
                backend = argv[++i];
            }
        }
        return cmdCompile(inputFile, outputFile, backend, stdlibPath);
    }

    if (cmd == "new") {
        std::string name = argc >= 3 ? argv[2] : "";
        return cmdNew(name);
    }

    if (cmd == "build") {
        std::string projectPath = argc >= 3 ? argv[2] : ".";
        return cmdBuild(projectPath, stdlibPath);
    }
    if (cmd == "run") {
        std::string projectPath = argc >= 3 ? argv[2] : ".";
        return cmdRunProject(projectPath, stdlibPath);
    }
    if (cmd == "repl")  return cmdRepl(stdlibPath);

    std::cerr << "Unknown command: " << cmd << "\n\nRun 'ume help' for usage.\n";
    return 1;
}


