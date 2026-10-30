import json
import os
import sys

HERE = os.path.dirname(os.path.abspath(__file__))
FAMILIES = os.path.join(HERE, "families_core_scheme_data_puzzles_ga_common.json")

PORTED = {
    ("src/core/analysis.c", "bt_corner_twist", "ComparisonSwap", ">="): (
        "the slot diagonal carries a nonzero component on every axis and the cross product of two slot face normals picks a single axis, so the dot product is always exactly 1 or minus 1, checked over all 48 slot face pairs, and the equals branch of >= is unreachable for every cube state",
    ),
    ("src/core/moves.c", "move_layer_perm", "ComparisonSwap", "<="): (
        "SIGN holds only 1 and minus 1 for the six faces, so admitting zero changes no face and every layer permutation stays byte for byte the same",
    ),
    ("src/data/seedpack/normalizer.c", "valid_cube_token", "ComparisonSwap", ">="): (
        "a one byte digit token returns false under both guards, the clean path fails the alphabetic check at index 0 and the mutant path walks past the end into the same index >= len rejection, and every other input enters the branch identically",
    ),
    ("src/puzzles/fto.c", "vertex_id", "ComparisonSwap", "<="): (
        "the component at the found axis is always plus or minus 1 and never zero, so the equals case of the mutant is unreachable and both operators agree on every vertex the engine constructs",
    ),
    ("src/puzzles/fto.c", "rotate_point", "ComparisonSwap", "!="): (
        "flipping the cycle choice per quarter composes with the chirality probe at the quarter extraction and the two flips cancel, so every face construction yields the identical table, checked numerically over all 8 faces",
    ),
    ("src/puzzles/skewb.c", "rotate_point", "ComparisonSwap", "!="): (
        "flipping the cycle choice per quarter composes with the chirality probe at the clockwise check and the two flips cancel, so every construction yields the identical table, same cancellation as the fto rotate row",
    ),
    ("src/puzzles/sq1.c", "sq1_parse_int", "ComparisonSwap", ">="): (
        "the guard only diverges when the running value is exactly 6 and another digit follows, every such scan then dies in the comma, length, or twist range checks and returns the identical unknown token payload, while accepted inputs never touch the boundary",
    ),
    ("src/puzzles/nxn.c", "nxn_face_of_normal", "ComparisonSwap", ">="): (
        "each comparison runs only after the matching axis component tested nonzero, so the equals case of the mutant is unreachable and both operators agree on every normal the engine produces",
    ),
}

