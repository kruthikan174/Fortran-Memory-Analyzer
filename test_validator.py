"""
Phase 4: Pytest Test Suite for Fortran COMMON Block Validator
=============================================================
Tests that the validator correctly detects every seeded bug
in the outputs/ directory.

Run with:
    pytest test_validator.py -v
"""

import json
import pytest
import sys
import os

# ── Make sure validator.py is importable from the project root ────────────────
sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from validator import load_all_jsons, build_global_db, validate


# ── Fixtures ──────────────────────────────────────────────────────────────────

OUTPUTS_DIR = "outputs"

@pytest.fixture(scope="session")
def report():
    """
    Load all JSONs, build the global DB, and run the full validator once.
    All tests share this single report for speed.
    """
    records = load_all_jsons(OUTPUTS_DIR)
    db      = build_global_db(records)
    return validate(db)


@pytest.fixture(scope="session")
def db():
    """Expose the raw global DB for low-level checks."""
    records = load_all_jsons(OUTPUTS_DIR)
    return build_global_db(records)


# ── Helper ────────────────────────────────────────────────────────────────────

def issues_for(report, block_name) -> list[str]:
    """Return the list of issue strings for a given block."""
    block = report["blocks"].get(block_name)
    assert block is not None, f"Block /{block_name}/ not found in report"
    return block["issues"]


def has_issue_type(issues: list[str], tag: str) -> bool:
    """Check whether any issue string contains the given tag."""
    return any(tag in issue for issue in issues)


# ══════════════════════════════════════════════════════════════════════════════
#  SECTION 1 — Summary-level sanity checks
# ══════════════════════════════════════════════════════════════════════════════

class TestSummary:

    def test_blocks_are_discovered(self, report):
        """Validator must find at least 11 distinct COMMON blocks."""
        assert report["summary"]["total_blocks"] >= 11

    def test_violations_detected(self, report):
        """At least 6 violations must be detected across all files."""
        assert report["summary"]["total_issues"] >= 6

    def test_blocks_with_issues(self, report):
        """At least 4 blocks must have issues."""
        assert report["summary"]["blocks_with_issues"] >= 4

    def test_all_known_blocks_present(self, report):
        """Every expected block name must appear in the report."""
        expected = {
            "MYDATA", "BIGBLOCK", "PHYSICS", "ALIGNTEST",
            "EQTEST", "PERSIST", "MATRICES", "STRINGS",
            "SIM_STATE", "PHYSICS_CONST", "__BLANK_COMMON__"
        }
        found = set(report["blocks"].keys())
        missing = expected - found
        assert not missing, f"Missing blocks: {missing}"


# ══════════════════════════════════════════════════════════════════════════════
#  SECTION 2 — Test 01: MYDATA — should be CLEAN
# ══════════════════════════════════════════════════════════════════════════════

class TestTest01Clean:

    def test_mydata_is_clean(self, report):
        """/MYDATA/ (test01) is correctly matched — no issues expected."""
        assert report["blocks"]["MYDATA"]["clean"] is True

    def test_mydata_declared_in_two_files(self, report):
        """/MYDATA/ must be seen in both file_a and file_b."""
        files = report["blocks"]["MYDATA"]["declared_in"]
        assert len(files) == 2

    def test_mydata_no_size_mismatch(self, report):
        issues = issues_for(report, "MYDATA")
        assert not has_issue_type(issues, "SIZE MISMATCH")

    def test_mydata_no_type_punning(self, report):
        issues = issues_for(report, "MYDATA")
        assert not has_issue_type(issues, "TYPE PUNNING")


# ══════════════════════════════════════════════════════════════════════════════
#  SECTION 3 — Test 02: BIGBLOCK — size mismatch + type punning
# ══════════════════════════════════════════════════════════════════════════════

