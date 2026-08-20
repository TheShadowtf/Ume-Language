#pragma once
// package_resolver.h — Package and Selective Import Resolver for Ume
#include "ast.h"
#include <string>
#include <vector>
#include <unordered_map>
#include <unordered_set>
#include <memory>

namespace Ume {

class PackageResolver {
public:
    explicit PackageResolver(std::vector<std::string> searchPaths = {});

    void addSearchPath(const std::string& path);

    // Resolve all ImportDecl nodes in the program in-place
    void resolveImports(Program& program, const std::string& currentFilePath);

private:
    std::vector<std::string> searchPaths_;
    std::unordered_map<std::string, std::vector<std::string>> packageMap_; // packageName -> filePaths
    std::unordered_set<std::string> visitedFiles_;
    bool scanned_ = false;

    void scanPackages();
    void scanDirectory(const std::string& dirPath);

    std::string findPackageFile(const std::string& importPath,
                                const std::string& currentDir,
                                std::string& outSymbol,
                                bool& isWildcard);

    std::unique_ptr<Program> loadPackageAST(const std::string& filePath);

    bool isDeclared(const Program& prog, const std::string& name) const;
    std::string getDeclName(const ASTNode* decl) const;
};

} // namespace Ume
