"""
Phase 3: Fortran COMMON Block Validator
Reads all JSON files from the outputs/ directory,
groups by COMMON block name, and cross-validates for:
  1. Size mismatches
  2. Type punning (same offset, different type)
  3. Alignment violations
  4. SAVE attribute inconsistencies
  5. EQUIVALENCE conflicts (flags blocks that have equivalences)
"""

import json
import glob
import os
import sys
from collections import defaultdict


# ── Helpers ──────────────────────────────────────────────────────────────────

def load_all_jsons(outputs_dir: str) -> list[dict]:
    """Load every .json file in outputs_dir and return as a list."""
    pattern = os.path.join(outputs_dir, "*.json")
    files = sorted(glob.glob(pattern))
    if not files:
        print(f"[ERROR] No JSON files found in '{outputs_dir}'")
        sys.exit(1)

    records = []
    for path in files:
        with open(path, "r") as f:
            data = json.load(f)
        # Attach the json filename for easy reference in messages
        data["_json_file"] = os.path.basename(path)
        records.append(data)

    print(f"[INFO] Loaded {len(records)} JSON file(s) from '{outputs_dir}'\n")
    return records


def build_global_db(records: list[dict]) -> dict:
    """
    Build a global database keyed by COMMON block name.
    Value = list of dicts, one per (file, block) declaration.
    """
    db = defaultdict(list)   # block_name -> [declaration, ...]

    for record in records:
        source_file = record.get("file", record.get("_json_file"))
        for block in record.get("common_blocks", []):
            entry = {
                "source_file": source_file,
                "json_file":   record["_json_file"],
                "block_name":  block["name"],
                "total_size":  block["total_size"],
                "saved":       block.get("saved", False),
                "variables":   block["variables"],
                "equivalences": record.get("equivalences", []),
            }
            db[block["name"]].append(entry)

    return db


# ── Checks ────────────────────────────────────────────────────────────────────

NUMERIC_TYPES = {"INTEGER", "REAL", "DOUBLE PRECISION", "COMPLEX", "LOGICAL"}
FLOAT_TYPES   = {"REAL", "DOUBLE PRECISION", "COMPLEX"}
INT_TYPES     = {"INTEGER", "LOGICAL"}


def check_size_mismatch(block_name: str, declarations: list[dict]) -> list[str]:
    issues = []
    sizes = [(d["source_file"], d["total_size"]) for d in declarations]
    unique_sizes = set(s for _, s in sizes)
    if len(unique_sizes) > 1:
        detail = ", ".join(f"{f} → {s} bytes" for f, s in sizes)
        issues.append(
            f"[SIZE MISMATCH] /{block_name}/\n"
            f"    {detail}"
        )
    return issues


def check_type_punning(block_name: str, declarations: list[dict]) -> list[str]:
    """
    At every offset that appears in more than one file,
    check whether the variable types are compatible.
    Incompatible = one is a float type, the other is an integer/logical type.
    """
    issues = []

    # Build map: offset -> list of (file, varname, type, kind)
    offset_map = defaultdict(list)
    for decl in declarations:
        for var in decl["variables"]:
            offset_map[var["offset"]].append({
                "file":    decl["source_file"],
                "varname": var["name"],
                "type":    var["type"].upper(),
                "kind":    var["kind"],
            })

    for offset, entries in sorted(offset_map.items()):
        if len(entries) < 2:
            continue

        types_at_offset = set(e["type"] for e in entries)

        # Flag if we see a mix of float and integer types
        has_float = any(t in FLOAT_TYPES for t in types_at_offset)
        has_int   = any(t in INT_TYPES   for t in types_at_offset)

        if has_float and has_int:
            detail = ", ".join(
                f"{e['file']} declares '{e['varname']}' as {e['type']}(kind={e['kind']})"
                for e in entries
            )
            issues.append(
                f"[TYPE PUNNING] /{block_name}/ at offset {offset} bytes\n"
                f"    {detail}"
            )

        # Also flag same numeric type but different kind (e.g. REAL*4 vs REAL*8)
        elif len(types_at_offset) == 1:
            kinds_at_offset = set(e["kind"] for e in entries)
            if len(kinds_at_offset) > 1:
                detail = ", ".join(
                    f"{e['file']} declares '{e['varname']}' as {e['type']}(kind={e['kind']})"
                    for e in entries
                )
                issues.append(
                    f"[KIND MISMATCH] /{block_name}/ at offset {offset} bytes\n"
                    f"    {detail}"
                )

    return issues


