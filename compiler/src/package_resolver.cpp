// package_resolver.cpp — Package and Selective Import Resolver for Ume
#include "../include/package_resolver.h"
#include "../include/compiler.h"
#include "../include/lexer.h"
#include "../include/parser.h"

#include <fstream>
#include <sstream>
#include <iostream>
#include <filesystem>
#include <algorithm>

namespace fs = std::filesystem;

namespace Ume {

PackageResolver::PackageResolver(std::vector<std::string> searchPaths)
    : searchPaths_(std::move(searchPaths)) {}

void PackageResolver::addSearchPath(const std::string& path) {
    if (!path.empty() && std::find(searchPaths_.begin(), searchPaths_.end(), path) == searchPaths_.end()) {
        searchPaths_.push_back(path);
    }
}

void PackageResolver::scanPackages() {
    if (scanned_) return;
    scanned_ = true;

    for (const auto& sp : searchPaths_) {
        scanDirectory(sp);
    }
}

void PackageResolver::scanDirectory(const std::string& dirPath) {
    std::error_code ec;
    if (!fs::exists(dirPath, ec) || !fs::is_directory(dirPath, ec)) return;

    for (const auto& entry : fs::directory_iterator(dirPath, ec)) {
        if (ec) break;
        if (entry.is_regular_file() && entry.path().extension() == ".ume") {
            std::ifstream file(entry.path());
            if (!file.is_open()) continue;

            std::string line;
            while (std::getline(file, line)) {
                // Trim leading spaces
                size_t start = 0;
                while (start < line.size() && (line[start] == ' ' || line[start] == '\t')) start++;

                std::string keyword;
                if (line.substr(start, 8) == "package ") keyword = "package ";
                else if (line.substr(start, 10) == "namespace ") keyword = "namespace ";

                if (!keyword.empty()) {
                    size_t kLen = keyword.size();
                    size_t endPos = line.find_first_of(";{\r\n", start + kLen);
                    if (endPos != std::string::npos) {
                        std::string pkgName = line.substr(start + kLen, endPos - (start + kLen));
                        while (!pkgName.empty() && (pkgName.back() == ' ' || pkgName.back() == '\t' || pkgName.back() == '\r'))
                            pkgName.pop_back();
                        if (!pkgName.empty()) {
                            auto& list = packageMap_[pkgName];
                            std::string absPath = fs::absolute(entry.path()).string();
                            if (std::find(list.begin(), list.end(), absPath) == list.end())
                                list.push_back(absPath);
                        }
                    }
                    break;
                }

                // Stop scanning lines if we see non-comment non-empty content
                if (start < line.size() && line[start] != '/' && line[start] != '#' && line[start] != '\r' && line[start] != '\n') {
                    // Stop checking further lines for performance
                    break;
                }
            }
        } else if (entry.is_directory()) {
            // Recursively scan subdirectories only if they look like package dirs
            // (contain a package.ume or any .ume files at the top level)
            scanDirectory(entry.path().string());
        }
    }
}

std::string PackageResolver::findPackageFile(const std::string& rawImportPath,
                                              const std::string& currentDir,
                                              std::string& outSymbol,
                                              bool& isWildcard) {
    scanPackages();

    std::string pathStr = rawImportPath;
    outSymbol.clear();

    if (pathStr.size() >= 2 && pathStr.substr(pathStr.size() - 2) == ".*") {
        isWildcard = true;
        pathStr = pathStr.substr(0, pathStr.size() - 2);
    }

    // 1. Exact match in packageMap_
    auto it = packageMap_.find(pathStr);
    if (it != packageMap_.end() && !it->second.empty()) {
        return it->second.front();
    }

    // 2. Dotted split match in packageMap_ (e.g. packageMap_["mypkg.tools"] + symbol "add")
    size_t lastDot = pathStr.find_last_of('.');
    if (lastDot != std::string::npos) {
        std::string pkgPart = pathStr.substr(0, lastDot);
        std::string symPart = pathStr.substr(lastDot + 1);
        auto pit = packageMap_.find(pkgPart);
        if (pit != packageMap_.end() && !pit->second.empty()) {
            outSymbol = symPart;
            return pit->second.front();
        }
    }

    // Helper to test existing file candidates
    auto tryCandidate = [](const fs::path& p) -> std::string {
        std::error_code ec;
        if (fs::exists(p, ec) && fs::is_regular_file(p, ec)) {
            return fs::absolute(p, ec).string();
        }
        return "";
    };

    // Convert dotted path to relative filesystem path ("mypkg.tools" -> "mypkg/tools")
    std::string relPathStr = pathStr;
    std::replace(relPathStr.begin(), relPathStr.end(), '.', '/');

    std::vector<std::string> baseDirs;
    if (!currentDir.empty()) baseDirs.push_back(currentDir);
    for (const auto& sp : searchPaths_) baseDirs.push_back(sp);
    baseDirs.push_back("."); // Current working directory fallback

    // 3. Try relPathStr as file (e.g. mypkg/tools.ume) or package directory (mypkg/tools/package.ume)
    for (const auto& bd : baseDirs) {
        fs::path p1 = fs::path(bd) / (relPathStr + ".ume");
        std::string res1 = tryCandidate(p1);
        if (!res1.empty()) return res1;

        fs::path p2 = fs::path(bd) / relPathStr / "package.ume";
        std::string res2 = tryCandidate(p2);
        if (!res2.empty()) return res2;
    }

    // 4. Try stripping last segment as symbol (e.g. relPathStr = "mypkg/tools/add" -> file "mypkg/tools.ume", symbol "add")
    if (lastDot != std::string::npos) {
        std::string relPathShort = pathStr.substr(0, lastDot);
        std::replace(relPathShort.begin(), relPathShort.end(), '.', '/');
        std::string symPart = pathStr.substr(lastDot + 1);

        for (const auto& bd : baseDirs) {
            fs::path p1 = fs::path(bd) / (relPathShort + ".ume");
            std::string res1 = tryCandidate(p1);
            if (!res1.empty()) {
                outSymbol = symPart;
                return res1;
            }

            fs::path p2 = fs::path(bd) / relPathShort / "package.ume";
            std::string res2 = tryCandidate(p2);
            if (!res2.empty()) {
                outSymbol = symPart;
                return res2;
            }
        }
    }

    // 5. Bare filename matching (e.g. "math.ume" or "graphics.ume")
    for (const auto& bd : baseDirs) {
        fs::path p = fs::path(bd) / pathStr;
        if (p.extension().empty()) p.replace_extension(".ume");
        std::string res = tryCandidate(p);
        if (!res.empty()) return res;
    }

    return "";
}

std::unique_ptr<Program> PackageResolver::loadPackageAST(const std::string& filePath) {
    std::ifstream file(filePath);
    if (!file.is_open()) return nullptr;

    std::ostringstream ss;
    ss << file.rdbuf();
    std::string source = ss.str();

    std::string parentDir = fs::path(filePath).parent_path().string();

    Preprocessor prep(searchPaths_);
    std::string preprocessed = prep.process(source, filePath);

    Lexer lexer(preprocessed, filePath);
    std::vector<Token> tokens = lexer.tokenize();

    Parser parser(std::move(tokens), filePath);
    return parser.parse();
}

std::string PackageResolver::getDeclName(const ASTNode* decl) const {
    if (auto* f = dynamic_cast<const FuncDecl*>(decl))       return f->name;
    if (auto* c = dynamic_cast<const ClassDecl*>(decl))      return c->name;
    if (auto* s = dynamic_cast<const StructDecl*>(decl))     return s->name;
    if (auto* e = dynamic_cast<const EnumDecl*>(decl))       return e->name;
    if (auto* i = dynamic_cast<const InterfaceDecl*>(decl))  return i->name;
    if (auto* fd = dynamic_cast<const FieldDecl*>(decl))     return fd->name;
    if (auto* vd = dynamic_cast<const VarDeclStmt*>(decl))   return vd->name;
    return "";
}

bool PackageResolver::isDeclared(const Program& prog, const std::string& name) const {
    if (name.empty()) return false;
    for (const auto& decl : prog.declarations) {
        if (getDeclName(decl.get()) == name) return true;
    }
    return false;
}

void PackageResolver::resolveImports(Program& program, const std::string& currentFilePath) {
    std::string currentDir;
    if (!currentFilePath.empty()) {
        try {
            std::error_code ec;
            std::string canonical = fs::weakly_canonical(fs::absolute(currentFilePath), ec).string();
            if (visitedFiles_.count(canonical)) return;
            visitedFiles_.insert(canonical);
            currentDir = fs::path(canonical).parent_path().string();
        } catch (...) {
            currentDir = fs::path(currentFilePath).parent_path().string();
        }
    }
    if (!currentDir.empty()) scanDirectory(currentDir);

    std::vector<ASTNodePtr> resolvedDecls;
    resolvedDecls.reserve(program.declarations.size());

    // --- MAKE SURE THIS LINE IS HERE ---
    scanPackages(); 
    // -----------------------------------

    // 1. Detect current file package/namespace name
    std::string currentPkgName;
    for (const auto& d : program.declarations) {
        if (auto* pd = dynamic_cast<const PackageDecl*>(d.get())) {
            currentPkgName = pd->name;
            break;
        }
        if (auto* nd = dynamic_cast<const NamespaceDecl*>(d.get())) {
            currentPkgName = nd->name;
            break;
        }
    }
    


    // Helper to check if a declaration name is already present
    auto isAlreadyDeclared = [&](const std::string& n) -> bool {
        if (n.empty()) return false;
        if (isDeclared(program, n)) return true;
        for (const auto& d : resolvedDecls) {
            if (getDeclName(d.get()) == n) return true;
        }
        return false;
    };

    // Helper to extract declarations into resolvedDecls
    std::function<void(std::vector<ASTNodePtr>&, const std::string&, bool)> extractDecls =
        [&](std::vector<ASTNodePtr>& decls, const std::string& targetSymbol, bool wildcard) {
            for (auto& pDecl : decls) {
                if (!pDecl) continue;
                if (dynamic_cast<PackageDecl*>(pDecl.get()) || dynamic_cast<ImportDecl*>(pDecl.get()))
                    continue;

                if (auto* ns = dynamic_cast<NamespaceDecl*>(pDecl.get())) {
                    extractDecls(ns->declarations, targetSymbol, wildcard);
                    continue;
                }

                std::string name = getDeclName(pDecl.get());

                if (!targetSymbol.empty() && !wildcard) {
                    if (name == targetSymbol) {
                        if (!isAlreadyDeclared(name)) {
                            resolvedDecls.push_back(std::move(pDecl));
                        }
                    }
                } else {
                    if (name.empty() || !isAlreadyDeclared(name)) {
                        resolvedDecls.push_back(std::move(pDecl));
                    }
                }
            }
        };

    // Auto-import other files belonging to the same package in the same directory structure
    auto isSamePackagePath = [](const fs::path& dir1, const fs::path& dir2) -> bool {
        std::error_code ec;
        fs::path c1 = fs::weakly_canonical(dir1, ec);
        fs::path c2 = fs::weakly_canonical(dir2, ec);
        if (ec || c1.empty() || c2.empty()) return dir1 == dir2;
        return c1 == c2; // Exact match only to prevent false positives
    };

    if (!currentPkgName.empty()) {
        auto it = packageMap_.find(currentPkgName);
        if (it != packageMap_.end()) {
            std::vector<std::string> candidates = it->second;
            for (const auto& pkgFile : candidates) {
                std::error_code ec;
                fs::path canonPkgPath = fs::weakly_canonical(fs::absolute(pkgFile), ec);
                std::string canonStr = canonPkgPath.string();

                if (visitedFiles_.count(canonStr) || (!currentFilePath.empty() && canonStr == fs::weakly_canonical(fs::absolute(currentFilePath), ec).string())) {
                    continue;
                }

                // Removed the directory check! If it's in the packageMap_ 
                // under the same package name, we just load it.
                auto pkgAST = loadPackageAST(pkgFile);
                if (pkgAST) {
                    resolveImports(*pkgAST, pkgFile);
                    extractDecls(pkgAST->declarations, "", true);
                } else {
                    std::cerr << "[PackageResolver] Error: Auto-import failed to load/parse: " << pkgFile << "\n";
                }
            }
        }
    } else if (!currentDir.empty()) {
        std::error_code ec;
        if (fs::exists(currentDir, ec) && fs::is_directory(currentDir, ec)) {
            for (const auto& entry : fs::directory_iterator(currentDir, ec)) {
                if (ec) break;
                if (entry.is_regular_file() && entry.path().extension() == ".ume") {
                    fs::path canonEntryPath = fs::weakly_canonical(fs::absolute(entry.path()), ec);
                    std::string canonStr = canonEntryPath.string();

                    if (visitedFiles_.count(canonStr) || (!currentFilePath.empty() && canonStr == fs::weakly_canonical(fs::absolute(currentFilePath), ec).string())) {
                        continue;
                    }

                    // Check if entry has no package declaration
                    std::ifstream file(entry.path());
                    if (!file.is_open()) continue;
                    std::string line;
                    bool hasPkg = false;
                    while (std::getline(file, line)) {
                        size_t start = 0;
                        while (start < line.size() && (line[start] == ' ' || line[start] == '\t')) start++;
                        if (line.substr(start, 8) == "package " || line.substr(start, 10) == "namespace ") {
                            hasPkg = true;
                            break;
                        }
                        if (start < line.size() && line[start] != '/' && line[start] != '#' && line[start] != '\r' && line[start] != '\n') {
                            break;
                        }
                    }
                    if (!hasPkg) {
                        auto pkgAST = loadPackageAST(entry.path().string());
                        if (pkgAST) {
                            resolveImports(*pkgAST, entry.path().string());
                            extractDecls(pkgAST->declarations, "", true);
                        }
                    }
                }
            }
        }
    }

        for (auto& decl : program.declarations) {
        auto* imp = dynamic_cast<ImportDecl*>(decl.get());
        if (!imp) {
            resolvedDecls.push_back(std::move(decl));
            continue;
        }

        std::string targetSymbol = imp->symbol;
        bool wildcard = imp->wildcard;

        // --- NEW: Handle wildcard imports by loading ALL files in the package ---
        if (wildcard) {
            std::string pkgName = imp->path;
            if (pkgName.size() >= 2 && pkgName.substr(pkgName.size() - 2) == ".*") {
                pkgName = pkgName.substr(0, pkgName.size() - 2);
            }

            // Look up all files registered under this package name
            auto it = packageMap_.find(pkgName);
            if (it != packageMap_.end()) {
                for (const auto& pkgFile : it->second) {
                    auto pkgAST = loadPackageAST(pkgFile);
                    if (!pkgAST) {
                        std::cerr << "[PackageResolver] Error: Failed to load/parse package file: " << pkgFile << "\n";
                        continue;
                    }
                    resolveImports(*pkgAST, pkgFile);
                    extractDecls(pkgAST->declarations, "", true);
                }
            } else {
                std::cerr << "[PackageResolver] Warning: Wildcard package not found: " << pkgName << "\n";
            }
            continue;
        }
        // -----------------------------------------------------------------------

        std::string pkgFile = findPackageFile(imp->path, currentDir, targetSymbol, wildcard);

        if (pkgFile.empty()) {
            std::cerr << "[PackageResolver] Warning: Could not resolve import: " << imp->path << "\n";
            resolvedDecls.push_back(std::move(decl));
            continue;
        }

        auto pkgAST = loadPackageAST(pkgFile);
        if (!pkgAST) {
            std::cerr << "[PackageResolver] Error: Failed to load/parse package file: " << pkgFile << "\n";
            resolvedDecls.push_back(std::move(decl));
            continue;
        }

        // Recursively resolve imports in the package file
        resolveImports(*pkgAST, pkgFile);

        // Filter and inject declarations from loaded package AST
        extractDecls(pkgAST->declarations, targetSymbol, wildcard);
    }

    // Sort declarations to help the C++ codegen with forward references
    std::stable_sort(resolvedDecls.begin(), resolvedDecls.end(),
        [](const ASTNodePtr& a, const ASTNodePtr& b) {
            auto rank = [](const ASTNode* n) {
                if (dynamic_cast<const VarDeclStmt*>(n)) return 0; // Global vars first
                if (dynamic_cast<const ClassDecl*>(n)) return 1;   // Classes next
                if (dynamic_cast<const EnumDecl*>(n)) return 1;    // Enums next
                if (dynamic_cast<const FuncDecl*>(n)) return 2;    // Functions last
                return 3;
            };
            return rank(a.get()) < rank(b.get());
        });

    program.declarations = std::move(resolvedDecls);
}

} // namespace Ume