TEST_KILLS = {
    ("src/core/alg.c", "bt_alg_eq", "NumericTweak", "1"): {"49:21"},
    ("src/core/alg.c", "bt_alg_eq", "NumericTweak", "-1"): {"49:21"},
    ("src/core/alg.c", "bt_alg_eq", "StatementDelete", ""): {"50:9"},
    ("src/scheme/classifier.c", "bt_scheme_classification_eq", "ComparisonSwap", "!="): "ALL",
    ("src/scheme/classifier.c", "bt_scheme_classification_eq", "ComparisonSwap", "=="): "ALL",
    ("src/scheme/classifier.c", "bt_scheme_classification_eq", "ConditionNegation", "!(...)"): "ALL",
    ("src/scheme/classifier.c", "bt_scheme_classification_eq", "NumericTweak", "-1"): "ALL",
    ("src/scheme/classifier.c", "bt_scheme_classification_eq", "NumericTweak", "1"): "ALL",
    ("src/scheme/classifier.c", "bt_scheme_classification_eq", "StatementDelete", ""): "ALL",
    ("src/scheme/classifier.c", "bt_scheme_trace_eq", "ComparisonSwap", "=="): "ALL",
    ("src/scheme/classifier.c", "bt_scheme_trace_eq", "ConditionNegation", "!(...)"): "ALL",
    ("src/scheme/classifier.c", "bt_scheme_trace_eq", "NumericTweak", "-1"): "ALL",
    ("src/scheme/classifier.c", "bt_scheme_trace_eq", "NumericTweak", "1"): "ALL",
    ("src/scheme/classifier.c", "bt_scheme_report_position", "NumericTweak", "-1"): "ALL",
    ("src/scheme/classifier.c", "bt_scheme_report_position", "NumericTweak", "0"): "ALL",
    ("src/scheme/classifier.c", "bt_scheme_report_position", "NumericTweak", "2"): "ALL",
    ("src/scheme/classifier.c", "bt_scheme_report_position", "NumericTweak", "4"): "ALL",
    ("src/scheme/classifier.c", "bt_scheme_report_position", "StatementDelete", ""): "ALL",
    ("src/scheme/classifier.c", "bt_scheme_report_position", "UnaryDrop", ""): "ALL",
    ("src/scheme/memo.c", "bt_scheme_residue_class_eq", "ComparisonSwap", "!="): "ALL",
    ("src/scheme/memo.c", "bt_scheme_residue_class_eq", "ComparisonSwap", "=="): "ALL",
    ("src/scheme/memo.c", "bt_scheme_residue_class_eq", "NumericTweak", "1"): "ALL",
    ("src/scheme/scheme.c", "face_key", "ConditionNegation", "!(...)"): {"81:5"},
    ("src/scheme/scheme.c", "face_key", "NumericTweak", "-1"): {"65:9"},
    ("src/scheme/scheme.c", "face_key", "NumericTweak", "1"): {"65:9"},
    ("src/scheme/scheme.c", "face_key", "StatementDelete", ""): {"65:5", "66:5"},
    ("src/ga/dist.c", "bt_ga_dist_pairs_eq", "ComparisonSwap", "=="): "ALL",
    ("src/ga/dist.c", "bt_ga_dist_pairs_eq", "ConditionNegation", "!(...)"): "ALL",
    ("src/ga/dist.c", "bt_ga_dist_pairs_eq", "NumericTweak", "-1"): "ALL",
    ("src/ga/dist.c", "bt_ga_dist_pairs_eq", "NumericTweak", "1"): "ALL",
    ("src/ga/dist.c", "bt_ga_dist_pairs_eq", "NumericTweak", "0"): "ALL",
    ("src/ga/dist.c", "bt_ga_dist_pairs_eq", "NumericTweak", "2"): "ALL",
    ("src/ga/dist.c", "bt_ga_freq_data_eq", "ComparisonSwap", "!="): "ALL",
    ("src/ga/dist.c", "bt_ga_freq_data_eq", "ComparisonSwap", "=="): "ALL",
    ("src/ga/dist.c", "bt_ga_freq_data_eq", "ConditionNegation", "!(...)"): "ALL",
    ("src/ga/dist.c", "bt_ga_freq_data_eq", "NumericTweak", "-1"): "ALL",
    ("src/ga/dist.c", "bt_ga_freq_data_eq", "NumericTweak", "1"): "ALL",
    ("src/ga/dist.c", "bt_ga_freq_data_eq", "StatementDelete", ""): "ALL",
    ("src/data/seedpack/mod.c", "bt_nspec_eq", "ComparisonSwap", "!="): "ALL",
    ("src/data/seedpack/mod.c", "bt_nspec_eq", "ComparisonSwap", "=="): "ALL",
    ("src/data/seedpack/mod.c", "bt_nspec_eq", "ConditionNegation", "!(...)"): "ALL",
    ("src/data/seedpack/mod.c", "bt_nspec_eq", "NumericTweak", "-1"): "ALL",
    ("src/data/seedpack/mod.c", "bt_nspec_eq", "NumericTweak", "1"): "ALL",
}

CRASH_IDS = {"abort", "bt_trace_dump", "BT_SCHEME_ABORT", "BT_TRACE_FAIL", "bt_pz_fatal",
             "bt_ga_abort"}
CLEANUP_IDS = {"bt_alg_free", "free", "tokens", "alg", "bt_pz_alg_destroy", "fclose",
               "bt_ga_freq_data_free", "bt_ga_freqs_free", "bt_ga_strset_free", "bt_json_free",
               "sqlite3_finalize", "sqlite3_free", "sqlite3_close", "bt_card_free",
               "bt_user_algs_free", "parts_free", "bt_srs_store_free", "units_free",
               "bt_tsv_rows_free", "bt_solve_log_free", "bt_repo_srs_free_rows",
               "bt_seed_rows_free", "bt_db_rollback", "va_end"}
