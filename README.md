Fortran COMMON Block Data Extraction Framework

A C++17-based framework for extracting memory layout information from legacy Fortran "COMMON" blocks using LLVM Flang and exporting the results as structured JSON metadata.

---

Table of Contents

- "Introduction" (#introduction)
- "Problem Statement" (#problem-statement)
- "Project Objectives" (#project-objectives)
- "System Architecture" (#system-architecture)
- "Module Responsibilities" (#module-responsibilities)
- "Workflow" (#workflow)
- "Implementation Details" (#implementation-details)
- "Data Structures" (#data-structures)
- "JSON Schema" (#json-schema)
- "Project Structure" (#project-structure)
- "Build Instructions" (#build-instructions)
- "Usage" (#usage)
- "Sample Input and Output" (#sample-input-and-output)
- "Error Handling" (#error-handling)
- "Design Decisions" (#design-decisions)
- "Future Enhancements" (#future-enhancements)
- "Team Contributions" (#team-contributions)

---

Introduction

Legacy scientific and engineering applications often contain large Fortran codebases developed over several decades. A significant portion of these applications use "COMMON" blocks to share variables between program units.

Manually identifying the memory layout of "COMMON" blocks becomes increasingly difficult as project size grows. This framework automates the extraction process and produces a structured representation that can be consumed by validation and reporting tools.

The Collector module acts as the foundation of the entire system by generating accurate metadata describing each detected "COMMON" block.

---

Problem Statement

Fortran "COMMON" blocks provide a mechanism for sharing variables across program units through a common memory region.

Example:

REAL TEMP
INTEGER COUNT

COMMON /DATA/ TEMP, COUNT

In large legacy systems:

- Thousands of COMMON blocks may exist.
- Variable ordering must remain consistent.
- Memory layout mismatches can cause runtime errors.
- Manual inspection is time-consuming and error-prone.

The objective of this project is to automatically extract this information and represent it in a machine-readable format.

---

Project Objectives

The Collector module is responsible for:

- Reading Fortran source files.
- Invoking LLVM Flang compiler infrastructure.
- Extracting symbol table information.
- Identifying COMMON block definitions.
- Determining variable types and sizes.
- Computing memory offsets.
- Exporting standardized JSON metadata.

---

System Architecture

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
│ Structured JSON     │
└─────────────────────┘

---

Module Responsibilities

FlangRunner

The FlangRunner module serves as an interface between the application and the LLVM Flang compiler.

Responsibilities

- Execute compiler commands.
- Capture compiler output.
- Handle execution failures.
- Return raw symbol dump data.

Example Command

flang-new -fc1 -fdebug-dump-symbols sample.f90

---

SymbolDumpParser

The SymbolDumpParser processes compiler-generated symbol information and converts it into structured C++ objects.

Extracted Information

- COMMON block names
- Variable names
- Data types
- Memory offsets
- Storage sizes

---

JSON Exporter

The JSON Exporter serializes extracted metadata into a standard format that can be consumed by other modules.

---

Workflow

Step 1: Read Source File

Input:

REAL A
INTEGER B

COMMON /BLOCK1/ A, B

Step 2: Generate Symbol Dump

LLVM Flang generates symbol table information.

Step 3: Parse Symbols

Relevant COMMON block metadata is extracted.

Step 4: Construct Internal Objects

Objects representing COMMON blocks and variables are created.

Step 5: Export JSON

Structured JSON output is generated.

---

Implementation Details

Technologies Used

Technology| Purpose
C++17| Core implementation
LLVM Flang| Symbol extraction
STL| Containers and utilities
nlohmann/json| JSON serialization
CMake| Build system
Regular Expressions| Symbol parsing

---

Data Structures

CommonMember

Represents an individual variable within a COMMON block.

struct CommonMember {
    std::string name;
    std::string type;
    uint64_t size;
    uint64_t offset;
};

Attributes

Field| Description
name| Variable name
type| Fortran data type
size| Storage size in bytes
offset| Position within COMMON block

---

CommonBlockDef

Represents a complete COMMON block definition.

struct CommonBlockDef {
    std::string name;
    std::vector<CommonMember> members;
};

---

JSON Schema

Root Object

{
  "file": "sample.f90",
  "common_blocks": []
}

COMMON Block

{
  "name": "BLOCK1",
  "members": []
}

Member Definition

{
  "name": "A",
  "type": "REAL",
  "size": 4,
  "offset": 0
}

---

Project Structure

project-root/
│
├── src/
│   ├── FlangRunner.h
│   ├── FlangRunner.cpp
│   ├── SymbolDumpParser.h
│   ├── SymbolDumpParser.cpp
│   ├── JsonExporter.h
│   ├── JsonExporter.cpp
│   ├── CommonBlockDef.h
│   └── main.cpp
│
├── tests/
│
├── docs/
│
├── examples/
│
├── CMakeLists.txt
│
└── README.md

---

Build Instructions

Prerequisites

- LLVM Flang
- CMake 3.15+
- C++17 Compatible Compiler

Build

mkdir build
cd build

cmake ..
cmake --build .

---

Usage

Run the collector:

collector sample.f90

Output:

output.json

---

Sample Input and Output

Input

REAL A
INTEGER B

COMMON /BLOCK1/ A, B

Output

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

Missing File

Error: Input file not found.

Flang Not Installed

Error: flang-new executable not found.

Invalid Source File

Error: Failed to parse Fortran source.

No COMMON Blocks Found

{
  "file": "test.f90",
  "common_blocks": []
}

---

Design Decisions

Why LLVM Flang?

LLVM Flang provides accurate compiler-level symbol information and avoids implementing a custom Fortran parser.

Why JSON?

JSON is lightweight, language-independent, and easy to integrate with downstream validation and reporting tools.

Why Separate Components?

A modular architecture improves maintainability, testing, and future extensibility.

---

Future Enhancements

- Support for additional Fortran standards.
- Visualization of COMMON block layouts.
- Detection of alignment and padding issues.
- Integration with static analysis pipelines.
- Automatic migration recommendations.

---

Team Contributions

Person 1 — Collector Module

- Flang integration
- Symbol extraction
- COMMON block detection
- Memory layout generation
- JSON export

Person 2 — Validator Module

- Consistency verification
- Type checking
- Layout validation

Person 3 — CLI and Reporting

- Command-line interface
- Report generation
- Testing framework

---

Conclusion

The Fortran COMMON Block Data Extraction Framework provides an automated mechanism for extracting memory layout information from legacy Fortran programs. The generated metadata forms the foundation for validation, reporting, and modernization workflows, reducing manual effort and improving analysis accuracy.
