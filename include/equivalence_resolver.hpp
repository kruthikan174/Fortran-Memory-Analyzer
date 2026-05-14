#pragma once

#include "common_block.hpp"
#include <string>
#include <vector>
#include <map>
#include <regex>
#include <sstream>
#include <iostream>

// ─────────────────────────────────────────────────────────────────────────────
// EquivalenceResolver
//
// EQUIVALENCE is the hardest part of Part 1.
//
// The EQUIVALENCE statement lets two variables alias the same memory:
//   EQUIVALENCE (A, B)   → A and B start at the same byte address
//
// When one of these variables is in a COMMON block, this can SHIFT the
// starting offsets of other variables in that block, or even EXTEND the
// block backwards.
//
// Example:
//   REAL    :: A(10)         ! local, 40 bytes
//   REAL    :: X(100)
//   COMMON  /BLK/ X          ! X is at offset 0 in BLK
//   EQUIVALENCE (X(1), A(5)) ! A(5) overlaps X(1)
//                            ! So A(1) is at offset -16 (before BLK start!)
//                            ! BLK now effectively starts earlier
//
// This is a VIOLATION and must be flagged.
//
// For our prototype, we:
//   1. Parse EQUIVALENCE statements from the raw source (not the dump)
//   2. Detect which pairs involve a COMMON block variable
//   3. Flag any offset shifts or backward extensions as warnings
//   4. Record the equivalence pairs in the JSON for Person 2 to analyze
// ─────────────────────────────────────────────────────────────────────────────

struct EquivalencePair {
    std::string var_a;         // name of first variable
    std::string var_b;         // name of second variable
    int         index_a = 1;  // array index (1-based), 1 for scalars
    int         index_b = 1;
    bool        involves_common = false;  // true if either var is in a COMMON block
    std::string common_block_name;        // which block is involved
    bool        causes_extension = false; // true if the block would extend backwards
    long        offset_shift = 0;         // how many bytes the start shifts
};

class EquivalenceResolver {
public:

    // Parse EQUIVALENCE statements directly from Fortran source text.
    // We do this on the raw source because Flang's dump may not show them clearly.
    static std::vector<EquivalencePair> parseFromSource(const std::string& source_text) {
        std::vector<EquivalencePair> pairs;

        // Regex: EQUIVALENCE (A, B) or EQUIVALENCE (A(n), B(m))
        // Fortran is case-insensitive; handle both EQUIVALENCE and equivalence
        std::regex re_equiv(
            R"(EQUIVALENCE\s*\(\s*(\w+)(?:\((\d+)\))?\s*,\s*(\w+)(?:\((\d+)\))?\s*\))",
            std::regex::icase
        );

        std::string text = source_text;
        // Remove Fortran line continuations (&)
        std::regex re_cont(R"(&\s*\n\s*)");
        text = std::regex_replace(text, re_cont, " ");

        // Remove comments (everything from ! to end of line)
        std::regex re_comment(R"(!.*$)", std::regex::multiline);
        text = std::regex_replace(text, re_comment, "");

        auto begin = std::sregex_iterator(text.begin(), text.end(), re_equiv);
        auto end   = std::sregex_iterator();

        for (auto it = begin; it != end; ++it) {
            EquivalencePair ep;
            ep.var_a   = (*it)[1].str();
            ep.index_a = (*it)[2].matched ? std::stoi((*it)[2].str()) : 1;
            ep.var_b   = (*it)[3].str();
            ep.index_b = (*it)[4].matched ? std::stoi((*it)[4].str()) : 1;
            pairs.push_back(ep);
        }

        return pairs;
    }

    // Cross-reference parsed EQUIVALENCE pairs against extracted COMMON blocks.
    // Marks pairs that involve COMMON block variables and computes offset shifts.
    static void resolve(std::vector<EquivalencePair>& pairs,
                        std::vector<CommonBlockDef>&  blocks) {

        // Build a lookup: variable_name → (block_name, variable offset, variable size)
        struct VarInfo { std::string block; long offset; long size; int kind; };
        std::map<std::string, VarInfo> var_map;

        for (auto& blk : blocks) {
            for (auto& var : blk.variables) {
                var_map[var.name] = { blk.name, var.offset, var.size_bytes, var.kind };
            }
        }

        for (auto& ep : pairs) {
            bool a_in_common = var_map.count(ep.var_a) > 0;
            bool b_in_common = var_map.count(ep.var_b) > 0;

            if (a_in_common || b_in_common) {
                ep.involves_common = true;

                // Determine which one is the COMMON variable and which is local
                if (a_in_common) {
                    auto& vi = var_map[ep.var_a];
                    ep.common_block_name = vi.block;

                    // Byte position of A(index_a) = A.offset + (index_a-1)*kind
                    long a_byte = vi.offset + (long)(ep.index_a - 1) * vi.kind;

                    // B(index_b) must coincide with a_byte
                    // If index_b > 1, then B(1) is at: a_byte - (index_b-1)*kind_B
                    // We don't know kind_B without the symbol table, so flag it
                    if (ep.index_b > 1) {
                        ep.offset_shift = -(long)(ep.index_b - 1) * 4; // estimate
                        ep.causes_extension = (a_byte - ep.offset_shift < 0);
                    }

                } else { // b_in_common
                    auto& vi = var_map[ep.var_b];
                    ep.common_block_name = vi.block;
                    long b_byte = vi.offset + (long)(ep.index_b - 1) * vi.kind;

                    if (ep.index_a > 1) {
                        ep.offset_shift = -(long)(ep.index_a - 1) * 4;
                        ep.causes_extension = (b_byte - ep.offset_shift < 0);
                    }
                }
            }
        }
    }

    // Serialize equivalence pairs into a JSON array string (for embedding in output)
    static std::string toJson(const std::vector<EquivalencePair>& pairs) {
        if (pairs.empty()) return "[]";
        std::ostringstream o;
        o << "[\n";
        for (size_t i = 0; i < pairs.size(); i++) {
            const auto& ep = pairs[i];
            o << "    {\n";
            o << "      \"var_a\": \""           << ep.var_a              << "\",\n";
            o << "      \"index_a\": "            << ep.index_a            << ",\n";
            o << "      \"var_b\": \""            << ep.var_b              << "\",\n";
            o << "      \"index_b\": "            << ep.index_b            << ",\n";
            o << "      \"involves_common\": "    << (ep.involves_common ? "true" : "false") << ",\n";
            o << "      \"common_block\": \""     << ep.common_block_name  << "\",\n";
            o << "      \"causes_extension\": "  << (ep.causes_extension ? "true" : "false") << ",\n";
            o << "      \"offset_shift\": "       << ep.offset_shift        << "\n";
            o << "    }";
            if (i + 1 < pairs.size()) o << ",";
            o << "\n";
        }
        o << "  ]";
        return o.str();
    }
};
