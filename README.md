# Part 1 — Fortran COMMON Block Data Extraction (C++)

## What This Is

You are **Person 1** in the 3-person team. Your job is to build the **Collector** — the tool that reads Fortran source files and emits structured JSON describing the memory layout of every `COMMON` block found in each file.

Person 2 (validator) and Person 3 (CLI + tests) are entirely dependent on your JSON output being accurate and schema-consistent. **You are the data source of the whole pipeline.**

---

## Architecture

```
Fortran .f/.f90 file
        │
        ▼
  FlangRunner          → runs: flang-new -fc1 -fdebug-dump-symbols file.f90
        │
        ▼
  SymbolDumpParser     → parses the text output into CommonBlockDef structs
        │
        ▼
  EquivalenceResolver  → parses EQUIVALENCE statements, computes offset shifts
        │
        ▼
  JsonSerializer       → emits the final JSON file
```

### Header files (include/)
| File | Purpose |
|---|---|
| `common_block.hpp` | Core data structures: `Variable`, `CommonBlockDef`, `TranslationUnit` |
| `flang_runner.hpp` | Subprocess wrapper around `flang-new` |
| `symbol_dump_parser.hpp` | Parses Flang's `-fdebug-dump-symbols` text output |
| `equivalence_resolver.hpp` | Handles `EQUIVALENCE` offset shifts |
| `json_serializer.hpp` | Converts structs → JSON strings (no external deps) |

### Source files (src/)
| File | Purpose |
|---|---|
| `collector.cpp` | `main()` — processes one file, writes one JSON |
| `batch_collect.cpp` | Walks a directory, runs collector on every Fortran file |

---

## Requirements

### System
- Linux or macOS (WSL2 works on Windows)
- g++ or clang++ with C++17 support
- CMake 3.15+ (optional — `build.sh` works without it)
- **flang-new** (LLVM Flang frontend)

### Install Flang

**Ubuntu/Debian:**
```bash
sudo apt install flang-18
# or
sudo apt install flang-new
```

**macOS (Homebrew):**
```bash
brew install llvm
export PATH="$(brew --prefix llvm)/bin:$PATH"
```

**Verify:**
```bash
flang-new --version
```

If flang-new is at a non-standard path, use:
```bash
export FLANG_PATH=/path/to/flang-new
```

---

## Build

### With build.sh (recommended, no cmake needed)
```bash
chmod +x build.sh
./build.sh          # release
./build.sh debug    # with -g for debugging
./build.sh clean    # remove build/
```

### With CMake
```bash
mkdir build && cd build
cmake .. -DCMAKE_BUILD_TYPE=Release
make -j$(nproc)
```

### Manually
```bash
g++ -std=c++17 -O2 -Iinclude -o build/collector src/collector.cpp
g++ -std=c++17 -O2 -Iinclude -o build/batch_collect src/batch_collect.cpp
```

---

## Usage

### Single file
```bash
./build/collector path/to/file.f90 outputs/file.json
```

### Multiple files (batch mode)
```bash
./build/batch_collect path/to/fortran_project/ outputs/
```

### Custom flang path
```bash
FLANG_PATH=/usr/bin/flang-18 ./build/collector file.f90
```

---

## JSON Output Schema

Every output file follows this exact schema (Person 2 and 3 depend on it — **do not change field names**):

```json
{
  "file": "path/to/file.f90",
  "common_blocks": [
    {
      "name": "MYDATA",
      "total_size": 404,
      "saved": true,
      "source_file": "path/to/file.f90",
      "line_number": -1,
      "variables": [
        {
          "name": "X",
          "type": "REAL",
          "kind": 4,
          "dimensions": [100],
          "size_bytes": 400,
          "offset": 0,
          "char_len": 1
        },
        {
          "name": "N",
          "type": "INTEGER",
          "kind": 4,
          "dimensions": [],
          "size_bytes": 4,
          "offset": 400,
          "char_len": 1
        }
      ]
    }
  ],
  "equivalences": [
    {
      "var_a": "X",
      "index_a": 1,
      "var_b": "A",
      "index_b": 5,
      "involves_common": true,
      "common_block": "MYDATA",
      "causes_extension": false,
      "offset_shift": 0
    }
  ]
}
```

