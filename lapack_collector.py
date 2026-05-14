"""
Phase 5: Pure Python COMMON Block Collector for Legacy Fortran
==============================================================
Parses .f / .f90 / .f77 files directly using regex.
No Flang or LLVM required.

Produces the same JSON format as the C++ collector so that
validator.py works unchanged.

Usage:
    python lapack_collector.py <fortran_source_dir> <outputs_dir>

Example:
    python lapack_collector.py lapack-3.12.0 lapack_outputs
"""

import re
import os
import sys
import json
from pathlib import Path


# ── Fortran type system ───────────────────────────────────────────────────────

# Maps Fortran type declarations to (type_name, kind_bytes)
TYPE_MAP = {
    # integers
    r"INTEGER\*8":              ("INTEGER", 8),
    r"INTEGER\*4":              ("INTEGER", 4),
    r"INTEGER\*2":              ("INTEGER", 2),
    r"INTEGER\*1":              ("INTEGER", 1),
    r"INTEGER\(KIND=8\)":       ("INTEGER", 8),
    r"INTEGER\(KIND=4\)":       ("INTEGER", 4),
    r"INTEGER\(KIND=2\)":       ("INTEGER", 2),
    r"INTEGER\(8\)":            ("INTEGER", 8),
    r"INTEGER\(4\)":            ("INTEGER", 4),
    r"INTEGER\(2\)":            ("INTEGER", 2),
    r"INTEGER":                 ("INTEGER", 4),
    # reals
    r"DOUBLE\s+PRECISION":      ("REAL",    8),
    r"REAL\*8":                 ("REAL",    8),
    r"REAL\*4":                 ("REAL",    4),
    r"REAL\(KIND=8\)":          ("REAL",    8),
    r"REAL\(KIND=4\)":          ("REAL",    4),
    r"REAL\(8\)":               ("REAL",    8),
    r"REAL\(4\)":               ("REAL",    4),
    r"REAL":                    ("REAL",    4),
    # complex
    r"DOUBLE\s+COMPLEX":        ("COMPLEX", 16),
    r"COMPLEX\*16":             ("COMPLEX", 16),
    r"COMPLEX\*8":              ("COMPLEX", 8),
    r"COMPLEX\(KIND=8\)":       ("COMPLEX", 16),
    r"COMPLEX\(KIND=4\)":       ("COMPLEX", 8),
    r"COMPLEX\(8\)":            ("COMPLEX", 16),
    r"COMPLEX\(4\)":            ("COMPLEX", 8),
    r"COMPLEX":                 ("COMPLEX", 8),
    # logical
    r"LOGICAL\*4":              ("LOGICAL", 4),
    r"LOGICAL\*1":              ("LOGICAL", 1),
    r"LOGICAL":                 ("LOGICAL", 4),
    # character  (char_len handled separately)
    r"CHARACTER\*\(\*\)":       ("CHARACTER", 1),
    r"CHARACTER\*\d+":          ("CHARACTER", 1),
    r"CHARACTER":               ("CHARACTER", 1),
}

# Pre-compile for speed
COMPILED_TYPES = [
    (re.compile(r"^" + pat, re.IGNORECASE), info)
    for pat, info in TYPE_MAP.items()
]


def parse_fortran_type(decl_str: str):
    """
    Given a declaration string like 'REAL*8' or 'INTEGER(KIND=4)',
    return (type_name, kind_bytes).
    """
    s = decl_str.strip().upper()
    for pattern, info in COMPILED_TYPES:
        if pattern.match(s):
            return info
    return ("UNKNOWN", 4)


def parse_char_len(decl_str: str) -> int:
    """Extract character length from CHARACTER*N declarations."""
    m = re.search(r"CHARACTER\*(\d+)", decl_str, re.IGNORECASE)
    if m:
        return int(m.group(1))
    return 1


# ── Array dimension parser ────────────────────────────────────────────────────

def parse_dimensions(dim_str: str) -> list:
    """
    Parse dimension string like '100' or '10,20' or 'N' into a list.
    Unknown/variable dimensions stored as -1.
    """
    if not dim_str:
        return []
    dims = []
    for part in dim_str.split(","):
        part = part.strip()
        # Handle N:M ranges (take M-N+1 or just store -1)
        if ":" in part:
            dims.append(-1)
        else:
            try:
                dims.append(int(part))
            except ValueError:
                dims.append(-1)   # symbolic dimension like N, LDA, etc.
    return dims


