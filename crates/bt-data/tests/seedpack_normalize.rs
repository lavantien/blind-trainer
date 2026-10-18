mod common;

use bt_data::seedpack::{SeedRow, load_pack, load_registry, normalize_alg};
use std::path::PathBuf;

fn real_packs() -> PathBuf {
    common::packs()
}

#[test]
fn normalize_strips_grouping_parens() {
    let out = normalize_alg("3bld", "(L' U) (R U') (L U2') (R' U) (R U2 R')").expect("normalize");
    assert_eq!(out.alg, "L' U R U' L U2' R' U R U2 R'");
    assert!(out.notes.is_empty());
}

#[test]
fn normalize_expands_group_repeat_2() {
    let out = normalize_alg("skewb", "(y2) (F' L F L')2").expect("normalize");
    assert_eq!(out.alg, "y2 F' L F L' F' L F L'");
}

#[test]
fn normalize_expands_group_repeat_3() {
    let out = normalize_alg("3x3", "(U R U' R')3").expect("normalize");
    assert_eq!(out.alg, "U R U' R' U R U' R' U R U' R'");
}

#[test]
fn normalize_expands_bracket_star_repeats() {
    let out = normalize_alg("megaminx", "[(R U R' U) (R' U' R2 U') (R' U R' U R) U] *2")
        .expect("normalize");
    assert_eq!(
        out.alg,
        "R U R' U R' U' R2 U' R' U R' U R U R U R' U R' U' R2 U' R' U R' U R U"
    );
}

#[test]
fn normalize_splits_arrow_chains_into_a_note() {
    let out = normalize_alg("skewb", "R' F R F' -> cat. 1").expect("normalize");
    assert_eq!(out.alg, "R' F R F'");
    assert_eq!(out.notes, vec!["chain note: -> cat. 1".to_string()]);
}

#[test]
fn normalize_keeps_parens_through_non_sq1_auf_shaped_groups() {
    let out = normalize_alg("3x3", "R2 U R' U R' U' R U' R2 (U' D) R' U R D'").expect("normalize");
    assert_eq!(out.alg, "R2 U R' U R' U' R U' R2 U' D R' U R D'");
    assert!(out.notes.is_empty());
}

#[test]
fn normalize_converts_separating_commas_to_whitespace() {
    let out = normalize_alg("bigcube", "r2, U2, x").expect("normalize");
    assert_eq!(out.alg, "r2 U2 x");
}

#[test]
fn normalize_maps_the_literal_x_rotation() {
    let out = normalize_alg(
        "bigcube",
        "Rw U2, X, Rw U2, Rw U2 , Rw' U2, Lw U2, Rw' U2, Rw U2, Rw' U2, Rw'",
    )
    .expect("normalize");
    assert_eq!(
        out.alg,
        "Rw U2 x Rw U2 Rw U2 Rw' U2 Lw U2 Rw' U2 Rw U2 Rw' U2 Rw'"
    );
    assert_eq!(out.notes, vec!["mapped X to x".to_string()]);
}

#[test]
fn normalize_moves_pure_prose_to_a_note() {
    let out = normalize_alg("3bld", "apply swap directly, no setup").expect("normalize");
    assert_eq!(out.alg, "");
    assert_eq!(
        out.notes,
        vec!["prose alg: apply swap directly, no setup".to_string()]
    );
}

#[test]
fn normalize_wraps_bare_sq1_tuples() {
    let out = normalize_alg("sq1", "/ 3,3 /").expect("normalize");
    assert_eq!(out.alg, "/ (3,3) /");
    let out = normalize_alg("sq1", "/ 3,0 / -1,1").expect("normalize");
    assert_eq!(out.alg, "/ (3,0) / (-1,1)");
}

#[test]
fn normalize_strips_sq1_auf_prefixes_with_a_note() {
    let out = normalize_alg("sq1", "(U) / (3,3) / (0,3)").expect("normalize");
    assert_eq!(out.alg, "/ (3,3) / (0,3)");
    assert_eq!(out.notes, vec!["auf prefix stripped: (U)".to_string()]);
    let out = normalize_alg("sq1", "(U2 D') (0,1) / (3,0)").expect("normalize");
    assert_eq!(out.alg, "(0,1) / (3,0)");
    assert_eq!(out.notes, vec!["auf prefix stripped: (U2 D')".to_string()]);
    let out = normalize_alg("sq1", "(U D)(0,-1) / (3,0)").expect("normalize");
    assert_eq!(out.alg, "(0,-1) / (3,0)");
    assert_eq!(out.notes, vec!["auf prefix stripped: (U D)".to_string()]);
}