### Field reference
| Field | Type | Description |
|---|---|---|
| `name` | string | COMMON block name. `"__BLANK_COMMON__"` for unnamed blocks |
| `total_size` | int | Total bytes in this block (sum of all variable sizes) |
| `saved` | bool | Whether SAVE attribute was found for this block |
| `type` | string | One of: `REAL`, `DOUBLE_PRECISION`, `INTEGER`, `LOGICAL`, `CHARACTER`, `COMPLEX` |
| `kind` | int | Byte-width of the base type (e.g. 4 for REAL, 8 for REAL*8) |
| `dimensions` | int[] | Empty for scalars; `[100]` for 1D; `[3,4]` for 3×4 array |
| `size_bytes` | int | Total bytes for this variable: `kind × product(dimensions)` |
| `offset` | int | Byte offset from start of COMMON block |
| `char_len` | int | String length for CHARACTER type; 1 for all others |

---

## Type Size Reference

| Fortran Type | kind | Bytes per element |
|---|---|---|
| `REAL` / `REAL*4` | 4 | 4 |
| `REAL*8` / `DOUBLE PRECISION` | 8 | 8 |
| `INTEGER` / `INTEGER*4` | 4 | 4 |
| `INTEGER*8` | 8 | 8 |
| `LOGICAL` | 4 | 4 |
| `COMPLEX` | 8 | 8 (= 2 × REAL*4) |
| `CHARACTER*n` | 1 | n (length in bytes) |

---

## Edge Cases You Must Handle

### 1. Blank COMMON
```fortran
COMMON X, Y, Z        ! no /name/ → name = "__BLANK_COMMON__"
```

### 2. Arrays — size computation
```fortran
REAL*8 :: A(3,4)      ! size = 8 × 3 × 4 = 96 bytes
```

### 3. EQUIVALENCE offset shifts
```fortran
EQUIVALENCE (X(1), A(5))    ! A starts 16 bytes before X in memory
```
Detected and recorded in the `equivalences` array in JSON.

### 4. SAVE detection
Both inline (`SAVE /BLOCKNAME/`) and attribute form (`SAVE` next to variable) must be detected.

### 5. Multiple blocks per file
One subroutine can declare multiple COMMON blocks. All are extracted.

### 6. REAL*8 alignment violations
`REAL*8` at offset not divisible by 8 → flagged by Person 2, but you must compute offsets accurately so they can detect it.

---

## Test Files

All test cases are in `tests/fortran_samples/`:

| File | Tests |
|---|---|
| `test01_file_a.f90` / `test01_file_b.f90` | Clean matching block (no errors) |
| `test02_size_mismatch_a.f90` / `test02_size_mismatch_b.f90` | Size mismatch bug |
| `test03_type_punning_a.f90` / `test03_type_punning_b.f90` | Type punning bug |
| `test04_alignment.f90` | Alignment violation |
| `test05_equivalence.f90` | EQUIVALENCE backward extension |
| `test06_save_inconsistent_a.f90` / `test06_save_inconsistent_b.f90` | SAVE inconsistency |
| `test07_blank_common.f90` | Unnamed COMMON block |
| `test08_multidim.f90` | Multi-dimensional arrays |
| `test09_character.f90` | CHARACTER type handling |
| `test10_multiple_blocks.f90` | Multiple blocks in one file |

---

## Integration Contract with Person 2 and 3

- **Field names are frozen.** Do not rename any JSON key.
- **`name` must be uppercase.** Normalize all block/variable names to uppercase.
- **`__BLANK_COMMON__`** is the canonical name for unnamed blocks.
- **`offset` is always from the start of the block**, not from the file start.
- **`total_size` must equal the sum of all `size_bytes` values** in `variables`.
- If a file has no COMMON blocks, emit `"common_blocks": []` (not null).
- If flang fails on a file, still emit a valid JSON with an `"error"` key.

---

## Project Structure

```
fortran_analyzer/
├── include/
│   ├── common_block.hpp          ← data structures
│   ├── flang_runner.hpp          ← flang subprocess wrapper
│   ├── symbol_dump_parser.hpp    ← parses flang dump output
│   ├── equivalence_resolver.hpp  ← EQUIVALENCE handling
│   └── json_serializer.hpp       ← JSON output
├── src/
│   ├── collector.cpp             ← single-file main()
│   └── batch_collect.cpp         ← directory batch main()
├── tests/
│   └── fortran_samples/          ← 10 test .f90 files
├── outputs/                      ← generated JSON goes here
├── CMakeLists.txt
├── build.sh
└── README.md
```