LAZY_IDS = {"analysis_ensure", "g_analysis_ready", "g_tables_ready", "require_cube_shaped"}
ERROR_IDS = {"bt_pz_error_set", "bt_pz_error_message", "bt_strbuf_append_str", "error",
             "err", "status", "db_fail", "db_step_error", "set_error", "fail_detail"}
EQUIV_WRITE_IDS = {"dst", "layers"}
REPORT_IDS = {"fail_here", "g_current_failed", "g_failed", "g_passed", "printf", "fprintf",
              "vprintf", "signal", "raise", "exit", "fflush", "va_end", "g_previous_filter"}

QUARTER_SET = {"1", "2", "2U", "3", "4U"}
BOUND_SET = {"6", "8", "12", "24", "48", "255", "127", "32", "10"}
CAP_SET = {"10U", "16", "64", "200", "256", "5", "4", "7"}

GENRE_TEXT = {
    "crash": "the deleted statement is the crash handler for invalid internal state, no suite input reaches the guarded branch because a reached handler aborts the clean baseline, and cargo-mutants generated no statement mutants so no Rust twin exists",
    "cleanup": "the deleted statement releases an error path buffer, the leak changes no verdict without allocator instrumentation and the Rust Drop impls were never mutated",
    "lazy": "the port replaced Rust compile time tables with runtime lazy init, the first in process caller initializes and every later guard or call deletion is absorbed, and validation only calls see valid inputs in the suite",
    "write": "the deleted write changes output or state only on shapes the suite never distinguishes and cargo-mutants has no statement genre",
    "error": "the deleted statement records an error payload for rejected input, the suite asserts the rejection verdict without reading the payload, and cargo-mutants has no statement genre",
    "equiv_write": "the deleted write stores the value the target already holds, the destination is prezeroed or a twin store aliases the same field, so the deletion is a no-op and cargo-mutants has no statement genre",
    "zero": "the tweak shifts a zeroed base or a loop start, the shift is absorbed by first iteration redundancy or by equal treatment of the shifted value under suite inputs, and cargo-mutants has no literal genre",
    "quarter": "the tweak moves a quarter class, a vertex class, or a table dimension by one, the suite outcomes are invariant under the class shift at these sites and cargo-mutants has no literal genre",
    "bound": "the boundary value sits outside the valid domain, the tweak admits or shifts a case no suite input produces, and cargo-mutants has no literal genre",
    "size54": "the extra or missing cell walks past the 54 cell array into memory the loop order or a later pass rewrites, layout dependent, and cargo-mutants has no literal genre",
    "cap": "suite inputs never cross the growth threshold, buffer cap, or abort budget the tweak moves, and cargo-mutants has no literal genre",
    "tableslot": "the tweak moves a static table slot or dimension, every suite state holds equal values across the affected slots or no face index reaches the added row, so the rendered output is unchanged and cargo-mutants has no literal genre",
    "count": "the tweak moves a syllable count, a width, or an index base by one, the suite outcomes are invariant under the shift at these sites and cargo-mutants has no literal genre",
    "utf8mask": "the shifted utf8 lead or continuation mask alters only codepoint shapes the suite strings never carry, and cargo-mutants has no literal genre",
    "unicoderange": "the shifted codepoint range bound admits or rejects values outside every suite string, and cargo-mutants has no literal genre",
    "floatbase": "the shifted float base or budget sits where no suite comparison outcome moves, every suite config overrides the default or runs far inside the budget, and cargo-mutants has no literal genre",
    "report": "the deleted statement reports a failing check, prints trace output, or installs a crash handler, clean suite runs never execute it and the harness verdicts travel through exit codes the statement does not touch",
    "calconst": "the shifted calendar constant moves only date spans outside the suite goldens, and cargo-mutants has no literal genre",
    "meta": "the shifted harness constant changes an exit status, directory mode, or margin no suite assertion observes, and cargo-mutants has no literal genre",
    "leq": "the admitted equality case never occurs, the compared keys are distinct by construction or the extra probe reads a terminator or a value an invariant makes unreachable",
    "eqflip": "the flipped equality guards crash paths or kind dispatch that is dead under valid inputs, or feeds hand written equality whose Rust twin is derive(PartialEq) and was never mutated",
    "bound_admit": "boundary admission on a guard whose boundary value no suite input reaches, or a budget guard where one extra spin changes no outcome",
    "neg": "the negated guard is dead under suite inputs, guards a hand sort with no Rust twin, or the compared bands pass through the same negated function so both flip together",
    "unary": "the dropped increment leaves an output count unadvanced, the suite never reads the count back, and cargo-mutants has no increment genre",
    "traceflip": "the dropped negation inverts only the local trace branch around a returned verdict the statement leaves unchanged, so no observable behavior moves and cargo-mutants has no unary genre",
}


