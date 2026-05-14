#pragma once

#include <string>
#include <vector>
#include <map>
#include <stdexcept>

// ─────────────────────────────────────────────────────────────────────────────
// Data structures representing the layout of one Fortran COMMON block
// These are filled by the Collector and consumed by Person 2 (validator)
// ─────────────────────────────────────────────────────────────────────────────

// Fortran base types we recognize
enum class FortranType {
    REAL,
    DOUBLE_PRECISION,
    INTEGER,
    LOGICAL,
    CHARACTER,
    COMPLEX,
    UNKNOWN
};

// Convert type enum → readable string (used in JSON output)
inline std::string typeToString(FortranType t) {
    switch (t) {
        case FortranType::REAL:             return "REAL";
        case FortranType::DOUBLE_PRECISION: return "DOUBLE_PRECISION";
        case FortranType::INTEGER:          return "INTEGER";
        case FortranType::LOGICAL:          return "LOGICAL";
        case FortranType::CHARACTER:        return "CHARACTER";
        case FortranType::COMPLEX:          return "COMPLEX";
        default:                            return "UNKNOWN";
    }
}

// Parse type string from Flang output → enum
inline FortranType parseType(const std::string& s) {
    if (s.find("REAL")             != std::string::npos) return FortranType::REAL;
    if (s.find("DOUBLE")           != std::string::npos) return FortranType::DOUBLE_PRECISION;
    if (s.find("INTEGER")          != std::string::npos) return FortranType::INTEGER;
    if (s.find("LOGICAL")          != std::string::npos) return FortranType::LOGICAL;
    if (s.find("CHARACTER")        != std::string::npos) return FortranType::CHARACTER;
    if (s.find("COMPLEX")          != std::string::npos) return FortranType::COMPLEX;
    return FortranType::UNKNOWN;
}

// One variable inside a COMMON block
struct Variable {
    std::string          name;          // e.g. "X"
    FortranType          type;          // e.g. REAL
    int                  kind;          // byte-width of base type (4 or 8 usually)
    std::vector<int>     dimensions;    // empty = scalar; [100] = 1-D array of 100; [3,4] = 3×4
    long                 size_bytes;    // total bytes: kind * product(dimensions)
    long                 offset;        // byte offset from start of COMMON block
    int                  char_len;      // only for CHARACTER type (length in chars), else 1
};

// One COMMON block definition found in a single file
struct CommonBlockDef {
    std::string           name;         // block name, or "__BLANK_COMMON__" if unnamed
    long                  total_size;   // sum of all variable sizes in bytes
    bool                  is_saved;     // true if SAVE attribute is present
    std::vector<Variable> variables;
    std::string           source_file;  // which .f/.f90 file this came from
    int                   line_number;  // line in source where COMMON declaration appears (-1 if unknown)
};

// All COMMON blocks found in one translation unit (one source file)
struct TranslationUnit {
    std::string                  file_path;
    std::vector<CommonBlockDef>  common_blocks;
};