def compute_array_size(kind: int, dims: list) -> int:
    """Compute total bytes; returns -1 if any dimension is symbolic."""
    if not dims:
        return kind
    total = 1
    for d in dims:
        if d == -1:
            return -1
    for d in dims:
        total *= d
    return total * kind


# ── Fortran source cleaner ────────────────────────────────────────────────────

def clean_source(raw: str) -> list[tuple[int, str]]:
    """
    Return list of (line_number, cleaned_line) tuples.
    - Strips fixed-form comments (lines starting with C or *)
    - Strips inline ! comments
    - Joins continuation lines (& at end, or column 6 continuation in fixed form)
    - Uppercases everything
    """
    lines = raw.splitlines()
    result   = []
    buf      = ""
    buf_line = 0

    i = 0
    while i < len(lines):
        raw_line = lines[i]
        i += 1
        lineno = i

        # Fixed-form: comment lines start with C, c, or * in column 1
        stripped = raw_line.rstrip()
        if re.match(r'^[Cc*]', stripped):
            continue

        # Remove inline Fortran comments (!)
        code = re.sub(r'!.*$', '', stripped).rstrip().upper()

        if not code.strip():
            continue

        # Fixed-form continuation: non-space/zero in column 6
        if len(code) >= 6 and code[5] not in (' ', '0') and buf:
            buf += " " + code[6:].strip()
            continue

        # Free-form continuation: line ends with &
        if buf:
            # flush previous buffer
            result.append((buf_line, buf))
            buf = ""

        if code.endswith("&"):
            buf      = code[:-1].strip()
            buf_line = lineno
        else:
            result.append((lineno, code.strip()))

    if buf:
        result.append((buf_line, buf))

    return result


# ── Variable declaration collector ───────────────────────────────────────────

# Matches:  REAL X, Y(10), Z
# or:       REAL :: X, Y(10), Z
VAR_LIST_RE = re.compile(
    r"^(\w[\w\s\*\(\),=]*?)\s*::\s*(.+)$|^(\w[\w\s\*\(\)]*?)\s+(\w.*)$"
)

COMMON_RE = re.compile(
    r"^COMMON\s*(/(\w+)/|//)\s*(.+)$", re.IGNORECASE
)

SAVE_RE = re.compile(r"^SAVE\b", re.IGNORECASE)

EQUIVALENCE_RE = re.compile(r"^EQUIVALENCE\s*\((.+)\)", re.IGNORECASE)


def parse_var_list(var_str: str) -> list[dict]:
    """
    Parse a comma-separated variable list from a COMMON statement.
    Handles: X, Y(10), Z(5,5), A
    Returns list of {"name": ..., "dims": [...]}
    """
    vars_out = []
    # Split by comma but respect parentheses
    depth = 0
    current = ""
    for ch in var_str:
        if ch == "(":
            depth += 1
            current += ch
        elif ch == ")":
            depth -= 1
            current += ch
        elif ch == "," and depth == 0:
            vars_out.append(current.strip())
            current = ""
        else:
            current += ch
    if current.strip():
        vars_out.append(current.strip())

    result = []
    for v in vars_out:
        v = v.strip()
        m = re.match(r"^(\w+)\(([^)]+)\)$", v)
        if m:
            result.append({"name": m.group(1).upper(),
                           "dims": parse_dimensions(m.group(2))})
        else:
            result.append({"name": v.upper(), "dims": []})
    return result


def parse_type_declaration(line: str) -> tuple[str, int, int, list[str]]:
    """
    Try to parse a variable type declaration line.
    Returns (type_name, kind, char_len, [varnames with optional dims])
    or None if not a type declaration.
    """
    # Try each type pattern
    for pattern, (type_name, kind) in COMPILED_TYPES:
        m = pattern.match(line)
        if not m:
            continue
        rest = line[m.end():].strip()
        # Strip leading :: if present
        rest = re.sub(r"^::", "", rest).strip()
        if not rest:
            continue
        char_len = parse_char_len(line) if type_name == "CHARACTER" else 1
        return type_name, kind, char_len, rest
    return None


# ── Single-file parser ────────────────────────────────────────────────────────