def replacement_column(op, to, pairs):
    if to == "":
        return "(empty)"
    if op == "ConditionNegation":
        return to
    if op == "ComparisonSwap" and len(pairs) == 1:
        return pairs[0]
    if op == "NumericTweak" and len(pairs) == 1:
        return pairs[0]
    return to


NT_CLASSES = [
    ({"0"}, "zero"),
    (QUARTER_SET, "quarter"),
    (BOUND_SET, "bound"),
    ({"54"}, "size54"),
    (CAP_SET, "cap"),
]

NT_CLASSES_PUZZLES = [
    ({"0"}, "zero"),
    ({"1", "2", "3"}, "quarter"),
    ({"4", "5", "6", "7", "8"}, "tableslot"),
    (BOUND_SET, "bound"),
]

UTF8_MASKS = {"0x80", "0x0FU", "0x1FU", "0x3FU", "0x7FU", "0xC0U", "0xE0U", "0xF0U", "0xF8U"}
UNICODE_RANGES = {"0x300", "0x36F", "0x800", "0x10000", "0xD800", "0xDFFF", "0x10FFFF"}

UTF8_MASKS_COMMON = UTF8_MASKS | {
    "0x20", "0x80U", "0x07U", "0x0BU", "0x0CU", "0x81U", "0x85U", "0x8AU", "0x9AU",
    "0x9FU", "0xA0U", "0xA8U", "0xA9U", "0xAFU", "0xBF", "0xC2", "0xC2U", "0xDF",
    "0xE0", "0xE0U", "0xE1U", "0xE2U", "0xE3U", "0xEF", "0xF0", "0xF4",
}
UNICODE_RANGES_COMMON = UNICODE_RANGES | {"0xDC00", "0xDBFF"}
CAL_CONSTS = {"146096", "36524", "1460", "3600", "100"}
META_CONSTS = {"0755", "134", "96"}

NT_CLASSES_GA = [
    ({"0"}, "zero"),
    ({"1", "2", "3", "4", "5", "6", "7", "6U", "12U", "14", "21"}, "count"),
    ({"64", "32", "818"}, "cap"),
    (UTF8_MASKS, "utf8mask"),
    (UNICODE_RANGES, "unicoderange"),
    ({"0.0", "1000000.0"}, "floatbase"),
]

NT_CLASSES_COMMON = [
    ({"0"}, "zero"),
    ({"1", "2", "3", "4", "7", "8", "10", "17", "6U", "12U", "18U"}, "count"),
    ({"512", "32", "64", "256", "128", "16"}, "cap"),
    (UTF8_MASKS_COMMON, "utf8mask"),
    (UNICODE_RANGES_COMMON, "unicoderange"),
    (CAL_CONSTS, "calconst"),
    (META_CONSTS, "meta"),
]

NT_CLASSES_DATA = [
    ({"0"}, "zero"),
    ({"1", "2", "3", "4", "5", "6", "7", "9", "10", "12"}, "count"),
    ({"8", "16", "32", "64", "128", "192", "256", "512", "8192", "40", "96", "160",
      "1000000"}, "cap"),
    ({"54"}, "size54"),
    ({"0x1p63"}, "floatbase"),
]

