#pragma once

#include "common_block.hpp"
#include <string>
#include <vector>
#include <sstream>
#include <regex>
#include <iostream>
#include <algorithm>
#include <cctype>
#include <map>

// ─────────────────────────────────────────────────────────────────────────────
// SymbolDumpParser — correct for Flang 18 output format
//
// KEY INSIGHT from actual Flang 18 output:
//   The dump has multiple nested scopes. Variable names like "x" appear
//   in MANY scopes (builtin modules, other subroutines, etc.).
//   We must ONLY collect variables from the same Subprogram scope that
//   contains the CommonBlockDetails line.
//
// Flang 18 actual format (inside "Subprogram scope: file_a_sub ..."):
//
//   n, SAVE size=4 offset=400: ObjectEntity type: INTEGER(4)
//   x, SAVE size=400 offset=0: ObjectEntity type: REAL(4) shape: 1_8:100_8
//   mydata size=404 offset=0: CommonBlockDetails alignment=4: x n
//
// Strategy:
//   - Walk line by line tracking which "Subprogram scope:" we are inside
//   - Only collect ObjectEntity and CommonBlockDetails lines from that scope
//   - A new scope begins when we see "scope:" with LESS indentation
//   - Each subprogram scope gets its own variable table
// ─────────────────────────────────────────────────────────────────────────────

class SymbolDumpParser {
public:

    // ── Utilities ─────────────────────────────────────────────────────────────

    static std::string trim(const std::string& s) {
        size_t a = s.find_first_not_of(" \t\r\n");
        size_t b = s.find_last_not_of(" \t\r\n");
        return (a == std::string::npos) ? "" : s.substr(a, b - a + 1);
    }

    static std::string toUpper(std::string s) {
        std::transform(s.begin(), s.end(), s.begin(), ::toupper);
        return s;
    }

    // Count leading spaces (used to detect scope changes)
    static int indentOf(const std::string& line) {
        int n = 0;
        for (char c : line) {
            if (c == ' ') n++;
            else break;
        }
        return n;
    }

    // Parse extent from "1_8:100_8" → 100, "3_8:6_8" → 4, "100" → 100
    static int parseExtent(const std::string& tok) {
        std::regex re_suf(R"(_\d+)");
        std::string s = std::regex_replace(trim(tok), re_suf, "");
        auto c = s.find(':');
        if (c != std::string::npos) {
            int lo = std::stoi(s.substr(0, c));
            int hi = std::stoi(s.substr(c + 1));
            return hi - lo + 1;
        }
        return std::stoi(s);
    }

    static std::vector<int> parseShape(const std::string& raw) {
        std::vector<int> dims;
        std::istringstream ss(raw);
        std::string tok;
        while (std::getline(ss, tok, ',')) {
            tok = trim(tok);
            if (tok.empty()) continue;
            try { dims.push_back(parseExtent(tok)); } catch (...) {}
        }
        return dims;
    }

    static long elemCount(const std::vector<int>& dims) {
        if (dims.empty()) return 1;
        long p = 1; for (int d : dims) p *= d; return p;
    }

    // ── Per-variable info ─────────────────────────────────────────────────────

    struct VarInfo {
        std::string      name;
        FortranType      type     = FortranType::UNKNOWN;
        int              kind     = 4;
        int              char_len = 1;
        std::vector<int> dims;
        long             size_bytes = 0;
        long             offset     = 0;
        bool             is_saved   = false;
    };

    struct BlockInfo {
        std::string              name;
        long                     total_size = 0;
        std::vector<std::string> members;
    };

