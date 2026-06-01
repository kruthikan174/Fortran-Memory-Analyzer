Fortran COMMON Block Data Extraction Framework

Overview

This project provides an automated solution for extracting and representing Fortran "COMMON" block information in a structured JSON format. The framework is intended to support analysis and modernization of legacy Fortran applications that rely on shared memory through "COMMON" blocks.

This repository contains the implementation of the Collector Module, which serves as the data generation layer for the complete system.

---

Project Architecture

Fortran Source File
        │
        ▼
   FlangRunner
        │
        ▼
 Symbol Dump Output
        │
        ▼
 SymbolDumpParser
        │
        ▼
 CommonBlock Objects
        │
        ▼
  JSON Exporter
        │
        ▼
   Output JSON

---

Objective

Legacy Fortran programs frequently use "COMMON" blocks to share variables between program units. Understanding the layout of these blocks manually can be difficult, especially in large codebases.

The goal of this project is to:

- Identify all COMMON blocks in a Fortran source file
- Extract variable information from each block
- Determine memory layout details
- Generate a structured JSON representation
- Provide metadata for validation and reporting modules

---

Module Responsibilities

The Collector module performs the following tasks:

- Reads Fortran source files (".f", ".f90")
- Invokes LLVM Flang compiler tools
- Extracts symbol table information
- Detects COMMON block definitions
- Retrieves variable metadata
- Calculates offsets and storage sizes
- Exports standardized JSON output

---

Technologies Used

Technology| Purpose
C++17| Core implementation
LLVM Flang| Symbol extraction
STL| Data structures and utilities
nlohmann/json| JSON serialization
Regex| Parsing compiler output
CMake| Build configuration

---

Project Structure

project-root/
│
├── src/
│   ├── FlangRunner.h
│   ├── FlangRunner.cpp
│   ├── SymbolDumpParser.h
│   ├── SymbolDumpParser.cpp
│   ├── CommonBlockDef.h
│   ├── JsonExporter.h
│   ├── JsonExporter.cpp
│   └── main.cpp
│
├── tests/
│
├── docs/
│
├── CMakeLists.txt
│
└── README.md

---

Core Components

1. FlangRunner

Responsible for executing LLVM Flang and collecting the generated symbol dump.

Example command:

flang-new -fc1 -fdebug-dump-symbols sample.f90

Responsibilities

- Execute compiler commands
- Capture compiler output
- Report execution errors
- Return raw symbol dump

---

2. SymbolDumpParser

Processes the compiler-generated symbol dump and extracts COMMON block information.

Extracted Information

- COMMON block names
- Variable names
- Data types
- Memory offsets
- Storage sizes

The parsed information is converted into internal C++ data structures.

---

3. JSON Exporter

Converts extracted metadata into a standardized JSON format.

Example output:

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

---

Data Structures

CommonMember

struct CommonMember {
    std::string name;
    std::string type;
    uint64_t size;
    uint64_t offset;
};

Represents a single variable within a COMMON block.

---

CommonBlockDef

struct CommonBlockDef {
    std::string name;
    std::vector<CommonMember> members;
};

Represents a complete COMMON block definition.

---

Build Instructions

Prerequisites

- C++17 compatible compiler
- LLVM Flang installed
- CMake 3.15 or later

Build

mkdir build
cd build

cmake ..
cmake --build .

---

Usage

Run the collector on a Fortran source file:

collector sample.f90

The program generates JSON metadata describing all detected COMMON blocks.

---

Example

Input

REAL A
INTEGER B

COMMON /BLOCK1/ A, B

Generated Output

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

---

Error Handling

File Not Found

Error: Input file not found.

Flang Not Installed

Error: flang-new executable not available.

Invalid Fortran Source

Error: Compiler parsing failed.

No COMMON Blocks Present

{
  "file": "example.f90",
  "common_blocks": []
}

---

Future Improvements

- Support for additional Fortran standards
- Improved type inference
- COMMON block visualization
- Static analysis integration
- Automated migration support for legacy code

---

Team Contributions

Person 1 – Collector Module

- LLVM Flang integration
- Symbol table extraction
- COMMON block identification
- Memory layout generation
- JSON metadata export

Person 2 – Validator Module

- Layout consistency checks
- Type validation
- Error detection

Person 3 – CLI & Reporting

- Command-line interface
- Reporting utilities
- Testing and automation

---

Conclusion

The Collector module automates the extraction of COMMON block metadata from legacy Fortran source files. By generating a structured JSON representation of memory layouts, it provides a reliable foundation for validation, reporting, and future modernization efforts.