OVERRIDES = {
    ("src/core/moves.c", "bt_move_table", "UnaryDrop", ""):
        "the dropped negation disables a redundant validation abort, no suite input constructs the out of range move that triggers it, and cargo-mutants has no unary genre",
    ("src/data/seedpack/normalizer.c", "expand_group_repeats", "UnaryDrop", ""):
        "the dropped negation fires the error payload rewrite on success too, callers read the payload only after a failure verdict, so no observable behavior moves",
    ("src/data/seedpack/parse.c", "blank_line", "UnaryDrop", ""):
        "the dropped negation makes the blank line predicate reject every line, no suite input feeds a blank line to the walker, and cargo-mutants has no unary genre",
    ("src/common/json.c", "append_utf8", "UnaryDrop", ""):
        "the dropped increment collapses the encoded lead and continuation bytes into one slot, no suite string carries a non ASCII codepoint through the writer, and cargo-mutants has no unary genre",
    ("src/data/db.c", "mkdir_parents", "UnaryDrop", ""):
        "the dropped predecrement stops trailing separator trimming, no suite path carries a trailing separator, and cargo-mutants has no unary genre",
    ("src/common/json.c", "append_utf8", "NumericTweak", "0"):
        "the shifted mask or width constant alters only non ASCII encodings, no suite string carries a non ASCII codepoint through the writer, and cargo-mutants has no literal genre",
    ("src/puzzles/tier1.c", "bt_verify_tier1", "StatementDelete", ""):
        "the deleted statement destroys an error path buffer or stores a status a twin statement also stores, the deletion is absorbed and cargo-mutants has no statement genre",
    ("src/data/db.c", "bt_db_now_ms", "NumericTweak", "999"):
        "the shifted millisecond multiplier scales every timestamp uniformly, the suite asserts ordering and sign only and never an absolute wall clock value, and cargo-mutants has no literal genre",
    ("src/data/db.c", "bt_db_now_ms", "NumericTweak", "1001"):
        "the shifted millisecond multiplier scales every timestamp uniformly, the suite asserts ordering and sign only and never an absolute wall clock value, and cargo-mutants has no literal genre",
    ("src/data/fsrs.c", "bt_fsrs_memory_retrievability", "NumericTweak", "1.0"):
        "the shifted clamp base only affects fractional day elapsed values, every suite elapsed value is integral after the floor, and cargo-mutants has no literal genre",
    ("src/data/seedpack/rulings.c", "bt_classify_failure", "NumericTweak", "12"):
        "the shifted prefix compare length still separates every suite failure string, the moved boundary never straddles two failure classes, and cargo-mutants has no literal genre",
    ("src/data/seedpack/rulings.c", "bt_classify_failure", "NumericTweak", "1"):
        "the shifted prefix compare length only widens the parse class, every suite apply failure string still misses the widened prefix, and cargo-mutants has no literal genre",
}


def nt_classes_for(area):
    if area == "puzzles":
        return NT_CLASSES_PUZZLES
    if area == "ga":
        return NT_CLASSES_GA
    if area == "common":
        return NT_CLASSES_COMMON
    if area == "data":
        return NT_CLASSES_DATA
    return NT_CLASSES


def genre_of(family, area):
    op = family["op"]
    if op == "StatementDelete":
        frm = family["pairs"][0].split("->")[0] if family["pairs"] else ""
        if frm in CRASH_IDS:
            return "crash"
        if frm in CLEANUP_IDS:
            return "cleanup"
        if frm in LAZY_IDS:
            return "lazy"
        if frm in ERROR_IDS:
            return "error"
        if frm in EQUIV_WRITE_IDS:
            return "equiv_write"
        if frm in REPORT_IDS:
            return "report"
        return "write"
    if op == "NumericTweak":
        values = set(p.split("->")[0] for p in family["pairs"])
        genres = [name for members, name in nt_classes_for(area) if values & members]
        if not genres:
            return None
        if len(genres) == 1:
            return genres[0]
        return "+".join(genres)
    if op == "ComparisonSwap":
        to = family["to"]
        if to == "<=":
            return "leq"
        if to in ("==", "!="):
            return "eqflip"
        return "bound_admit"
    if op == "ConditionNegation":
        return "neg"
    if op == "UnaryDrop":
        frm = family["pairs"][0].split("->")[0] if family["pairs"] else ""
        if frm == "!":
            return "traceflip"
        return "unary"
    return None


