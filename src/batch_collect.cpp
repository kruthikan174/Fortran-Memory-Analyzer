// ─────────────────────────────────────────────────────────────────────────────
// batch_collect.cpp  —  Run the collector over an entire directory of Fortran files
//
// USAGE:
//   ./batch_collect <input_dir> <output_dir>
//
// Example:
//   ./batch_collect ./fortran_project/ ./outputs/
//
// This is what Person 3's CLI (main.cpp) will call internally.
// It walks the input directory, finds all .f/.f90/.F/.F90 files,
// and runs the collector logic on each one.
// ─────────────────────────────────────────────────────────────────────────────

#include "common_block.hpp"
#include "flang_runner.hpp"
#include "symbol_dump_parser.hpp"
#include "equivalence_resolver.hpp"
#include "json_serializer.hpp"

#include <iostream>
#include <fstream>
#include <sstream>
#include <filesystem>
#include <vector>
#include <string>
#include <algorithm>

namespace fs = std::filesystem;

static std::string readFile(const std::string& path) {
    std::ifstream f(path);
    if (!f.is_open()) return "";
    std::ostringstream ss;
    ss << f.rdbuf();
    return ss.str();
}

static bool isFortranFile(const fs::path& p) {
    std::string ext = p.extension().string();
    std::transform(ext.begin(), ext.end(), ext.begin(), ::tolower);
    return ext == ".f" || ext == ".f90" || ext == ".f77" || ext == ".for";
}

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

int main(int argc, char* argv[]) {
    if (argc < 3) {
        std::cerr
            << "Usage: ./batch_collect <input_dir> <output_dir>\n"
            << "Example: ./batch_collect ./project/ ./outputs/\n";
        return 1;
    }

    std::string input_dir  = argv[1];
    std::string output_dir = argv[2];

    if (!fs::exists(input_dir)) {
        std::cerr << "[batch_collect] Input directory not found: " << input_dir << "\n";
        return 1;
    }

    fs::create_directories(output_dir);

    FlangRunner runner;
    runner.applyEnvOverride();

    if (!runner.isAvailable()) {
        std::cerr << "[batch_collect] flang-new not found. Set FLANG_PATH or install flang.\n";
        return 1;
    }

    // Collect all Fortran files
    std::vector<fs::path> fortran_files;
    for (auto& entry : fs::recursive_directory_iterator(input_dir)) {
        if (entry.is_regular_file() && isFortranFile(entry.path())) {
            fortran_files.push_back(entry.path());
        }
    }

    std::sort(fortran_files.begin(), fortran_files.end());
    std::cerr << "[batch_collect] Found " << fortran_files.size() << " Fortran file(s).\n\n";

    int success = 0, failure = 0;

    for (const auto& fpath : fortran_files) {
        std::string file_str = fpath.string();
        std::cerr << "[batch_collect] Processing: " << file_str << "\n";

        try {
            // Run Flang dump
            std::string dump = runner.runDump(file_str);

            // Parse symbols
            SymbolDumpParser parser;
            auto blocks = parser.parse(dump, file_str);

            // Parse EQUIVALENCE from source
            std::string source = readFile(file_str);
            auto equivs = EquivalenceResolver::parseFromSource(source);
            EquivalenceResolver::resolve(equivs, blocks);

            // Build TU
            TranslationUnit tu;
            tu.file_path     = file_str;
            tu.common_blocks = blocks;

            // Determine output JSON path
            // e.g. input_dir/subdir/file.f90 → output_dir/subdir/file.json
            fs::path rel = fs::relative(fpath, input_dir);
            fs::path out_path = fs::path(output_dir) / rel;
            out_path.replace_extension(".json");

            fs::create_directories(out_path.parent_path());

            std::ofstream out(out_path.string());
            if (!out.is_open()) {
                std::cerr << "  [ERROR] Cannot write: " << out_path << "\n";
                failure++;
                continue;
            }
            out << buildFullJson(tu, equivs);
            out.close();

            std::cerr << "  → Wrote: " << out_path
                      << "  (" << blocks.size() << " blocks)\n";
            success++;

        } catch (const std::exception& e) {
            std::cerr << "  [ERROR] " << e.what() << "\n";
            failure++;
        }
    }

    std::cerr << "\n[batch_collect] Done. " << success << " succeeded, "
              << failure << " failed.\n";

    return (failure > 0) ? 1 : 0;
}
