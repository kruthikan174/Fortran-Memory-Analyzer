#pragma once

#include <string>
#include <stdexcept>
#include <cstdio>
#include <cstdlib>
#include <array>
#include <sstream>
#include <iostream>

// ─────────────────────────────────────────────────────────────────────────────
// FlangRunner: invokes flang-new as a subprocess and captures its symbol dump.
//
// Two dump modes supported:
//   - SYMBOLS:    flang-new -fc1 -fdebug-dump-symbols <file>
//   - PARSE_TREE: flang-new -fc1 -fdebug-dump-parse-tree <file>  (for debugging)
//
// The output is returned as a plain std::string for the parser to process.
// ─────────────────────────────────────────────────────────────────────────────

enum class DumpMode {
    SYMBOLS,
    PARSE_TREE
};

class FlangRunner {
public:

    // Path to the flang-new binary (can be overridden if not in PATH)
    std::string flang_binary = "flang-new";

    // Run flang on a given file and return the dump output as a string.
    // Throws std::runtime_error if flang is not found or returns non-zero.
    std::string runDump(const std::string& fortran_file, DumpMode mode = DumpMode::SYMBOLS) {

        // Build the command string
        std::string flag = (mode == DumpMode::SYMBOLS)
            ? "-fdebug-dump-symbols"
            : "-fdebug-dump-parse-tree";

        // Use 2>&1 to capture both stdout and stderr from flang
        std::string cmd = flang_binary
            + " -fc1 " + flag
            + " \"" + fortran_file + "\""
            + " 2>&1";

        std::cerr << "[FlangRunner] Running: " << cmd << "\n";

        // Open pipe to capture output
        FILE* pipe = popen(cmd.c_str(), "r");
        if (!pipe) {
            throw std::runtime_error(
                "FlangRunner: failed to invoke flang-new. Is it installed and in PATH?\n"
                "  Install: sudo apt install flang-18  (or your distro's equivalent)\n"
                "  Or set FLANG_PATH env variable."
            );
        }

        // Read all output
        std::ostringstream output;
        std::array<char, 4096> buf;
        while (fgets(buf.data(), buf.size(), pipe)) {
            output << buf.data();
        }

        int ret = pclose(pipe);
        if (ret != 0) {
            // Flang may emit errors on bad input — we still return the output
            // since partial dumps can still be useful; the parser handles errors.
            std::cerr << "[FlangRunner] Warning: flang-new exited with code "
                      << ret << " for file: " << fortran_file << "\n";
        }

        return output.str();
    }

    // Check whether flang-new is actually available on this machine.
    bool isAvailable() {
        std::string cmd = flang_binary + " --version > /dev/null 2>&1";
        return (system(cmd.c_str()) == 0);
    }

    // Allow overriding the binary path via environment variable FLANG_PATH.
    // Also auto-detects common LLVM installation paths.
    void applyEnvOverride() {
        const char* env = getenv("FLANG_PATH");
        if (env) {
            flang_binary = std::string(env);
            std::cerr << "[FlangRunner] Using FLANG_PATH override: " << flang_binary << "\n";
            return;
        }
        // Auto-detect: try common paths used by apt-installed LLVM
        static const char* candidates[] = {
            "flang-new",
            "/usr/lib/llvm-18/bin/flang-new",
            "/usr/lib/llvm-17/bin/flang-new",
            "/usr/lib/llvm-16/bin/flang-new",
            "/usr/bin/flang-new",
            "/usr/local/bin/flang-new",
            nullptr
        };
        for (int i = 0; candidates[i]; i++) {
            std::string cmd = std::string(candidates[i]) + " --version > /dev/null 2>&1";
            if (system(cmd.c_str()) == 0) {
                flang_binary = candidates[i];
                if (i > 0) // only log if not the default
                    std::cerr << "[FlangRunner] Auto-detected flang-new at: " << flang_binary << "\n";
                return;
            }
        }
    }
};
