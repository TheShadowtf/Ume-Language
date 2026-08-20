// diagnostics.cpp — Rich Diagnostic Reporting Engine implementation for Ume Language
#include "../include/diagnostics.h"

#include <fstream>
#include <sstream>
#include <iomanip>
#include <algorithm>
#include <filesystem>
#include <unordered_map>

#ifdef _WIN32
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#endif

namespace fs = std::filesystem;

namespace Ume {

bool DiagnosticEngine::useColors_ = true;

// Cache file lines to prevent re-reading files from disk repeatedly during compilation
static std::unordered_map<std::string, std::vector<std::string>> g_fileCache;

static std::vector<std::string> getFileLines(const std::string& filename) {
    if (filename.empty()) return {};
    auto it = g_fileCache.find(filename);
    if (it != g_fileCache.end()) return it->second;

    std::ifstream file(filename);
    if (!file.is_open()) {
        // Try normalized path
        try {
            std::string canonical = fs::absolute(filename).string();
            it = g_fileCache.find(canonical);
            if (it != g_fileCache.end()) return it->second;
            std::ifstream f2(canonical);
            if (f2.is_open()) {
                std::vector<std::string> lines;
                std::string l;
                while (std::getline(f2, l)) lines.push_back(l);
                g_fileCache[filename] = lines;
                g_fileCache[canonical] = lines;
                return lines;
            }
        } catch (...) {}
        return {};
    }

    std::vector<std::string> lines;
    std::string l;
    while (std::getline(file, l)) lines.push_back(l);
    g_fileCache[filename] = lines;
    return lines;
}

std::string DiagnosticEngine::getSourceLine(const std::string& filename, int line, const std::string& fallbackSource) {
    if (line <= 0) return "";

    // 1. Try disk file first
    auto lines = getFileLines(filename);
    if (!lines.empty() && line <= static_cast<int>(lines.size())) {
        return lines[line - 1];
    }

    // 2. Try fallback source string
    if (!fallbackSource.empty()) {
        std::istringstream iss(fallbackSource);
        std::string row;
        int current = 1;
        while (std::getline(iss, row)) {
            if (current == line) return row;
            current++;
        }
    }

    return "";
}

// Convert string tabs to spaces for precise column caret alignment
static std::string expandTabs(const std::string& input, int tabSize = 4) {
    std::string res;
    for (char c : input) {
        if (c == '\t') {
            res.append(tabSize, ' ');
        } else {
            res.push_back(c);
        }
    }
    return res;
}

// Adjust column position when tabs are expanded
static int computeVisualColumn(const std::string& rawLine, int col, int tabSize = 4) {
    int visCol = 1;
    int target = std::min(col - 1, static_cast<int>(rawLine.size()));
    for (int i = 0; i < target; i++) {
        if (rawLine[i] == '\t') visCol += tabSize;
        else visCol += 1;
    }
    return visCol;
}

std::string DiagnosticEngine::format(const Diagnostic& diag, const std::string& fallbackSource) {
    std::ostringstream ss;

    // ANSI escape sequences
    const char* RED     = useColors_ ? "\033[1;31m" : "";
    const char* YELLOW  = useColors_ ? "\033[1;33m" : "";
    const char* CYAN    = useColors_ ? "\033[1;36m" : "";
    const char* BLUE    = useColors_ ? "\033[1;34m" : "";
    const char* GREEN   = useColors_ ? "\033[1;32m" : "";
    const char* BOLD    = useColors_ ? "\033[1m"    : "";
    const char* RESET   = useColors_ ? "\033[0m"    : "";

    const char* levelStr = "error";
    const char* levelColor = RED;
    if (diag.level == DiagnosticLevel::Warning) {
        levelStr = "warning";
        levelColor = YELLOW;
    } else if (diag.level == DiagnosticLevel::Info) {
        levelStr = "info";
        levelColor = CYAN;
    }

    // Header: error: <message>
    ss << levelColor << levelStr << ": " << BOLD << diag.message << RESET << "\n";

    if (diag.location.isValid()) {
        std::string fn = diag.location.filename.empty() ? "<unknown>" : diag.location.filename;
        // Normalize slashes for clean presentation
        std::replace(fn.begin(), fn.end(), '/', '\\');

        // Location header:   --> file.ume:10:28
        ss << BLUE << "  --> " << RESET << fn << ":" << diag.location.line << ":" << diag.location.column << "\n";
        ss << BLUE << "   |" << RESET << "\n";

        std::string rawLine = getSourceLine(diag.location.filename, diag.location.line, fallbackSource);
        if (!rawLine.empty()) {
            std::string expandedLine = expandTabs(rawLine);
            int visualCol = computeVisualColumn(rawLine, diag.location.column);

            int lineNum = diag.location.line;
            std::string lineNumStr = std::to_string(lineNum);
            int padWidth = std::max(2, static_cast<int>(lineNumStr.size()));

            // Code line: 10 |     population = alloc(populationSize);
            ss << BLUE << std::setw(padWidth) << lineNumStr << " | " << RESET << expandedLine << "\n";

            // Caret line:    |                       ^ label
            ss << BLUE << std::string(padWidth, ' ') << " | " << RESET;
            int indent = std::max(0, visualCol - 1);
            ss << std::string(indent, ' ') << levelColor << "^";
            if (!diag.label.empty()) {
                ss << " " << diag.label;
            }
            ss << RESET << "\n";
        }

        ss << BLUE << "   |" << RESET << "\n";
    }

    // Help section
    if (!diag.help.empty()) {
        ss << GREEN << "help: " << RESET << diag.help << "\n";
    }

    for (const auto& note : diag.notes) {
        ss << CYAN << "note: " << RESET << note.second << "\n";
    }

    return ss.str();
}

void DiagnosticEngine::print(const Diagnostic& diag, const std::string& fallbackSource) {
    std::cerr << format(diag, fallbackSource);
}

}
