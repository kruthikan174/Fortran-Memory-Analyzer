Fortran COMMON Block Data Extraction Framework

Overview

This project provides an automated way to extract and represent Fortran "COMMON" block information in a structured JSON format. It is designed to assist in the analysis and modernization of legacy Fortran applications where shared memory through "COMMON" blocks is widely used.

The project is divided into three independent modules:

1. Collector – Extracts COMMON block information from Fortran source files.
2. Validator – Checks extracted data for inconsistencies and potential issues.
3. CLI & Reporting – Provides user interaction and report generation.

This repository contains the implementation of the Collector Module.

---

Problem Statement

Legacy Fortran applications often rely on "COMMON" blocks to share variables across multiple program units. As projects grow larger, manually identifying memory layouts, variable offsets, and shared data structures becomes difficult.

The objective of this module is to automatically identify COMMON blocks and generate machine-readable metadata that can be used for further validation and analysis.

---

Module Responsibilities

The Collector module performs the following tasks:

- Reads Fortran source files (".f", ".f90")
- Invokes LLVM Flang compiler tools
- Extracts symbol table information
- Detects COMMON block definitions
- Retrieves variable information
- Calculates memory layout details
- Exports structured JSON output

The generated JSON acts as the input for downstream validation and reporting modules.

---

System Workflow

Fortran Source File
        |
        v
   FlangRunner
        |
        v
 Symbol Dump Output
        |
        v
 SymbolDumpParser
        |
        v
 CommonBlock Objects
        |
        v
  JSON Exporter
        |
        v
   Output JSON

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

src/
│
├── FlangRunner.h
├── FlangRunner.cpp
│
├── SymbolDumpParser.h
├── SymbolDumpParser.cpp
│
├── CommonBlockDef.h
│
├── JsonExporter.h
├── JsonExporter.cpp
│
└── main.cpp

tests/

docs/

CMakeLists.txt
README.md

---

Core Components

1. FlangRunner

Responsible for invoking the LLVM Flang compiler and collecting symbol dump information.

Example command:

flang-new -fc1 -fdebug-dump-symbols sample.f90

Output from the compiler is captured and forwarded to the parser.

---

2. SymbolDumpParser

Processes the compiler-generated symbol dump and extracts:

- COMMON block names
- Variable names
- Data types
- Memory offsets
- Storage sizes

The extracted information is converted into internal C++ data structures.

---

3. JSON Exporter

Converts parsed data into a standardized JSON format.

Example:

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

Represents an individual variable inside a COMMON block.

---

CommonBlockDef

struct CommonBlockDef {
    std::string name;
    std::vector<CommonMember> members;
};

Represents a complete COMMON block definition.

---

Building the Project

Prerequisites

- C++17 compatible compiler
- LLVM Flang installed
- CMake 3.15 or later

Build Steps

mkdir build
cd build

cmake ..
cmake --build .

---

Running the Collector

collector sample.f90

Expected output:

{
  "file": "sample.f90",
  "common_blocks": [...]
}

---

Error Handling

The collector handles common failure scenarios such as:

File Not Found

Input file does not exist.

Flang Not Available

Unable to locate flang-new executable.

Invalid Fortran Source

Compiler parsing failed.

No COMMON Blocks Present

{
  "file": "example.f90",
  "common_blocks": []
}

---

Sample Input

REAL A
INTEGER B

COMMON /BLOCK1/ A, B

Sample Output

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

Future Enhancements

- Support for nested program units
- Enhanced type inference
- COMMON block visualization
- Integration with static analysis tools
- Automatic migration assistance for legacy Fortran code

---

Team Contribution

Person 1 – Collector Module

- Integrated LLVM Flang
- Extracted symbol table information
- Parsed COMMON block definitions
- Computed memory layout metadata
- Generated JSON output

Person 2 – Validator Module

- Validated COMMON block consistency
- Checked type compatibility
- Detected layout mismatches

Person 3 – CLI & Reporting

- Developed command-line interface
- Implemented reporting features
- Added testing and automation support

---

Conclusion

The Collector module provides an automated and reliable method for extracting COMMON block metadata from legacy Fortran programs. The generated JSON serves as a standardized representation that supports validation, reporting, and future modernization efforts.
