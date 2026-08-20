#pragma once
// diagnostics.h — Rich Diagnostic Reporting Engine for Ume Language

#include <string>
#include <vector>
#include <memory>
#include <iostream>

namespace Ume {

enum class DiagnosticLevel {
    Error,
    Warning,
    Info
};

struct SourceLocation {
    std::string filename;
    int line   = 0;
    int column = 0;

    bool isValid() const { return line > 0; }
};

struct Diagnostic {
    DiagnosticLevel level = DiagnosticLevel::Error;
    std::string message;
    SourceLocation location;
    std::string label; // Annotation next to caret
    std::string help;  // Helpful hint/suggestion at bottom
    std::vector<std::pair<SourceLocation, std::string>> notes;
};

class DiagnosticEngine {
public:
    static std::string format(const Diagnostic& diag, const std::string& fallbackSource = "");
    static void print(const Diagnostic& diag, const std::string& fallbackSource = "");

    // Utility to read a specific line from a file or fallback source
    static std::string getSourceLine(const std::string& filename, int line, const std::string& fallbackSource = "");
    
    static void setUseColors(bool enable) { useColors_ = enable; }
    static bool useColors() { return useColors_; }

private:
    static bool useColors_;
};

}