def parse_fortran_file(filepath: str) -> dict:
    """
    Parse one Fortran file and return a JSON-compatible dict
    matching the format produced by the C++ collector.
    """
    try:
        with open(filepath, "r", encoding="utf-8", errors="replace") as f:
            raw = f.read()
    except Exception as e:
        return {"file": filepath, "common_blocks": [], "equivalences": [],
                "error": str(e)}

    lines = clean_source(raw)

    # Pass 1: collect all variable declarations in this file
    # var_types: varname -> (type, kind, char_len)
    var_types: dict[str, tuple] = {}

    for _, line in lines:
        parsed = parse_type_declaration(line)
        if parsed is None:
            continue
        type_name, kind, char_len, var_list_str = parsed
        # Parse the variable list (may include dimensions)
        vars_in_decl = parse_var_list(var_list_str)
        for v in vars_in_decl:
            var_types[v["name"]] = (type_name, kind, char_len)

    # Pass 2: collect COMMON blocks
    # common_vars: block_name -> [{"name", "dims"}]
    common_vars: dict[str, list] = {}
    has_save = False
    equivalences = []

    for lineno, line in lines:
        # SAVE statement
        if SAVE_RE.match(line):
            has_save = True

        # EQUIVALENCE
        m = EQUIVALENCE_RE.match(line)
        if m:
            equivalences.append({"raw": m.group(1), "involves_common": True})

        # COMMON
        m = COMMON_RE.match(line)
        if not m:
            continue

        block_name = (m.group(2) or "__BLANK_COMMON__").upper()
        var_list_str = m.group(3).strip()
        vars_in_block = parse_var_list(var_list_str)

        if block_name not in common_vars:
            common_vars[block_name] = []
        common_vars[block_name].extend(vars_in_block)

    # Pass 3: build output structure
    common_blocks = []
    for block_name, var_list in common_vars.items():
        variables = []
        offset = 0
        total_size = 0

        for v in var_list:
            vname = v["name"]
            dims  = v["dims"]

            # Look up type; default to REAL*4 if unknown (common in old Fortran)
            if vname in var_types:
                type_name, kind, char_len = var_types[vname]
            else:
                # Implicit typing: I-N = INTEGER, else REAL
                if vname and vname[0].upper() in "IJKLMN":
                    type_name, kind, char_len = "INTEGER", 4, 1
                else:
                    type_name, kind, char_len = "REAL", 4, 1

            size = compute_array_size(kind if type_name != "CHARACTER"
                                      else char_len, dims)
            if size == -1:
                size = 0   # symbolic — can't compute statically

            variables.append({
                "name":       vname,
                "type":       type_name,
                "kind":       kind,
                "dimensions": dims,
                "size_bytes": size,
                "offset":     offset,
                "char_len":   char_len,
            })
            offset     += size
            total_size += size

        common_blocks.append({
            "name":        block_name,
            "total_size":  total_size,
            "saved":       has_save,
            "source_file": filepath,
            "line_number": -1,
            "variables":   variables,
        })

    return {
        "file":          filepath,
        "common_blocks": common_blocks,
        "equivalences":  equivalences,
    }


# ── Batch runner ──────────────────────────────────────────────────────────────

def collect_all(source_dir: str, outputs_dir: str):
    os.makedirs(outputs_dir, exist_ok=True)

    fortran_files = []
    for ext in ("*.f", "*.f90", "*.f77", "*.for", "*.F", "*.F90"):
        fortran_files.extend(Path(source_dir).rglob(ext))

    print(f"[INFO] Found {len(fortran_files)} Fortran files in '{source_dir}'")

    processed  = 0
    with_common = 0
    total_blocks = 0

    for fpath in sorted(fortran_files):
        result = parse_fortran_file(str(fpath))

        if result["common_blocks"]:
            with_common  += 1
            total_blocks += len(result["common_blocks"])

            # Save one JSON per source file
            rel   = fpath.relative_to(source_dir)
            # flatten path separators for the filename
            safe  = str(rel).replace(os.sep, "__").replace("/", "__")
            out   = os.path.join(outputs_dir, safe + ".json")
            with open(out, "w") as f:
                json.dump(result, f, indent=2)

        processed += 1
        if processed % 200 == 0:
            print(f"  ... processed {processed}/{len(fortran_files)} files")

    print(f"[INFO] Done. {with_common} files had COMMON blocks "
          f"({total_blocks} block declarations total).")
    print(f"[INFO] JSONs written to '{outputs_dir}/'")
    return with_common, total_blocks


# ── Entry point ───────────────────────────────────────────────────────────────

if __name__ == "__main__":
    source_dir  = sys.argv[1] if len(sys.argv) > 1 else "lapack-3.12.0"
    outputs_dir = sys.argv[2] if len(sys.argv) > 2 else "lapack_outputs"
    collect_all(source_dir, outputs_dir)