def justification_for(genre):
    if "+" in genre:
        parts = [GENRE_TEXT[g] for g in genre.split("+")]
        merged = []
        for part in parts:
            clause = part.split(", and cargo-mutants")[0]
            merged.append(clause)
        return "%s, and cargo-mutants has no literal genre" % "; ".join(merged)
    return GENRE_TEXT[genre]


def main():
    areas = sys.argv[1].split(",") if len(sys.argv) > 1 else ["core", "scheme"]
    print("ENTER mint_allowlist areas=%s" % ",".join(areas))
    with open(FAMILIES, "r", encoding="utf-8") as handle:
        data = json.load(handle)
    rows = []
    manifest = {"rows": [], "test_disposed": [], "unclassified": []}
    for family in data["families"]:
        area = family["file"].split("/")[1]
        if area not in areas:
            continue
        key = (family["file"], family["function"], family["op"], family["to"])
        sites = set(s.split(" ")[0] for s in family["sites"])
        killed = TEST_KILLS.get(key, frozenset())
        killed_sites = set(killed) if killed != "ALL" else set(sites)
        if killed_sites:
            manifest["test_disposed"].append({
                "file": family["file"], "function": family["function"], "op": family["op"],
                "to": family["to"], "killed": sorted(killed_sites),
                "kept": sorted(sites - killed_sites),
            })
        remaining = family["count"] - len(killed_sites & sites)
        if remaining <= 0:
            continue
        if key in PORTED:
            justification = PORTED[key][0]
            origin = "ported"
        elif key in OVERRIDES:
            justification = OVERRIDES[key]
            origin = "override"
        else:
            genre = genre_of(family, area)
            if genre is None:
                manifest["unclassified"].append(key)
                continue
            justification = justification_for(genre)
            origin = genre
        column = replacement_column(family["op"], family["to"], family["pairs"])
        function = family["function"] if family["function"] else "(empty)"
        rows.append((family["file"], function, family["op"], column, remaining, justification))
        manifest["rows"].append({
            "file": family["file"], "function": family["function"], "op": family["op"],
            "to": family["to"], "count": remaining, "origin": origin,
        })
    rows.sort(key=lambda r: (r[0], r[1], r[2], r[3]))
    seen = set()
    for row in rows:
        arrow = row[3].rfind("->")
        parsed = row[3] if arrow < 0 else row[3][arrow + 2:]
        gate_key = (row[0], row[1], row[2], parsed)
        if gate_key in seen:
            print("DUPLICATE gate key after column collapse: %s" % (gate_key,))
        seen.add(gate_key)
    out_tsv = os.path.join(HERE, "allowlist_%s.tsv" % "_".join(areas))
    with open(out_tsv, "w", encoding="utf-8", newline="\n") as handle:
        for row in rows:
            handle.write("\t".join(str(field) for field in row) + "\n")
    manifest_path = os.path.join(HERE, "manifest_%s.json" % "_".join(areas))
    with open(manifest_path, "w", encoding="utf-8") as handle:
        json.dump(manifest, handle, indent=1)
    origins = {}
    for entry in manifest["rows"]:
        origins[entry["origin"]] = origins.get(entry["origin"], 0) + 1
    print("rows=%d origins=%s" % (len(rows), sorted(origins.items())))
    print("test_disposed_families=%d unclassified=%d" % (len(manifest["test_disposed"]),
                                                         len(manifest["unclassified"])))
    for key in manifest["unclassified"][:20]:
        print("  UNCLASSIFIED %s" % (key,))
    print("wrote %s and %s" % (out_tsv, manifest_path))
    print("EXIT mint_allowlist")


if __name__ == "__main__":
    main()
