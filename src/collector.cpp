// ─────────────────────────────────────────────────────────────────────────────
// collector.cpp  —  Part 1 of the Fortran COMMON Block Memory Safety Analyzer
//
// PURPOSE:
//   Processes one Fortran source file and emits a JSON file describing the
//   layout of every COMMON block found in it.
//
// USAGE:
//   ./collector <fortran_file.f90> [output.json]
//
//   If output file is omitted, JSON is written to stdout.
//   Set FLANG_PATH env var to override the path to flang-new.
//
// OUTPUT (one JSON per input file):
//   {
//     "file": "...",
//     "common_blocks": [ ... ],
//     "equivalences": [ ... ]
//   }
//
// COMPILE:
//   g++ -std=c++17 -O2 -Iinclude -o collector src/collector.cpp
// ─────────────────────────────────────────────────────────────────────────────

#include "common_block.hpp"
#include "flang_runner.hpp"
#include "symbol_dump_parser.hpp"
#include "equivalence_resolver.hpp"
#include "json_serializer.hpp"

#include <iostream>
#include <fstream>
#include <sstream>
#include <string>
#include <filesystem>

namespace fs = std::filesystem;

// ── Read entire file into a string ───────────────────────────────────────────
static std::string readFile(const std::string& path) {
    std::ifstream f(path);
    if (!f.is_open())
        throw std::runtime_error("Cannot open file: " + path);
    std::ostringstream ss;
    ss << f.rdbuf();
    return ss.str();
}

// ── Build the final JSON including equivalences ───────────────────────────────
static std::string buildFullJson(
    const TranslationUnit& tu,
    const std::vector<EquivalencePair>& equivs)
{
    std::ostringstream o;
    o << "{\n";
    o << "  \"file\": \"" << JsonSerializer::escapeString(tu.file_path) << "\",\n";
    o << "  \"common_blocks\": [\n";

    for (size_t i = 0; i < tu.common_blocks.size(); i++) {
        o << JsonSerializer::serializeBlock(tu.common_blocks[i], "    ");
        if (i + 1 < tu.common_blocks.size()) o << ",";
        o << "\n";
    }

    o << "  ],\n";
    o << "  \"equivalences\": " << EquivalenceResolver::toJson(equivs) << "\n";
    o << "}\n";
    return o.str();
}

// ── Main ──────────────────────────────────────────────────────────────────────
int main(int argc, char* argv[]) {

    // ── Argument validation ──────────────────────────────────────────────────
    if (argc < 2) {
        std::cerr
            << "Usage:   ./collector <fortran_file.f90> [output.json]\n"
            << "\n"
            << "Options:\n"
            << "  FLANG_PATH env var  Override path to flang-new binary\n"
            << "\n"
            << "Example:\n"
            << "  ./collector src/physics.f90 outputs/physics.json\n"
            << "  FLANG_PATH=/usr/bin/flang-18 ./collector myfile.f90\n";
        return 1;
    }

    std::string input_file  = argv[1];
    std::string output_file = (argc >= 3) ? argv[2] : "";

    // Check the input file exists
    if (!fs::exists(input_file)) {
        std::cerr << "[collector] Error: input file not found: " << input_file << "\n";
        return 1;
    }

    // ── Step 1: Run Flang to get the symbol dump ─────────────────────────────
    FlangRunner runner;
    runner.applyEnvOverride();

    if (!runner.isAvailable()) {
        std::cerr
            << "[collector] ERROR: flang-new not found in PATH.\n"
            << "  Install:  sudo apt install flang-18\n"
            << "  Or set:   export FLANG_PATH=/path/to/flang-new\n";
        return 1;
    }

    std::string dump_output;
    try {
        dump_output = runner.runDump(input_file, DumpMode::SYMBOLS);
    } catch (const std::exception& e) {
        std::cerr << "[collector] Flang invocation failed: " << e.what() << "\n";
        return 1;
    }

    std::cerr << "[collector] Received " << dump_output.size()
              << " bytes from Flang dump.\n";

    // ── Step 2: Parse the symbol dump ────────────────────────────────────────
    SymbolDumpParser parser;
    std::vector<CommonBlockDef> blocks;

    try {
        blocks = parser.parse(dump_output, input_file);
    } catch (const std::exception& e) {
        std::cerr << "[collector] Parser error: " << e.what() << "\n";
        return 1;
    }

    std::cerr << "[collector] Found " << blocks.size() << " COMMON block(s).\n";
    for (const auto& b : blocks) {
        std::cerr << "  //" << b.name << "//  size=" << b.total_size
                  << " bytes,  " << b.variables.size() << " variable(s)"
                  << (b.is_saved ? "  [SAVED]" : "") << "\n";
    }

    // ── Step 3: Parse EQUIVALENCE from source ────────────────────────────────
    std::string source_text;
    try {
        source_text = readFile(input_file);
    } catch (...) {
        std::cerr << "[collector] Warning: could not re-read source for EQUIVALENCE analysis.\n";
    }

    std::vector<EquivalencePair> equivs;
    if (!source_text.empty()) {
        equivs = EquivalenceResolver::parseFromSource(source_text);
        EquivalenceResolver::resolve(equivs, blocks);

        if (!equivs.empty()) {
            std::cerr << "[collector] Found " << equivs.size() << " EQUIVALENCE pair(s):\n";
            for (const auto& ep : equivs) {
                std::cerr << "  EQUIVALENCE (" << ep.var_a << "(" << ep.index_a
                          << "), " << ep.var_b << "(" << ep.index_b << "))";
                if (ep.involves_common) {
                    std::cerr << "  → involves COMMON /" << ep.common_block_name << "/";
                    if (ep.causes_extension)
                        std::cerr << "  ⚠ CAUSES BACKWARD EXTENSION";
                }
                std::cerr << "\n";
            }
        }
    }

    // ── Step 4: Build TranslationUnit and emit JSON ───────────────────────────
    TranslationUnit tu;
    tu.file_path     = input_file;
    tu.common_blocks = blocks;

    std::string json_output = buildFullJson(tu, equivs);

    // Write to file or stdout
    if (output_file.empty()) {
        std::cout << json_output;
    } else {
        // Create output directory if it doesn't exist
        fs::path out_path(output_file);
        if (out_path.has_parent_path()) {
            fs::create_directories(out_path.parent_path());
        }

        std::ofstream out(output_file);
        if (!out.is_open()) {
            std::cerr << "[collector] Error: cannot write to: " << output_file << "\n";
            return 1;
        }
        out << json_output;
        out.close();

        std::cerr << "[collector] Written to: " << output_file << "\n";
    }

    return 0;
}