class TestTest02SizeMismatch:

    def test_bigblock_has_issues(self, report):
        """/BIGBLOCK/ must NOT be clean."""
        assert report["blocks"]["BIGBLOCK"]["clean"] is False

    def test_bigblock_size_mismatch_detected(self, report):
        """Size mismatch: file_a=400 bytes, file_b=800 bytes."""
        issues = issues_for(report, "BIGBLOCK")
        assert has_issue_type(issues, "SIZE MISMATCH"), \
            "Expected SIZE MISMATCH for /BIGBLOCK/"

    def test_bigblock_size_mismatch_mentions_both_sizes(self, report):
        """The SIZE MISMATCH message must reference both 400 and 800."""
        issues = issues_for(report, "BIGBLOCK")
        size_issues = [i for i in issues if "SIZE MISMATCH" in i]
        assert size_issues, "No SIZE MISMATCH issue found"
        msg = size_issues[0]
        assert "400" in msg and "800" in msg, \
            f"Expected both 400 and 800 bytes in message, got:\n{msg}"

    def test_bigblock_type_punning_at_offset_0(self, report):
        """REAL vs INTEGER at offset 0 must be flagged as TYPE PUNNING."""
        issues = issues_for(report, "BIGBLOCK")
        pun_issues = [i for i in issues if "TYPE PUNNING" in i and "offset 0" in i]
        assert pun_issues, "Expected TYPE PUNNING at offset 0 for /BIGBLOCK/"

    def test_bigblock_mentions_both_files(self, report):
        """Both file_a and file_b must appear in the BIGBLOCK report."""
        files = report["blocks"]["BIGBLOCK"]["declared_in"]
        file_names = " ".join(files)
        assert "test02_size_mismatch_a" in file_names
        assert "test02_size_mismatch_b" in file_names


# ══════════════════════════════════════════════════════════════════════════════
#  SECTION 4 — Test 03: PHYSICS — type punning at two offsets
# ══════════════════════════════════════════════════════════════════════════════

class TestTest03TypePunning:

    def test_physics_has_issues(self, report):
        """/PHYSICS/ must NOT be clean."""
        assert report["blocks"]["PHYSICS"]["clean"] is False

    def test_physics_type_punning_offset_0(self, report):
        """VOLTAGE(REAL) vs IRAW(INTEGER) at offset 0."""
        issues = issues_for(report, "PHYSICS")
        pun_at_0 = [i for i in issues if "TYPE PUNNING" in i and "offset 0" in i]
        assert pun_at_0, "Expected TYPE PUNNING at offset 0 for /PHYSICS/"

    def test_physics_type_punning_offset_4(self, report):
        """CURRENT(REAL) vs IRAW2(INTEGER) at offset 4."""
        issues = issues_for(report, "PHYSICS")
        pun_at_4 = [i for i in issues if "TYPE PUNNING" in i and "offset 4" in i]
        assert pun_at_4, "Expected TYPE PUNNING at offset 4 for /PHYSICS/"

    def test_physics_two_punning_issues(self, report):
        """Exactly 2 TYPE PUNNING issues for /PHYSICS/."""
        issues = issues_for(report, "PHYSICS")
        pun_issues = [i for i in issues if "TYPE PUNNING" in i]
        assert len(pun_issues) == 2, \
            f"Expected 2 TYPE PUNNING issues, got {len(pun_issues)}"

    def test_physics_mentions_voltage_and_iraw(self, report):
        """Issue message must name the conflicting variables."""
        issues = issues_for(report, "PHYSICS")
        all_text = " ".join(issues)
        assert "VOLTAGE" in all_text or "voltage" in all_text.lower()
        assert "IRAW" in all_text or "iraw" in all_text.lower()


# ══════════════════════════════════════════════════════════════════════════════
#  SECTION 5 — Test 04: ALIGNTEST — should be CLEAN
# ══════════════════════════════════════════════════════════════════════════════

class TestTest04Alignment:

    def test_aligntest_is_clean(self, report):
        """/ALIGNTEST/ — if no misaligned vars were seeded, must be clean."""
        assert report["blocks"]["ALIGNTEST"]["clean"] is True

    def test_aligntest_no_alignment_issues(self, report):
        issues = issues_for(report, "ALIGNTEST")
        assert not has_issue_type(issues, "ALIGNMENT")


# ══════════════════════════════════════════════════════════════════════════════
#  SECTION 6 — Test 05: EQTEST — equivalence warning
# ══════════════════════════════════════════════════════════════════════════════

class TestTest05Equivalence:

    def test_eqtest_has_issues(self, report):
        """/EQTEST/ has EQUIVALENCE statements — must not be clean."""
        assert report["blocks"]["EQTEST"]["clean"] is False

    def test_eqtest_equivalence_warning(self, report):
        """EQUIVALENCE WARNING must be raised for /EQTEST/."""
        issues = issues_for(report, "EQTEST")
        assert has_issue_type(issues, "EQUIVALENCE WARNING"), \
            "Expected EQUIVALENCE WARNING for /EQTEST/"

    def test_eqtest_warning_mentions_common_block(self, report):
        """The warning must reference the EQTEST block by name."""
        issues = issues_for(report, "EQTEST")
        eq_issues = [i for i in issues if "EQUIVALENCE" in i]
        assert any("EQTEST" in i for i in eq_issues)