#[test]
fn normalize_strips_a_trailing_sq1_equator_marker_with_a_note() {
    let out = normalize_alg("sq1", "(1,0) / (5,0) *").expect("normalize");
    assert_eq!(out.alg, "(1,0) / (5,0)");
    assert_eq!(out.notes, vec!["equator marker stripped: *".to_string()]);
}

#[test]
fn normalize_spaces_fused_sq1_slashes() {
    let out = normalize_alg("sq1", "(1,0)/(-1,0)").expect("normalize");
    assert_eq!(out.alg, "(1,0) / (-1,0)");
}

#[test]
fn normalize_rejects_unknown_tokens_and_broken_structure() {
    let err = normalize_alg("3x3", "R U swap").expect_err("mixed prose must fail");
    assert!(err.to_string().contains("swap"), "err: {err}");
    assert!(normalize_alg("3x3", "R U ?").is_err());
    assert!(normalize_alg("3x3", "R w").is_err());
    assert!(normalize_alg("3x3", "R U ->").is_err());
    assert!(normalize_alg("3x3", "(R U").is_err());
    assert!(normalize_alg("3x3", "R U)").is_err());
    assert!(normalize_alg("3x3", "(R U)x").is_err());
    assert!(normalize_alg("3x3", "(((R)99)99)99").is_err());
    assert!(normalize_alg("sq1", "(1,0) (U D) /").is_err());
    assert!(normalize_alg("sq1", "(x,y)").is_err());
    assert!(normalize_alg("sq1", "(0,1,2)").is_err());
    assert!(normalize_alg("sq1", "(01,0)").is_err());
}

const MEGA_PINNED_TOKENS: [&str; 16] = [
    "U", "U'", "U2", "U2'", "R", "R'", "R2", "R2'", "L", "L'", "F", "F'", "BR", "BR'", "y", "y'",
];

fn pack_rows(file: &str) -> Vec<SeedRow> {
    let root = real_packs();
    let entry = load_registry(&root)
        .expect("registry")
        .into_iter()
        .find(|entry| entry.file == file)
        .unwrap_or_else(|| panic!("registry holds {file}"));
    load_pack(&root, &entry).expect("pack loads")
}

fn raw_alg(file: &str, key: &str) -> String {
    let text = std::fs::read_to_string(real_packs().join(file)).expect("pack file");
    for line in text.lines().skip(1) {
        let cols: Vec<&str> = line.trim_end_matches('\r').split('\t').collect();
        if cols.first() == Some(&key) {
            return cols[1].to_string();
        }
    }
    panic!("{key} not found in {file}");
}

fn nxn_engine_token(token: &str) -> bool {
    let bytes = token.as_bytes();
    let mut index = 0;
    let digit_prefixed = bytes.len() > 1 && (b'2'..=b'7').contains(&bytes[0]);
    if digit_prefixed {
        index = 1;
    }
    let Some(&head) = bytes.get(index) else {
        return false;
    };
    index += 1;
    let upper_face = b"UDLFRB".contains(&head);
    let lower_face = b"udfrlb".contains(&head);
    if upper_face && bytes.get(index) == Some(&b'w') {
        index += 1;
    }
    let bare_head = matches!(head, b'M' | b'E' | b'S' | b'x' | b'y' | b'z');
    if !upper_face && !lower_face && !bare_head {
        return false;
    }
    if bare_head && digit_prefixed {
        return false;
    }
    let (mut prime, mut two) = (false, false);
    while index < bytes.len() {
        match bytes[index] {
            b'\'' if !prime => prime = true,
            b'2' if !two => two = true,
            _ => return false,
        }
        index += 1;
    }
    true
}

#[test]
fn sq1_fused_chains_normalize_to_the_engine_canonical_spacing() {
    for (key, file, expected) in [
        ("co.01", "sq1_co.tsv", "(1,0) / (3,0) / (-1,0)"),
        ("co.03", "sq1_co.tsv", "(1,0) / (-1,0)"),
        (
            "ep.90",
            "sq1_ep.tsv",
            "/ (0,3) / (1,0) / (2,-2) / (4,0) / (4,0) / (-3,2) / (-1,0) / (0,-3) / (4,0) / (-1,2) / (3,-2)",
        ),
    ] {
        let raw = raw_alg(file, key);
        assert!(
            raw.contains(")/"),
            "{key} keeps a fused slash in the source"
        );
        let out = normalize_alg("sq1", &raw).expect("normalize");
        assert_eq!(out.alg, expected, "{key}");
        let fixpoint = normalize_alg("sq1", &out.alg).expect("refixpoint");
        assert_eq!(fixpoint.alg, expected, "{key} normalizes to a fixpoint");
        assert!(fixpoint.notes.is_empty(), "{key} fixpoint adds no notes");
        let stored = pack_rows(file)
            .into_iter()
            .find(|row| row.case_key == key)
            .expect("row");
        assert_eq!(stored.alg, expected, "{key} stored alg");
    }
}