    // ── Parse one ObjectEntity line into VarInfo ──────────────────────────────
    static bool parseVarLine(const std::string& line, VarInfo& out) {
        if (line.find("ObjectEntity") == std::string::npos) return false;

        // First word = variable name (skip lines starting with '.' or '__')
        std::regex re_name(R"(^\s*([A-Za-z][A-Za-z0-9_]*)\s*[,\s])");
        std::smatch m;
        if (!std::regex_search(line, m, re_name)) return false;
        std::string raw_name = m[1].str();
        if (raw_name.empty()) return false;
        out.name = toUpper(raw_name);

        // SAVE
        out.is_saved = (line.find("SAVE") != std::string::npos);

        // size=N
        { std::regex re(R"(\bsize=(\d+))");
          if (std::regex_search(line, m, re)) out.size_bytes = std::stol(m[1].str()); }

        // offset=N
        { std::regex re(R"(\boffset=(\d+))");
          if (std::regex_search(line, m, re)) out.offset = std::stol(m[1].str()); }

        // Type parsing
        {
            // CHARACTER(8_8,1)  or  CHARACTER(8,1)
            std::regex re_ca(R"(\btype:\s*CHARACTER\((\d+)_\d+,\d+\))", std::regex::icase);
            std::regex re_cb(R"(\btype:\s*CHARACTER\((\d+),\d+\))",      std::regex::icase);
            // REAL(4), INTEGER(4), LOGICAL(4), COMPLEX(4)
            std::regex re_t(R"(\btype:\s*([A-Za-z]+)\((\d+)\))",          std::regex::icase);
            // DOUBLE PRECISION (no parens)
            std::regex re_dp(R"(\btype:\s*DOUBLE\s+PRECISION\b)",          std::regex::icase);

            if (std::regex_search(line, m, re_ca)) {
                out.type = FortranType::CHARACTER;
                out.kind = 1;
                out.char_len = std::stoi(m[1].str());
            } else if (std::regex_search(line, m, re_cb)) {
                out.type = FortranType::CHARACTER;
                out.kind = 1;
                out.char_len = std::stoi(m[1].str());
            } else if (std::regex_search(line, m, re_dp)) {
                out.type = FortranType::DOUBLE_PRECISION;
                out.kind = 8;
            } else if (std::regex_search(line, m, re_t)) {
                std::string tn = toUpper(m[1].str());
                int k = std::stoi(m[2].str());
                if      (tn == "REAL")    { out.type = (k==8) ? FortranType::DOUBLE_PRECISION
                                                               : FortranType::REAL;
                                            out.kind = k; }
                else if (tn == "INTEGER") { out.type = FortranType::INTEGER; out.kind = k; }
                else if (tn == "LOGICAL") { out.type = FortranType::LOGICAL; out.kind = k; }
                else if (tn == "COMPLEX") { out.type = FortranType::COMPLEX; out.kind = k*2; }
                else                      { out.type = FortranType::UNKNOWN; out.kind = k; }
            }
        }

        // shape: 1_8:100_8  or multi-dim  1_8:3_8,1_8:4_8
        { std::regex re_sh(R"(\bshape:\s*([\d_:,\s]+))");
          if (std::regex_search(line, m, re_sh))
              out.dims = parseShape(m[1].str()); }

        return true;
    }

    // ── Parse one CommonBlockDetails line into BlockInfo ──────────────────────
    static bool parseBlockLine(const std::string& line, BlockInfo& out) {
        if (line.find("CommonBlockDetails") == std::string::npos) return false;

        // Blank COMMON: line starts with whitespace then "size=" (no name token)
        // Named COMMON: line starts with whitespace then "blockname size=..."
        std::string tl = line;
        // strip leading whitespace to check first token
        size_t first = tl.find_first_not_of(" \t");
        bool blank = (first != std::string::npos && tl.substr(first, 5) == "size=");

        if (blank) {
            out.name = "__BLANK_COMMON__";
        } else {
            std::regex re_name(R"(^\s*([A-Za-z_][\w]*)\s)");
            std::smatch mn;
            if (!std::regex_search(line, mn, re_name)) return false;
            out.name = toUpper(mn[1].str());
            if (out.name.empty() || out.name == "BLANKCOMMON" || out.name == "SIZE")
                out.name = "__BLANK_COMMON__";
        }

        { std::regex re(R"(\bsize=(\d+))");
          std::smatch m;
          if (std::regex_search(line, m, re)) out.total_size = std::stol(m[1].str()); }

        // Members after last ':'
        size_t pos = line.find("CommonBlockDetails");
        if (pos != std::string::npos) {
            size_t colon = line.find(':', pos);
            if (colon != std::string::npos) {
                std::istringstream ss(line.substr(colon + 1));
                std::string tok;
                while (ss >> tok)
                    out.members.push_back(toUpper(tok));
            }
        }
        return true;
    }