# ══════════════════════════════════════════════════════════════════════════════
#  SECTION 7 — Test 06: PERSIST — SAVE inconsistency
# ══════════════════════════════════════════════════════════════════════════════

class TestTest06SaveInconsistency:

    def test_persist_has_issues(self, report):
        """/PERSIST/ must NOT be clean."""
        assert report["blocks"]["PERSIST"]["clean"] is False

    def test_persist_save_inconsistency_detected(self, report):
        """SAVE INCONSISTENCY must be flagged for /PERSIST/."""
        issues = issues_for(report, "PERSIST")
        assert has_issue_type(issues, "SAVE INCONSISTENCY"), \
            "Expected SAVE INCONSISTENCY for /PERSIST/"

    def test_persist_save_mentions_both_files(self, report):
        """Both file_a (SAVE) and file_b (no SAVE) must be named."""
        issues = issues_for(report, "PERSIST")
        save_issues = [i for i in issues if "SAVE INCONSISTENCY" in i]
        assert save_issues
        msg = save_issues[0]
        assert "test06_save_inconsistent_a" in msg
        assert "test06_save_inconsistent_b" in msg

    def test_persist_declared_in_two_files(self, report):
        files = report["blocks"]["PERSIST"]["declared_in"]
        assert len(files) == 2


# ══════════════════════════════════════════════════════════════════════════════
#  SECTION 8 — Test 07: Blank COMMON — should be CLEAN
# ══════════════════════════════════════════════════════════════════════════════

class TestTest07BlankCommon:

    def test_blank_common_discovered(self, report):
        """Blank COMMON block must be found (stored as __BLANK_COMMON__)."""
        assert "__BLANK_COMMON__" in report["blocks"]

    def test_blank_common_is_clean(self, report):
        assert report["blocks"]["__BLANK_COMMON__"]["clean"] is True


# ══════════════════════════════════════════════════════════════════════════════
#  SECTION 9 — Test 08: MATRICES — multidimensional, should be CLEAN
# ══════════════════════════════════════════════════════════════════════════════

class TestTest08Multidim:

    def test_matrices_is_clean(self, report):
        """/MATRICES/ multidimensional block must be clean."""
        assert report["blocks"]["MATRICES"]["clean"] is True

    def test_matrices_no_issues(self, report):
        issues = issues_for(report, "MATRICES")
        assert len(issues) == 0

    def test_matrices_has_variables(self, db):
        """DB must contain variables for /MATRICES/."""
        decls = db.get("MATRICES", [])
        assert decls, "No declarations found for /MATRICES/"
        all_vars = [v for d in decls for v in d["variables"]]
        assert len(all_vars) > 0


# ══════════════════════════════════════════════════════════════════════════════
#  SECTION 10 — Test 09: STRINGS — CHARACTER type, should be CLEAN
# ══════════════════════════════════════════════════════════════════════════════

class TestTest09Character:

    def test_strings_is_clean(self, report):
        """/STRINGS/ CHARACTER block must be clean."""
        assert report["blocks"]["STRINGS"]["clean"] is True

    def test_strings_has_character_variable(self, db):
        """At least one variable in /STRINGS/ must be CHARACTER type."""
        decls = db.get("STRINGS", [])
        all_vars = [v for d in decls for v in d["variables"]]
        types = [v["type"].upper() for v in all_vars]
        assert "CHARACTER" in types, \
            f"Expected CHARACTER type in /STRINGS/, got: {types}"


# ══════════════════════════════════════════════════════════════════════════════
#  SECTION 11 — Test 10: Multiple blocks in one file — both CLEAN
# ══════════════════════════════════════════════════════════════════════════════

class TestTest10MultipleBlocks:

    def test_sim_state_is_clean(self, report):
        assert report["blocks"]["SIM_STATE"]["clean"] is True

    def test_physics_const_is_clean(self, report):
        assert report["blocks"]["PHYSICS_CONST"]["clean"] is True

    def test_both_blocks_from_same_file(self, report):
        """Both blocks must originate from test10_multiple_blocks.f90."""
        for block_name in ("SIM_STATE", "PHYSICS_CONST"):
            files = report["blocks"][block_name]["declared_in"]
            assert any("test10" in f for f in files), \
                f"/{block_name}/ should come from test10 file"