#[test]
fn sq1_hazard_tails_stay_verbatim_in_loader_notes() {
    let ep90 = pack_rows("sq1_ep.tsv")
        .into_iter()
        .find(|row| row.case_key == "ep.90")
        .expect("ep.90");
    assert_eq!(
        ep90.front_json,
        "{\"case\":\"W on D\",\"loader_notes\":[\"equator marker stripped: *\",\"auf prefix stripped: (D')\"],\"parity\":true}"
    );
}

const SQ1_TURN_MIN: i64 = -5;
const SQ1_TURN_MAX: i64 = 6;

fn sq1_engine_token(token: &str) -> bool {
    if token == "/" {
        return true;
    }
    let Some(inner) = token
        .strip_prefix('(')
        .and_then(|rest| rest.strip_suffix(')'))
    else {
        return false;
    };
    let Some((top, bottom)) = inner.split_once(',') else {
        return false;
    };
    let (Ok(top), Ok(bottom)) = (top.parse::<i64>(), bottom.parse::<i64>()) else {
        return false;
    };
    let in_range = |turn: i64| (SQ1_TURN_MIN..=SQ1_TURN_MAX).contains(&turn);
    in_range(top) && in_range(bottom) && (top != 0 || bottom != 0)
}

#[test]
fn sq1_pack_rows_stay_inside_the_engine_twist_domain() {
    for file in [
        "sq1_cubeshape.tsv",
        "sq1_co.tsv",
        "sq1_eo.tsv",
        "sq1_cp.tsv",
        "sq1_ep.tsv",
    ] {
        for row in pack_rows(file) {
            assert!(!row.alg.is_empty(), "{} holds a twist core", row.case_key);
            assert!(
                !row.alg.contains(['*', 'U', 'D']),
                "{} leaks a pack hazard into the stored alg: {}",
                row.case_key,
                row.alg
            );
            for token in row.alg.split_whitespace() {
                assert!(
                    sq1_engine_token(token),
                    "{} token {} leaves the engine twist domain",
                    row.case_key,
                    token
                );
            }
        }
    }
}

#[test]
fn sq1_minus_six_turns_wrap_to_six_with_a_verbatim_note() {
    let out = normalize_alg("sq1", "(1,0) / (0,-6)").expect("wrap");
    assert_eq!(out.alg, "(1,0) / (0,6)");
    assert_eq!(out.notes, vec!["wraparound: (0,-6) -> (0,6)".to_string()]);
    let bare = normalize_alg("sq1", "/ -6,0 /").expect("bare wrap");
    assert_eq!(bare.alg, "/ (6,0) /");
    assert_eq!(bare.notes, vec!["wraparound: (-6,0) -> (6,0)".to_string()]);
    let ep96 = pack_rows("sq1_ep.tsv")
        .into_iter()
        .find(|row| row.case_key == "ep.96")
        .expect("ep.96");
    assert_eq!(
        ep96.alg,
        "/ (0,-3) / (3,0) / (0,-3) / (0,-3) / (1,-2) / (-2,0) / (-4,2) / (4,-2) / (0,-2) / (-2,1) / (3,3) / (0,6)"
    );
    assert_eq!(
        ep96.front_json,
        "{\"case\":\"H-W\",\"loader_notes\":[\"auf prefix stripped: (D2)\",\"wraparound: (0,-6) -> (0,6)\"],\"parity\":true}"
    );
}

#[test]
fn sq1_rejects_turns_the_engine_can_never_parse() {
    for text in [
        "(0,0)", "(7,0)", "(0,7)", "(1,-7)", "(-7,0)", "(6,7)", "(0,-7)", "(8,1)", "0,0", "7,-5",
    ] {
        assert!(
            normalize_alg("sq1", text).is_err(),
            "{text} must fail the loader"
        );
    }
    assert!(normalize_alg("sq1", "(6,6)").is_ok());
    assert!(normalize_alg("sq1", "(-5,-5)").is_ok());
}

