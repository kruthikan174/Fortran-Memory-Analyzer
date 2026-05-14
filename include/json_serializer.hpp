#pragma once

#include "common_block.hpp"
#include <string>
#include <sstream>

// ─────────────────────────────────────────────────────────────────────────────
// Minimal JSON serializer (no external lib needed - we write it by hand)
// Produces the exact schema that Person 2 (validator.cpp) will consume
// ─────────────────────────────────────────────────────────────────────────────

class JsonSerializer {
public:

    // Escape special characters in a string for safe JSON embedding
    static std::string escapeString(const std::string& s) {
        std::string out;
        out.reserve(s.size() + 4);
        for (char c : s) {
            switch (c) {
                case '"':  out += "\\\""; break;
                case '\\': out += "\\\\"; break;
                case '\n': out += "\\n";  break;
                case '\r': out += "\\r";  break;
                case '\t': out += "\\t";  break;
                default:   out += c;
            }
        }
        return out;
    }

    // Serialize a single Variable struct → JSON object string
    static std::string serializeVariable(const Variable& v, const std::string& indent) {
        std::ostringstream o;
        o << indent << "{\n";
        o << indent << "  \"name\": \""      << escapeString(v.name)         << "\",\n";
        o << indent << "  \"type\": \""      << typeToString(v.type)         << "\",\n";
        o << indent << "  \"kind\": "        << v.kind                       << ",\n";

        // dimensions array
        o << indent << "  \"dimensions\": [";
        for (size_t i = 0; i < v.dimensions.size(); i++) {
            o << v.dimensions[i];
            if (i + 1 < v.dimensions.size()) o << ", ";
        }
        o << "],\n";

        o << indent << "  \"size_bytes\": "  << v.size_bytes                 << ",\n";
        o << indent << "  \"offset\": "      << v.offset                     << ",\n";
        o << indent << "  \"char_len\": "    << v.char_len                   << "\n";
        o << indent << "}";
        return o.str();
    }

    // Serialize a single CommonBlockDef struct → JSON object string
    static std::string serializeBlock(const CommonBlockDef& b, const std::string& indent) {
        std::ostringstream o;
        o << indent << "{\n";
        o << indent << "  \"name\": \""        << escapeString(b.name)         << "\",\n";
        o << indent << "  \"total_size\": "    << b.total_size                 << ",\n";
        o << indent << "  \"saved\": "         << (b.is_saved ? "true" : "false") << ",\n";
        o << indent << "  \"source_file\": \"" << escapeString(b.source_file)  << "\",\n";
        o << indent << "  \"line_number\": "   << b.line_number                << ",\n";

        // variables array
        o << indent << "  \"variables\": [\n";
        for (size_t i = 0; i < b.variables.size(); i++) {
            o << serializeVariable(b.variables[i], indent + "    ");
            if (i + 1 < b.variables.size()) o << ",";
            o << "\n";
        }
        o << indent << "  ]\n";

        o << indent << "}";
        return o.str();
    }

    // Serialize a complete TranslationUnit → full JSON document string
    static std::string serializeTranslationUnit(const TranslationUnit& tu) {
        std::ostringstream o;
        o << "{\n";
        o << "  \"file\": \"" << escapeString(tu.file_path) << "\",\n";
        o << "  \"common_blocks\": [\n";

        for (size_t i = 0; i < tu.common_blocks.size(); i++) {
            o << serializeBlock(tu.common_blocks[i], "    ");
            if (i + 1 < tu.common_blocks.size()) o << ",";
            o << "\n";
        }

        o << "  ]\n";
        o << "}\n";
        return o.str();
    }
};