    // ── Main parse ────────────────────────────────────────────────────────────
    //
    // We walk the dump line by line, tracking which Subprogram scope we are in.
    // We detect scope boundaries by looking for lines that start a new scope
    // at a lower (or equal) indentation than where we entered our scope.
    //
    // Each Subprogram scope gets its own local variable table.
    // CommonBlockDetails lines are immediately joined with that local table.
    //
    std::vector<CommonBlockDef> parse(const std::string& dump_output,
                                      const std::string& source_file) {

        std::vector<CommonBlockDef> result;

        std::vector<std::string> lines;
        {
            std::istringstream ss(dump_output);
            std::string l;
            while (std::getline(ss, l)) lines.push_back(l);
        }

        int n = (int)lines.size();
        int i = 0;

        while (i < n) {
            const std::string& line = lines[i];

            // Detect start of a Subprogram scope:
            // "  Subprogram scope: funcname size=..."
            if (line.find("Subprogram scope:") != std::string::npos) {
                int scope_indent = indentOf(line);  // indentation of the scope header
                int body_indent  = scope_indent + 2; // expected indentation of body lines

                // Local variable table for THIS scope only
                std::map<std::string, VarInfo> local_vars;
                // Blocks found in THIS scope
                std::vector<BlockInfo> local_blocks;

                i++; // move past the scope header line

                // Read lines that belong to this scope
                // A line belongs if its indentation >= body_indent
                // OR if it is an empty line (continuation)
                while (i < n) {
                    const std::string& bline = lines[i];

                    // Empty line: skip, stay in scope
                    if (trim(bline).empty()) { i++; continue; }

                    int bindent = indentOf(bline);

                    // If indentation drops back to scope level or less → scope ended
                    if (bindent <= scope_indent) break;

                    // Skip compiler-internal symbols
                    std::string tl = trim(bline);
                    if (!tl.empty() && tl[0] == '.') { i++; continue; }
                    if (bline.find("CompilerCreated") != std::string::npos) { i++; continue; }

                    // Parse CommonBlockDetails
                    if (bline.find("CommonBlockDetails") != std::string::npos) {
                        BlockInfo bi;
                        if (parseBlockLine(bline, bi) && !bi.name.empty())
                            local_blocks.push_back(bi);
                        i++; continue;
                    }

                    // Parse ObjectEntity (variable)
                    if (bline.find("ObjectEntity") != std::string::npos) {
                        VarInfo vi;
                        if (parseVarLine(bline, vi) && !vi.name.empty()) {
                            // First occurrence wins (declaration order preserved)
                            if (local_vars.find(vi.name) == local_vars.end())
                                local_vars[vi.name] = vi;
                        }
                        i++; continue;
                    }

                    i++;
                }

                // ── Join local_blocks with local_vars ─────────────────────────
                for (auto& bi : local_blocks) {
                    CommonBlockDef def;
                    def.name        = bi.name;
                    def.source_file = source_file;
                    def.line_number = -1;
                    def.is_saved    = false;
                    def.total_size  = 0;

                    for (const auto& mname : bi.members) {
                        auto it = local_vars.find(mname);
                        if (it == local_vars.end()) {
                            std::cerr << "[Parser] Warning: member '" << mname
                                      << "' of /" << bi.name << "/ not found in local scope.\n";
                            continue;
                        }
                        const VarInfo& vi = it->second;
                        Variable var;
                        var.name       = vi.name;
                        var.type       = vi.type;
                        var.kind       = vi.kind;
                        var.char_len   = vi.char_len;
                        var.dimensions = vi.dims;
                        var.size_bytes = vi.size_bytes;
                        var.offset     = vi.offset;
                        def.variables.push_back(var);
                        def.total_size += var.size_bytes;
                        if (vi.is_saved) def.is_saved = true;
                    }

                    // Sanity check with Flang's reported size
                    if (bi.total_size > 0 && bi.total_size != def.total_size) {
                        std::cerr << "[Parser] /" << bi.name << "/ size: Flang="
                                  << bi.total_size << " computed=" << def.total_size
                                  << ". Using Flang value.\n";
                        def.total_size = bi.total_size;
                    }

                    result.push_back(def);
                }

                continue; // outer while already advanced i
            }

            i++;
        }

        return result;
    }
};