# ══════════════════════════════════════════════════════════════════════════════
#  SECTION 12 — Alignment check logic (unit tests, no JSON needed)
# ══════════════════════════════════════════════════════════════════════════════

from validator import check_alignment

class TestAlignmentLogic:
    """Unit tests for the alignment checker in isolation."""

    def _make_decl(self, block_name, variables):
        return {
            "source_file": "mock_file.f90",
            "block_name":  block_name,
            "total_size":  100,
            "saved":       False,
            "variables":   variables,
            "equivalences": [],
        }

    def test_aligned_real4_at_offset_0(self):
        """REAL*4 at offset 0 — perfectly aligned, no issue."""
        decl = self._make_decl("TEST", [
            {"name": "X", "type": "REAL", "kind": 4, "offset": 0,
             "size_bytes": 4, "dimensions": [], "char_len": 1}
        ])
        issues = check_alignment("TEST", [decl])
        assert issues == []

    def test_misaligned_real8_at_offset_4(self):
        """REAL*8 (kind=8) at offset 4 — misaligned, must be flagged."""
        decl = self._make_decl("TEST", [
            {"name": "Y", "type": "REAL", "kind": 8, "offset": 4,
             "size_bytes": 8, "dimensions": [], "char_len": 1}
        ])
        issues = check_alignment("TEST", [decl])
        assert len(issues) == 1
        assert "ALIGNMENT" in issues[0]
        assert "Y" in issues[0]

    def test_aligned_integer4_at_offset_4(self):
        """INTEGER*4 at offset 4 — aligned (4 % 4 == 0)."""
        decl = self._make_decl("TEST", [
            {"name": "N", "type": "INTEGER", "kind": 4, "offset": 4,
             "size_bytes": 4, "dimensions": [], "char_len": 1}
        ])
        issues = check_alignment("TEST", [decl])
        assert issues == []

    def test_misaligned_integer4_at_offset_3(self):
        """INTEGER*4 at offset 3 — misaligned (3 % 4 != 0)."""
        decl = self._make_decl("TEST", [
            {"name": "M", "type": "INTEGER", "kind": 4, "offset": 3,
             "size_bytes": 4, "dimensions": [], "char_len": 1}
        ])
        issues = check_alignment("TEST", [decl])
        assert len(issues) == 1
        assert "ALIGNMENT" in issues[0]


# ══════════════════════════════════════════════════════════════════════════════
#  SECTION 13 — Type punning logic (unit tests)
# ══════════════════════════════════════════════════════════════════════════════

from validator import check_type_punning

class TestTypePunningLogic:
    """Unit tests for the type punning checker in isolation."""

    def _decl(self, file, variables):
        return {
            "source_file": file,
            "variables": variables,
        }

    def _var(self, name, typ, kind, offset):
        return {"name": name, "type": typ, "kind": kind,
                "offset": offset, "size_bytes": kind,
                "dimensions": [], "char_len": 1}

    def test_real_vs_integer_at_offset_0(self):
        """REAL and INTEGER at same offset → TYPE PUNNING."""
        decls = [
            self._decl("a.f90", [self._var("X", "REAL",    4, 0)]),
            self._decl("b.f90", [self._var("I", "INTEGER", 4, 0)]),
        ]
        issues = check_type_punning("BLOCK", decls)
        assert any("TYPE PUNNING" in i for i in issues)

    def test_real_vs_real_same_kind_no_issue(self):
        """Two REALs of the same kind at the same offset → no issue."""
        decls = [
            self._decl("a.f90", [self._var("X", "REAL", 4, 0)]),
            self._decl("b.f90", [self._var("Y", "REAL", 4, 0)]),
        ]
        issues = check_type_punning("BLOCK", decls)
        assert not any("TYPE PUNNING" in i for i in issues)

    def test_real4_vs_real8_kind_mismatch(self):
        """REAL*4 vs REAL*8 at same offset → KIND MISMATCH."""
        decls = [
            self._decl("a.f90", [self._var("X", "REAL", 4, 0)]),
            self._decl("b.f90", [self._var("Y", "REAL", 8, 0)]),
        ]
        issues = check_type_punning("BLOCK", decls)
        assert any("KIND MISMATCH" in i for i in issues)

    def test_no_overlap_no_issue(self):
        """Variables at different offsets with different types → no issue."""
        decls = [
            self._decl("a.f90", [self._var("X", "REAL",    4, 0)]),
            self._decl("b.f90", [self._var("I", "INTEGER", 4, 4)]),
        ]
        issues = check_type_punning("BLOCK", decls)
        assert issues == []