def check_alignment(block_name: str, declarations: list[dict]) -> list[str]:
    """
    Every variable's offset must be a multiple of its kind (byte size).
    e.g. a REAL*8 (kind=8) at offset 4 is misaligned.
    """
    issues = []
    seen = set()   # avoid duplicate messages for the same var across files

    for decl in declarations:
        for var in decl["variables"]:
            alignment = var["kind"]
            offset    = var["offset"]
            key       = (block_name, var["name"], offset)
            if key in seen:
                continue
            seen.add(key)

            if alignment > 0 and offset % alignment != 0:
                issues.append(
                    f"[ALIGNMENT] /{block_name}/ variable '{var['name']}' "
                    f"in {decl['source_file']}\n"
                    f"    offset={offset} is not a multiple of kind={alignment}"
                )
    return issues


def check_save_consistency(block_name: str, declarations: list[dict]) -> list[str]:
    """
    All declarations of a block should agree on SAVE.
    Mixed SAVE / no-SAVE is a violation.
    """
    issues = []
    saved_files     = [d["source_file"] for d in declarations if     d["saved"]]
    not_saved_files = [d["source_file"] for d in declarations if not d["saved"]]

    if saved_files and not_saved_files:
        issues.append(
            f"[SAVE INCONSISTENCY] /{block_name}/\n"
            f"    SAVE declared in:     {saved_files}\n"
            f"    No SAVE declared in:  {not_saved_files}"
        )
    return issues


def check_equivalence_conflicts(block_name: str, declarations: list[dict]) -> list[str]:
    """
    If any file using this block also has EQUIVALENCE statements,
    flag it — EQUIVALENCE can silently shift offsets.
    """
    issues = []
    for decl in declarations:
        if decl.get("equivalences"):
            issues.append(
                f"[EQUIVALENCE WARNING] /{block_name}/ in {decl['source_file']}\n"
                f"    EQUIVALENCE statements present — offsets may be shifted: "
                f"{decl['equivalences']}"
            )
    return issues


# ── Report ────────────────────────────────────────────────────────────────────

def validate(db: dict) -> dict:
    """Run all checks and return a structured report."""
    report = {
        "summary": {
            "total_blocks":      0,
            "blocks_with_issues": 0,
            "total_issues":      0,
        },
        "blocks": {}
    }

    all_checks = [
        ("size_mismatch",         check_size_mismatch),
        ("type_punning",          check_type_punning),
        ("alignment",             check_alignment),
        ("save_inconsistency",    check_save_consistency),
        ("equivalence_conflicts", check_equivalence_conflicts),
    ]

    for block_name, declarations in sorted(db.items()):
        block_issues = []
        for check_name, check_fn in all_checks:
            found = check_fn(block_name, declarations)
            block_issues.extend(found)

        report["summary"]["total_blocks"] += 1
        if block_issues:
            report["summary"]["blocks_with_issues"] += 1
            report["summary"]["total_issues"] += len(block_issues)

        report["blocks"][block_name] = {
            "declared_in": [d["source_file"] for d in declarations],
            "issues":      block_issues,
            "clean":       len(block_issues) == 0,
        }

    return report


def print_report(report: dict):
    s = report["summary"]
    print("=" * 60)
    print("  FORTRAN COMMON BLOCK VALIDATION REPORT")
    print("=" * 60)
    print(f"  Blocks analyzed : {s['total_blocks']}")
    print(f"  Blocks with issues : {s['blocks_with_issues']}")
    print(f"  Total violations   : {s['total_issues']}")
    print("=" * 60)
    print()

    for block_name, info in report["blocks"].items():
        files_str = ", ".join(info["declared_in"])
        if info["clean"]:
            print(f"  ✓  /{block_name}/  →  CLEAN")
            print(f"     Files: {files_str}")
        else:
            print(f"  ✗  /{block_name}/  →  {len(info['issues'])} ISSUE(S)")
            print(f"     Files: {files_str}")
            for issue in info["issues"]:
                # indent each line of the issue message
                for line in issue.splitlines():
                    print(f"     {line}")
        print()

    if s["total_issues"] == 0:
        print("  ✓  All COMMON blocks are consistent. No violations found.")
    else:
        print(f"  ✗  {s['total_issues']} violation(s) detected across "
              f"{s['blocks_with_issues']} block(s).")
    print()


def save_report_json(report: dict, output_path: str):
    with open(output_path, "w") as f:
        json.dump(report, f, indent=2)
    print(f"[INFO] Full report saved to: {output_path}")


# ── Entry point ───────────────────────────────────────────────────────────────

def main():
    # Default: look for JSONs in ./outputs relative to where script is run
    outputs_dir  = sys.argv[1] if len(sys.argv) > 1 else "outputs"
    report_out   = sys.argv[2] if len(sys.argv) > 2 else "validation_report.json"

    records = load_all_jsons(outputs_dir)
    db      = build_global_db(records)
    report  = validate(db)

    print_report(report)
    save_report_json(report, report_out)


if __name__ == "__main__":
    main()