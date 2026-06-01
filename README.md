# Fortran COMMON Block Data Extraction Framework

> A C++17-based framework for extracting memory layout information from legacy Fortran `COMMON` blocks using LLVM Flang, exported as structured JSON metadata.

---

## Table of Contents

- [Introduction](#introduction)
- [Problem Statement](#problem-statement)
- [System Architecture](#system-architecture)
- [Module Responsibilities](#module-responsibilities)
- [Data Structures](#data-structures)
- [JSON Schema](#json-schema)
- [Project Structure](#project-structure)
- [Build Instructions](#build-instructions)
- [Usage](#usage)
- [Sample Input & Output](#sample-input--output)
- [Error Handling](#error-handling)
- [Design Decisions](#design-decisions)
- [Future Enhancements](#future-enhancements)
- [Team Contributions](#team-contributions)

---

## Introduction

Legacy scientific and engineering applications often contain large Fortran codebases developed over several decades. A significant portion of these applications use `COMMON` blocks to share variables between program units.

Manually identifying the memory layout of `COMMON` blocks becomes increasingly difficult as project size grows. This framework automates the extraction process and produces a structured representation consumable by validation and reporting tools.

---

## Problem Statement

Fortran `COMMON` blocks provide a mechanism for sharing variables across program units through a common memory region:

```fortran
REAL TEMP
INTEGER COUNT

COMMON /DATA/ TEMP, COUNT
```

In large legacy systems:

- Thousands of `COMMON` blocks may exist
- Variable ordering must remain consistent
- Memory layout mismatches can cause runtime errors
- Manual inspection is time-consuming and error-prone

This framework automatically extracts this information and represents it in a machine-readable format.

---

## System Architecture

```
┌─────────────────────┐
│  Fortran Source     │
│   (.f / .f90)       │
└──────────┬──────────┘
           │
           ▼
┌─────────────────────┐
│    FlangRunner      │
│ Executes LLVM Flang │
└──────────┬──────────┘
           │
           ▼
┌─────────────────────┐
│ Symbol Dump Output  │
└──────────┬──────────┘
           │
           ▼
┌─────────────────────┐
│ SymbolDumpParser    │
│ Parses Dump Data    │
└──────────┬──────────┘
           │
           ▼
┌─────────────────────┐
│ CommonBlock Objects │
└──────────┬──────────┘
           │
           ▼
┌─────────────────────┐
│   JSON Exporter     │
└──────────┬──────────┘
           │
           ▼
┌─────────────────────┐
│  Structured JSON    │
└─────────────────────┘
```

---

## Module Responsibilities

### FlangRunner

Serves as the interface between the application and the LLVM Flang compiler.

- Executes compiler commands
- Captures compiler output
- Handles execution failures
- Returns raw symbol dump data

**Example command executed internally:**

```bash
flang-new -fc1 -fdebug-dump-symbols sample.f90
```

---

### SymbolDumpParser

Processes compiler-generated symbol information and converts it into structured C++ objects.

Extracts:
- `COMMON` block names
- Variable names
- Data types
- Memory offsets
- Storage sizes

---

### JSON Exporter

Serializes extracted metadata into a standard JSON format consumable by downstream tools.

---

## Data Structures

### `CommonMember`

Represents an individual variable within a `COMMON` block.

```cpp
struct CommonMember {
    std::string name;
    std::string type;
    uint64_t    size;
    uint64_t    offset;
};
```

| Field    | Description                        |
|----------|------------------------------------|
| `name`   | Variable name                      |
| `type`   | Fortran data type                  |
| `size`   | Storage size in bytes              |
| `offset` | Position within the `COMMON` block |

---

### `CommonBlockDef`

Represents a complete `COMMON` block definition.

```cpp
struct CommonBlockDef {
    std::string              name;
    std::vector<CommonMember> members;
};
```

---

## JSON Schema

**Root object:**

```json
{
  "file": "sample.f90",
  "common_blocks": []
}
```

**`COMMON` block entry:**

```json
{
  "name": "BLOCK1",
  "members": []
}
```

**Member definition:**

```json
{
  "name": "A",
  "type": "REAL",
  "size": 4,
  "offset": 0
}
```

---

## Project Structure

```
project-root/
├── src/
│   ├── FlangRunner.h
│   ├── FlangRunner.cpp
│   ├── SymbolDumpParser.h
│   ├── SymbolDumpParser.cpp
│   ├── JsonExporter.h
│   ├── JsonExporter.cpp
│   ├── CommonBlockDef.h
│   └── main.cpp
├── tests/
├── docs/
├── examples/
├── CMakeLists.txt
└── README.md
```

---

## Build Instructions

### Prerequisites

- [LLVM Flang](https://flang.llvm.org/)
- CMake 3.15+
- C++17 compatible compiler

### Steps

```bash
mkdir build
cd build
cmake ..
cmake --build .
```

---

## Usage

```bash
./collector <input.f90>
```

Output is written to `output.json` in the current directory.

**Example:**

```bash
./collector sample.f90
# → output.json
```

---

## Sample Input & Output

**Input (`sample.f90`):**

```fortran
REAL A
INTEGER B

COMMON /BLOCK1/ A, B
```

**Output (`output.json`):**

```json
{
  "file": "sample.f90",
  "common_blocks": [
    {
      "name": "BLOCK1",
      "members": [
        {
          "name": "A",
          "type": "REAL",
          "size": 4,
          "offset": 0
        },
        {
          "name": "B",
          "type": "INTEGER",
          "size": 4,
          "offset": 4
        }
      ]
    }
  ]
}
```

---

## Error Handling

| Condition              | Message                                  |
|------------------------|------------------------------------------|
| Missing input file     | `Error: Input file not found.`           |
| Flang not installed    | `Error: flang-new executable not found.` |
| Invalid Fortran source | `Error: Failed to parse Fortran source.` |
| No `COMMON` blocks     | Returns `"common_blocks": []`            |

**Example — no blocks found:**

```json
{
  "file": "test.f90",
  "common_blocks": []
}
```

---

## Design Decisions

**Why LLVM Flang?**
LLVM Flang provides accurate compiler-level symbol information, eliminating the need for a custom Fortran parser.

**Why JSON?**
JSON is lightweight, language-independent, and easy to integrate with downstream validation and reporting tools.

**Why separate components?**
A modular architecture improves maintainability, testability, and future extensibility.

---

## Future Enhancements

- [ ] Support for additional Fortran standards (F77, F95, F2003)
- [ ] Visual layout diagrams for `COMMON` blocks
- [ ] Detection of alignment and padding issues
- [ ] Integration with static analysis pipelines
- [ ] Automatic migration recommendations

---

## Team Contributions

| Member   | Module              | Responsibilities                                                                 |
|----------|---------------------|----------------------------------------------------------------------------------|
| Person 1 | Collector           | Flang integration, symbol extraction, `COMMON` block detection, JSON export      |
| Person 2 | Validator           | Consistency verification, type checking, layout validation                       |
| Person 3 | CLI & Reporting     | Command-line interface, report generation, testing framework                     |

---

## Technologies Used

| Technology        | Purpose              |
|-------------------|----------------------|
| C++17             | Core implementation  |
| LLVM Flang        | Symbol extraction    |
| STL               | Containers & utilities |
| nlohmann/json     | JSON serialization   |
| CMake             | Build system         |
| Regular Expressions | Symbol parsing     |

---

## License

This project is licensed under the [MIT License](LICENSE).