#[test]
fn megaminx_4lll_33_bracket_repeat_expands_into_the_pinned_engine_tokens() {
    let raw = raw_alg("megaminx_4lll.tsv", "4lll.33");
    assert!(
        raw.contains('[') && raw.contains("*2"),
        "source holds the bracket form"
    );
    let row = pack_rows("megaminx_4lll.tsv")
        .into_iter()
        .find(|row| row.case_key == "4lll.33")
        .expect("4lll.33");
    assert_eq!(
        row.alg,
        "R U R' U R' U' R2 U' R' U R' U R U R U R' U R' U' R2 U' R' U R' U R U"
    );
    for token in row.alg.split_whitespace() {
        assert!(
            MEGA_PINNED_TOKENS.contains(&token),
            "4lll.33 token {token} leaves the pinned mega grammar"
        );
    }
    assert!(!row.front_json.contains("loader_notes"));
    let fixpoint = normalize_alg("megaminx", &row.alg).expect("refixpoint");
    assert_eq!(fixpoint.alg, row.alg);
}

#[test]
fn megaminx_pack_normalizes_inside_the_pinned_engine_grammar() {
    let rows = pack_rows("megaminx_4lll.tsv");
    assert_eq!(rows.len(), 39);
    for row in &rows {
        for token in row.alg.split_whitespace() {
            assert!(
                MEGA_PINNED_TOKENS.contains(&token),
                "{} token {} leaves the pinned mega grammar",
                row.case_key,
                token
            );
        }
    }
}

#[test]
fn bigcube_parity_oll4_maps_the_literal_x_rotation_into_the_nxn_grammar() {
    assert!(raw_alg("bigcube_parity.tsv", "parity.oll4").contains('X'));
    let row = pack_rows("bigcube_parity.tsv")
        .into_iter()
        .find(|row| row.case_key == "parity.oll4")
        .expect("parity.oll4");
    assert_eq!(
        row.alg,
        "Rw U2 x Rw U2 Rw U2 Rw' U2 Lw U2 Rw' U2 Rw U2 Rw' U2 Rw'"
    );
    assert!(!row.alg.contains('X'));
    assert!(row.front_json.contains("mapped X to x"));
}

#[test]
fn bigcube_packs_normalize_inside_the_nxn_engine_grammar() {
    assert!(!nxn_engine_token("X"));
    assert!(!nxn_engine_token("wR"));
    assert!(!nxn_engine_token("rw"));
    assert!(!nxn_engine_token("2M"));
    assert!(nxn_engine_token("3Rw'"));
    assert!(nxn_engine_token("Rw2'"));
    assert!(nxn_engine_token("u2"));
    for file in ["bigcube_parity.tsv", "bigcube_l2e.tsv"] {
        for row in pack_rows(file) {
            for token in row.alg.split_whitespace() {
                assert!(
                    nxn_engine_token(token),
                    "{} token {} leaves the nxn grammar",
                    row.case_key,
                    token
                );
            }
        }
    }
}

#[test]
fn group_repeat_rows_expand_from_the_real_packs() {
    let f2l = pack_rows("cfop_f2l.tsv");
    for (slot, face) in [("fr", "R"), ("fl", "F"), ("br", "B"), ("bl", "L")] {
        let key = format!("f2l.{slot}.29");
        assert!(
            raw_alg("cfop_f2l.tsv", &key).contains(")3"),
            "{key} source form"
        );
        let row = f2l.iter().find(|row| row.case_key == key).expect(&key);
        let group = format!("U {face} U' {face}'");
        assert_eq!(row.alg, format!("{group} {group} {group}"), "{key}");
        assert!(!row.front_json.contains("loader_notes"), "{key}");
    }
    let sarah = pack_rows("skewb_sarah.tsv")
        .into_iter()
        .find(|row| row.case_key == "sarah.05")
        .expect("sarah.05");
    assert_eq!(sarah.alg, "y2 F' L F L' F' L F L'");
}
#[test]
fn normalize_rejects_doubled_suffixes_and_mixed_case_letter_pairs() {
    for alg in ["R U Ru", "R U aU", "R U R''", "R U R22"] {
        let err = normalize_alg("3x3", alg).expect_err("token shape must fail");
        assert!(err.to_string().contains("unknown token"), "err: {err}");
    }
}

#[test]
fn normalize_expands_a_bracket_repeat_at_exactly_the_limit() {
    let out = normalize_alg("3x3", "[R]*99").expect("repeat at the limit must pass");
    assert_eq!(out.alg, vec!["R"; 99].join(" "));
}

#[test]
fn normalize_pins_the_bracket_expand_length_boundary_as_a_token_error() {
    let content = "R".repeat(9090);
    let alg = format!("[{content}]*11");
    let err =
        normalize_alg("3x3", &alg).expect_err("boundary expansion must fail as a token error");
    assert!(err.to_string().contains("unknown token"), "err: {err}");
}

#[test]
fn normalize_rejects_a_repeat_digit_on_a_nested_group() {
    let err = normalize_alg("3x3", "(R U (R2))2").expect_err("nested group repeat must fail");
    let shown = err.to_string();
    assert!(shown.contains("unknown token"), "err: {shown}");
    assert!(shown.contains("R22"), "err: {shown}");
}

#[test]
fn normalize_rejects_a_bare_digit_token() {
    for (alg, token) in [("R 2", "2"), ("R 7", "7"), ("R 8", "8"), ("2R 2", "2")] {
        let err = normalize_alg("3x3", alg).expect_err("a bare digit is not a turn");
        let shown = err.to_string();
        assert!(
            shown.contains(&format!("unknown token '{token}'")),
            "err: {shown}"
        );
    }
    let out = normalize_alg("3x3", "2R").expect("an inner slice digit prefix stays a turn");
    assert_eq!(out.alg, "2R");
}

#[test]
fn normalize_rejects_a_nested_bracket_repeat() {
    for alg in ["[[R U]*2]*2", "[[R]*2", "[[R U]*2]"] {
        let err =
            normalize_alg("3x3", alg).expect_err("a bracket inside bracket content is malformed");
        let shown = err.to_string();
        assert!(shown.contains("malformed repeat marker"), "err: {shown}");
        assert!(shown.contains(alg), "err: {shown}");
    }
}

#[test]
fn normalize_expands_nested_group_repeats_innermost_first() {
    let out = normalize_alg("3x3", "((R U)2)2").expect("the inner group binds its digit first");
    assert_eq!(out.alg, "R U R U R U R U");
    assert!(out.notes.is_empty());
}

#[test]
fn normalize_rejects_a_zero_bracket_repeat() {
    let err = normalize_alg("3x3", "U [R]*0").expect_err("zero bracket repeat must fail");
    assert!(err.to_string().contains("malformed repeat"), "err: {err}");
}

#[test]
fn normalize_rejects_a_zero_group_repeat() {
    let err = normalize_alg("3x3", "(R U)0").expect_err("zero group repeat must fail");
    assert!(err.to_string().contains("malformed repeat"), "err: {err}");
}

#[test]
fn normalize_expands_a_group_repeat_at_exactly_the_limit() {
    let out = normalize_alg("3x3", "(R U)99").expect("group repeat at the limit must pass");
    assert_eq!(out.alg, vec!["R U"; 99].join(" "));
}

#[test]
fn normalize_rejects_a_suffix_glued_to_a_group_repeat() {
    for alg in ["(R U)2x", "(R U)2'", "(R U)'"] {
        let err = normalize_alg("3x3", alg).expect_err("glued suffix must fail");
        assert!(err.to_string().contains("malformed repeat"), "err: {err}");
    }
}

#[test]
fn normalize_pins_the_group_expand_length_boundary_as_a_token_error() {
    let content = "R".repeat(9090);
    let alg = format!("({content})11");
    let err = normalize_alg("3x3", &alg).expect_err("boundary group must fail as a token error");
    assert!(err.to_string().contains("unknown token"), "err: {err}");
}

#[test]
fn normalize_strips_nested_grouping_parens() {
    let out = normalize_alg("3x3", "(R U (R' F))").expect("nested groups strip");
    assert_eq!(out.alg, "R U R' F");
}

#[test]
fn sq1_rejects_a_three_token_auf_shaped_prefix() {
    let err = normalize_alg("sq1", "(U D U) / (3,3)").expect_err("three token auf shape must fail");
    assert!(err.to_string().contains("unknown token"), "err: {err}");
}

#[test]
fn sq1_keeps_a_non_auf_prefix_group_and_rejects_its_token() {
    for alg in ["(R2) / (3,3)", "(x') (0,1)", "(F) / (0,1)"] {
        let err = normalize_alg("sq1", alg).expect_err("non auf prefix must not be stripped");
        let shown = err.to_string();
        assert!(shown.contains("unknown token"), "err: {shown}");
        assert!(
            shown.contains(alg.split_whitespace().next().unwrap_or_default()),
            "err: {shown}"
        );
    }
}

#[test]
fn sq1_pins_the_reported_span_of_an_unknown_token() {
    for alg in ["x/(3,3)", "x / (3,3)"] {
        let err = normalize_alg("sq1", alg).expect_err("stray letter must fail");
        let shown = err.to_string();
        assert!(shown.contains("unknown token 'x'"), "err: {shown}");
    }
}
