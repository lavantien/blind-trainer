use bt_data::paths::packs_root_from;
use bt_data::repo_seeds::{seed_all, seed_pack};
use bt_data::seedpack::{
    NSpec, PackEntry, SeedRow, expected_failure, load_pack, load_registry, normalize_alg,
    parse_pack, parse_registry,
};
use std::collections::BTreeMap;
use std::path::{Path, PathBuf};
use std::sync::atomic::{AtomicU64, Ordering};

const HEADER: &str = "case_key\talg\tfront_json\tsource_url\tsource_at";

fn workspace_root() -> PathBuf {
    PathBuf::from(env!("CARGO_MANIFEST_DIR"))
        .parent()
        .and_then(|p| p.parent())
        .expect("workspace root")
        .to_path_buf()
}

fn real_packs() -> PathBuf {
    match std::env::var("BT_REF_GROUND") {
        Ok(raw) if !raw.is_empty() => {
            let path = PathBuf::from(raw);
            if path.is_absolute() {
                path
            } else {
                workspace_root().join(path)
            }
        }
        _ => workspace_root().join("ref_ground"),
    }
    .join("packs")
}

static DIR_COUNTER: AtomicU64 = AtomicU64::new(0);

fn temp_packs(tag: &str) -> PathBuf {
    let dir = std::env::temp_dir()
        .join("bt-data-seedpack-tests")
        .join(format!(
            "{tag}-{}-{}",
            std::process::id(),
            DIR_COUNTER.fetch_add(1, Ordering::Relaxed)
        ));
    let _ = std::fs::remove_dir_all(&dir);
    std::fs::create_dir_all(&dir).expect("temp packs dir");
    dir
}

fn write_pack(dir: &Path, registry: &str, file: &str, tsv: &str) {
    std::fs::write(dir.join("registry.json"), registry).expect("registry");
    std::fs::write(dir.join(file), tsv).expect("tsv");
}

fn mini_registry(expected_rows: usize) -> String {
    format!(
        "[{{\"file\":\"mini.tsv\",\"event\":\"3x3\",\"method\":\"cfop\",\"stage\":\"t\",\"n\":3,\"expected_rows\":{expected_rows}}}]"
    )
}

fn mini_tsv(rows: &[(&str, &str)]) -> String {
    let mut text = String::from(HEADER);
    for (key, alg) in rows {
        text.push_str(&format!(
            "\n{key}\t{alg}\t{{\"case\":\"x\"}}\thttps://example.test/a\t2026-10-16"
        ));
    }
    text
}

fn mini_entry() -> PackEntry {
    parse_registry(&mini_registry(1))
        .expect("registry")
        .pop()
        .expect("one entry")
}

#[test]
fn registry_parses_the_real_registry_with_21_rows_and_all_three_n_shapes() {
    let entries = load_registry(&real_packs()).expect("real registry");
    assert_eq!(entries.len(), 21);
    assert_eq!(entries[0].n, NSpec::Unset);
    let parity = entries
        .iter()
        .find(|entry| entry.file == "bigcube_parity.tsv")
        .expect("parity entry");
    assert_eq!(parity.n, NSpec::Sizes(vec![4, 5, 6, 7]));
    let pll = entries
        .iter()
        .find(|entry| entry.event == "3x3" && entry.stage == "pll")
        .expect("3x3 pll entry");
    assert_eq!(pll.n, NSpec::Fixed(3));
    let oh = entries
        .iter()
        .find(|entry| entry.event == "oh" && entry.stage == "pll")
        .expect("oh pll entry");
    assert_eq!(oh.file, pll.file);
}

#[test]
fn registry_rejects_unknown_fields() {
    let text = "[{\"file\":\"a.tsv\",\"event\":\"e\",\"method\":\"m\",\"stage\":\"s\",\"n\":null,\"expected_rows\":1,\"oops\":true}]";
    let err = parse_registry(text).expect_err("unknown field must fail");
    assert!(err.to_string().contains("oops"), "err: {err}");
}

#[test]
fn registry_rejects_a_malformed_n_value() {
    let text = "[{\"file\":\"a.tsv\",\"event\":\"e\",\"method\":\"m\",\"stage\":\"s\",\"n\":\"3\",\"expected_rows\":1}]";
    let err = parse_registry(text).expect_err("string n must fail");
    assert!(err.to_string().contains("n"), "err: {err}");
}

#[test]
fn registry_rejects_a_missing_expected_rows_field() {
    let text = "[{\"file\":\"a.tsv\",\"event\":\"e\",\"method\":\"m\",\"stage\":\"s\",\"n\":null}]";
    assert!(parse_registry(text).is_err());
}

#[test]
fn registry_rejects_a_path_separator_in_the_file_name() {
    let text = "[{\"file\":\"../escape.tsv\",\"event\":\"e\",\"method\":\"m\",\"stage\":\"s\",\"n\":null,\"expected_rows\":1}]";
    let err = parse_registry(text).expect_err("path escape must fail");
    assert!(err.to_string().contains("file"), "err: {err}");
}

#[test]
fn registry_rejects_a_duplicate_event_method_stage_triplet() {
    let text = "[{\"file\":\"a.tsv\",\"event\":\"e\",\"method\":\"m\",\"stage\":\"s\",\"n\":null,\"expected_rows\":1},{\"file\":\"b.tsv\",\"event\":\"e\",\"method\":\"m\",\"stage\":\"s\",\"n\":null,\"expected_rows\":1}]";
    let err = parse_registry(text).expect_err("duplicate key triplet must fail");
    assert!(err.to_string().contains("s"), "err: {err}");
}

#[test]
fn registry_rejects_zero_expected_rows_and_out_of_range_n() {
    let zero = mini_registry(0);
    assert!(parse_registry(&zero).is_err());
    let negative = "[{\"file\":\"a.tsv\",\"event\":\"e\",\"method\":\"m\",\"stage\":\"s\",\"n\":-3,\"expected_rows\":1}]";
    assert!(parse_registry(negative).is_err());
    let sizes = "[{\"file\":\"a.tsv\",\"event\":\"e\",\"method\":\"m\",\"stage\":\"s\",\"n\":[4,9],\"expected_rows\":1}]";
    assert!(parse_registry(sizes).is_err());
}

#[test]
fn tsv_rejects_a_ragged_row_with_four_columns() {
    let dir = temp_packs("ragged4");
    write_pack(
        &dir,
        &mini_registry(1),
        "mini.tsv",
        &format!("{HEADER}\na1\tR U\t{{\"case\":\"x\"}}\thttps://example.test/a"),
    );
    let err = load_pack(&dir, &mini_entry()).expect_err("four columns must fail");
    assert!(err.to_string().contains("5"), "err: {err}");
    assert!(err.to_string().contains("mini.tsv"), "err: {err}");
}

#[test]
fn tsv_rejects_a_ragged_row_with_six_columns() {
    let dir = temp_packs("ragged6");
    write_pack(
        &dir,
        &mini_registry(1),
        "mini.tsv",
        &format!(
            "{HEADER}\na1\tR U\t{{\"case\":\"x\"}}\thttps://example.test/a\t2026-10-16\textra"
        ),
    );
    assert!(load_pack(&dir, &mini_entry()).is_err());
}

#[test]
fn tsv_rejects_a_renamed_header() {
    let dir = temp_packs("header");
    write_pack(
        &dir,
        &mini_registry(1),
        "mini.tsv",
        "key\talg\tfront_json\tsource_url\tsource_at\na1\tR U\t{\"case\":\"x\"}\thttps://example.test/a\t2026-10-16",
    );
    let err = load_pack(&dir, &mini_entry()).expect_err("renamed header must fail");
    assert!(err.to_string().contains("header"), "err: {err}");
}

#[test]
fn tsv_rejects_row_count_drift_in_both_directions() {
    let dir = temp_packs("drift-count");
    write_pack(
        &dir,
        &mini_registry(2),
        "mini.tsv",
        &mini_tsv(&[("a1", "R U")]),
    );
    let expect_two = || {
        parse_registry(&mini_registry(2))
            .expect("registry")
            .pop()
            .expect("entry")
    };
    let err = load_pack(&dir, &expect_two()).expect_err("missing row must fail");
    let shown = err.to_string();
    assert!(shown.contains("expected 2"), "err: {shown}");
    assert!(shown.contains("found 1"), "err: {shown}");
    write_pack(
        &dir,
        &mini_registry(1),
        "mini.tsv",
        &mini_tsv(&[("a1", "R U"), ("a2", "R U'")]),
    );
    assert!(load_pack(&dir, &mini_entry()).is_err());
}

#[test]
fn tsv_rejects_a_duplicate_case_key() {
    let dir = temp_packs("dup-key");
    write_pack(
        &dir,
        &mini_registry(2),
        "mini.tsv",
        &mini_tsv(&[("a1", "R U"), ("a1", "R U'")]),
    );
    let err = load_pack(&dir, &mini_entry()).expect_err("duplicate key must fail");
    assert!(err.to_string().contains("a1"), "err: {err}");
}

#[test]
fn tsv_rejects_front_json_that_is_not_an_object() {
    let dir = temp_packs("bad-json");
    write_pack(
        &dir,
        &mini_registry(1),
        "mini.tsv",
        &format!("{HEADER}\na1\tR U\t[1,2]\thttps://example.test/a\t2026-10-16"),
    );
    assert!(load_pack(&dir, &mini_entry()).is_err());
}

#[test]
fn tsv_rejects_an_empty_or_padded_case_key_and_a_bad_source_date() {
    let dir = temp_packs("bad-key");
    write_pack(
        &dir,
        &mini_registry(1),
        "mini.tsv",
        &format!("{HEADER}\n a1 \tR U\t{{\"case\":\"x\"}}\thttps://example.test/a\t2026-10-16"),
    );
    assert!(load_pack(&dir, &mini_entry()).is_err());
    write_pack(
        &dir,
        &mini_registry(1),
        "mini.tsv",
        &format!("{HEADER}\na1\tR U\t{{\"case\":\"x\"}}\thttps://example.test/a\t2026/10/16"),
    );
    assert!(load_pack(&dir, &mini_entry()).is_err());
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
fn whitespace_and_comma_variants_hash_identically() {
    let base = parse_pack(
        &mini_entry(),
        &format!("{HEADER}\na1\tR, U\t{{\"case\":\"x\"}}\thttps://example.test/a\t2026-10-16"),
    )
    .expect("pack");
    let spaced = parse_pack(
        &mini_entry(),
        &format!("{HEADER}\na1\tR   U\t{{\"case\":\"x\"}}\thttps://example.test/a\t2026-10-16"),
    )
    .expect("pack");
    assert_eq!(base[0].content_hash, spaced[0].content_hash);
    let grouped = parse_pack(
        &mini_entry(),
        &format!("{HEADER}\na1\t(R U)\t{{\"case\":\"x\"}}\thttps://example.test/a\t2026-10-16"),
    )
    .expect("pack");
    assert_eq!(base[0].content_hash, grouped[0].content_hash);
}

#[test]
fn canonical_hash_pins_the_reference_vector() {
    let rows = load_pack(
        &real_packs(),
        &load_registry(&real_packs())
            .expect("registry")
            .into_iter()
            .find(|entry| entry.file == "sq1_cubeshape.tsv")
            .expect("cubeshape entry"),
    )
    .expect("cubeshape rows");
    let row = rows
        .iter()
        .find(|row| row.case_key == "cs.002")
        .expect("cs.002");
    assert_eq!(
        row.content_hash,
        "e43d1ec256ede1a9fe0878cf40fd4fd9f022a11f154addf3c5297af5ad4fa805"
    );
}

#[test]
fn hash_covers_every_field() {
    let entry = mini_entry();
    let base = parse_pack(
        &entry,
        &format!("{HEADER}\na1\tR U\t{{\"case\":\"x\"}}\thttps://example.test/a\t2026-10-16"),
    )
    .expect("pack");
    let other_url = parse_pack(
        &entry,
        &format!("{HEADER}\na1\tR U\t{{\"case\":\"x\"}}\thttps://example.test/b\t2026-10-16"),
    )
    .expect("pack");
    assert_ne!(base[0].content_hash, other_url[0].content_hash);
    let other_json = parse_pack(
        &entry,
        &format!("{HEADER}\na1\tR U\t{{\"case\":\"y\"}}\thttps://example.test/a\t2026-10-16"),
    )
    .expect("pack");
    assert_ne!(base[0].content_hash, other_json[0].content_hash);
    let other_alg = parse_pack(
        &entry,
        &format!("{HEADER}\na1\tR U'\t{{\"case\":\"x\"}}\thttps://example.test/a\t2026-10-16"),
    )
    .expect("pack");
    assert_ne!(base[0].content_hash, other_alg[0].content_hash);
}

#[test]
fn enriched_front_json_sorts_keys_and_preserves_source_fields() {
    let rows = load_pack(
        &real_packs(),
        &load_registry(&real_packs())
            .expect("registry")
            .into_iter()
            .find(|entry| entry.file == "sq1_ep.tsv")
            .expect("ep entry"),
    )
    .expect("ep rows");
    let row = rows
        .iter()
        .find(|row| row.case_key == "ep.19")
        .expect("ep.19");
    assert_eq!(
        row.front_json,
        "{\"case\":\"W-Adj\",\"loader_notes\":[\"equator marker stripped: *\",\"auf prefix stripped: (U)\"]}"
    );
    assert_eq!(
        row.alg,
        "(1,0) / (3,0) / (-1,-1) / (3,0) / (-3,0) / (1,1) / (-3,0) / (5,0)"
    );
}

#[test]
fn front_json_rejects_a_loader_notes_collision() {
    let entry = mini_entry();
    let err = parse_pack(
        &entry,
        &format!("{HEADER}\na1\tR U\t{{\"loader_notes\":[]}}\thttps://example.test/a\t2026-10-16"),
    )
    .expect_err("preexisting note key must fail");
    assert!(err.to_string().contains("loader_notes"), "err: {err}");
}

#[test]
fn every_real_pack_loads_with_its_declared_row_count_and_stable_hashes() {
    let root = real_packs();
    let registry = load_registry(&root).expect("registry");
    let mut first = BTreeMap::new();
    let mut total: usize = 0;
    for entry in &registry {
        let rows = load_pack(&root, entry).expect("pack loads");
        assert_eq!(rows.len(), entry.expected_rows, "{}", entry.file);
        total += entry.expected_rows;
        for row in &rows {
            let key = format!(
                "{}/{}/{}/{}",
                row.event, row.method, row.stage, row.case_key
            );
            assert!(first.insert(key, row.content_hash.clone()).is_none());
        }
    }
    assert_eq!(total, 1115);
    for entry in &registry {
        let rows = load_pack(&root, entry).expect("pack reloads");
        for row in &rows {
            let key = format!(
                "{}/{}/{}/{}",
                row.event, row.method, row.stage, row.case_key
            );
            assert_eq!(&first[&key], &row.content_hash, "{}", key);
        }
    }
}

#[test]
fn expected_failure_marks_exactly_the_nine_ruled_rows() {
    let expected = [
        ("sq1", "lbl", "cubeshape", "cs.119"),
        ("sq1", "lbl", "cubeshape", "cs.075"),
        ("sq1", "lbl", "cubeshape", "cs.078"),
        ("sq1", "lbl", "cubeshape", "cs.084"),
        ("sq1", "lbl", "cubeshape", "cs.089"),
        ("sq1", "lbl", "cubeshape", "cs.097"),
        ("sq1", "lbl", "cubeshape", "cs.109"),
        ("sq1", "lbl", "cubeshape", "cs.118"),
        ("sq1", "lbl", "cubeshape", "cs.129"),
    ];
    for (event, method, stage, key) in expected {
        let reason = expected_failure(event, method, stage, key).expect(key);
        assert!(!reason.is_empty(), "{key}");
    }
    let unmarked = expected_failure("sq1", "lbl", "cubeshape", "cs.119x");
    assert!(unmarked.is_none());
    assert!(expected_failure("sq1", "lbl", "cubeshape", "cs.001").is_none());
    assert!(expected_failure("sq1", "lbl", "cubeshape", "cs.122").is_none());
    assert!(expected_failure("sq1", "lbl", "ep", "ep.19").is_none());
    assert!(expected_failure("3x3", "cfop", "f2l", "f2l.fr.07").is_none());
    assert!(expected_failure("3bld", "old_pochmann", "concept", "op.09").is_none());
    let root = real_packs();
    let registry = load_registry(&root).expect("registry");
    let cubeshape = registry
        .iter()
        .find(|entry| entry.file == "sq1_cubeshape.tsv")
        .expect("cubeshape");
    let marked = load_pack(&root, cubeshape)
        .expect("rows")
        .iter()
        .filter(|row| {
            expected_failure(&row.event, &row.method, &row.stage, &row.case_key).is_some()
        })
        .count();
    assert_eq!(marked, 9);
}

#[test]
fn seed_all_loads_every_real_row_and_a_rerun_verifies_without_duplicates() {
    let mut db = bt_data::db::Db::open_in_memory().expect("db");
    let reports = seed_all(&mut db, &real_packs()).expect("seed");
    assert_eq!(reports.len(), 21);
    let inserted: usize = reports.iter().map(|report| report.inserted).sum();
    assert_eq!(inserted, 1115);
    let second = seed_all(&mut db, &real_packs()).expect("reseed");
    let verified: usize = second.iter().map(|report| report.verified).sum();
    assert_eq!(verified, 1115);
    let count = bt_data::repo_seeds::seed_row_count(&db).expect("count");
    assert_eq!(count, 1115);
    let rows = bt_data::repo_seeds::load_seed_rows(&db).expect("rows");
    assert_eq!(rows.len(), 1115);
    let cs002 = rows
        .iter()
        .find(|row| row.case_key == "cs.002")
        .expect("cs.002");
    assert_eq!(cs002.alg, "/ (3,3) /");
    assert_eq!(cs002.event, "sq1");
    assert_eq!(cs002.method, "lbl");
    assert_eq!(cs002.stage, "cubeshape");
    assert_eq!(
        cs002.source_url,
        "https://cubingapp.com/algorithms/SQ1-Cube-Shape"
    );
    assert_eq!(cs002.source_at, "2026-10-10");
    assert_eq!(
        cs002.content_hash,
        "e43d1ec256ede1a9fe0878cf40fd4fd9f022a11f154addf3c5297af5ad4fa805"
    );
}

#[test]
fn a_rerun_of_an_unchanged_pack_reports_only_verifies() {
    let dir = temp_packs("noop");
    write_pack(
        &dir,
        &mini_registry(1),
        "mini.tsv",
        &mini_tsv(&[("a1", "R U")]),
    );
    let mut db = bt_data::db::Db::open_in_memory().expect("db");
    let first = seed_pack(&mut db, &dir, &mini_entry()).expect("seed");
    assert_eq!((first.inserted, first.verified), (1, 0));
    let second = seed_pack(&mut db, &dir, &mini_entry()).expect("reseed");
    assert_eq!((second.inserted, second.verified), (0, 1));
}

#[test]
fn a_drifted_rerun_fails_hard_naming_the_file_and_row() {
    let dir = temp_packs("drift");
    write_pack(
        &dir,
        &mini_registry(2),
        "mini.tsv",
        &mini_tsv(&[("a1", "R U"), ("a2", "R U'")]),
    );
    let entry = parse_registry(&mini_registry(2))
        .expect("registry")
        .pop()
        .expect("entry");
    let mut db = bt_data::db::Db::open_in_memory().expect("db");
    seed_pack(&mut db, &dir, &entry).expect("seed");
    write_pack(
        &dir,
        &mini_registry(2),
        "mini.tsv",
        &mini_tsv(&[("a1", "R U"), ("a2", "R U2")]),
    );
    let err = seed_pack(&mut db, &dir, &entry).expect_err("drift must fail");
    let shown = err.to_string();
    assert!(shown.contains("mini.tsv"), "err: {shown}");
    assert!(shown.contains("a2"), "err: {shown}");
    let stored: String = db
        .conn()
        .query_row(
            "SELECT alg FROM seed_alg WHERE case_key = 'a2'",
            [],
            |row| row.get(0),
        )
        .expect("stored row");
    assert_eq!(stored, "R U'");
}

#[test]
fn a_row_dropped_from_a_pack_fails_hard_naming_the_orphan() {
    let dir = temp_packs("orphan");
    write_pack(
        &dir,
        &mini_registry(2),
        "mini.tsv",
        &mini_tsv(&[("a1", "R U"), ("a2", "R U'")]),
    );
    let entry = parse_registry(&mini_registry(2))
        .expect("registry")
        .pop()
        .expect("entry");
    let mut db = bt_data::db::Db::open_in_memory().expect("db");
    seed_pack(&mut db, &dir, &entry).expect("seed");
    write_pack(
        &dir,
        &mini_registry(1),
        "mini.tsv",
        &mini_tsv(&[("a1", "R U")]),
    );
    let shrunk = parse_registry(&mini_registry(1))
        .expect("registry")
        .pop()
        .expect("entry");
    let err = seed_pack(&mut db, &dir, &shrunk).expect_err("dropped row must fail");
    let shown = err.to_string();
    assert!(shown.contains("a2"), "err: {shown}");
    assert!(shown.contains("mini.tsv"), "err: {shown}");
}

#[test]
fn packs_root_resolves_from_the_hub_and_the_env_override() {
    let config = serde_json::json!({"paths": {"packs": "ref_ground/packs"}});
    let from_config = packs_root_from(None, &config).expect("config root");
    assert_eq!(from_config, workspace_root().join("ref_ground/packs"));
    let absolute = std::env::temp_dir().join("rg");
    let env_absolute =
        packs_root_from(Some(&absolute.to_string_lossy()), &config).expect("env root");
    assert_eq!(env_absolute, absolute.join("packs"));
    let missing = serde_json::json!({"paths": {}});
    let err = packs_root_from(None, &missing).expect_err("missing key must fail");
    assert!(err.to_string().contains("paths.packs"), "err: {err}");
}

#[test]
fn registry_rejects_the_dotdot_file_name() {
    let text = "[{\"file\":\"..\",\"event\":\"e\",\"method\":\"m\",\"stage\":\"s\",\"n\":null,\"expected_rows\":1}]";
    let err = parse_registry(text).expect_err("dotdot file name must fail");
    assert!(err.to_string().contains("bare name"), "err: {err}");
}

#[test]
fn registry_rejects_an_empty_event_or_method_field() {
    let empty_event = "[{\"file\":\"a.tsv\",\"event\":\"\",\"method\":\"m\",\"stage\":\"s\",\"n\":null,\"expected_rows\":1}]";
    let err = parse_registry(empty_event).expect_err("empty event must fail");
    assert!(
        err.to_string().contains("empty event, method, or stage"),
        "err: {err}"
    );
    let empty_method = "[{\"file\":\"a.tsv\",\"event\":\"e\",\"method\":\"\",\"stage\":\"s\",\"n\":null,\"expected_rows\":1}]";
    let err = parse_registry(empty_method).expect_err("empty method must fail");
    assert!(
        err.to_string().contains("empty event, method, or stage"),
        "err: {err}"
    );
}

#[test]
fn tsv_pins_the_physical_line_number_of_a_ragged_row() {
    let dir = temp_packs("ragged-line");
    write_pack(
        &dir,
        &mini_registry(1),
        "mini.tsv",
        &format!("{HEADER}\n\na1\tR U\t{{\"case\":\"x\"}}\thttps://example.test/a"),
    );
    let err = load_pack(&dir, &mini_entry()).expect_err("ragged row must fail");
    let shown = err.to_string();
    assert!(shown.contains("line 3"), "err: {shown}");
}

#[test]
fn tsv_rejects_a_short_source_date() {
    let dir = temp_packs("short-date");
    write_pack(
        &dir,
        &mini_registry(1),
        "mini.tsv",
        &format!("{HEADER}\na1\tR U\t{{\"case\":\"x\"}}\thttps://example.test/a\t1234-56-7"),
    );
    let err = load_pack(&dir, &mini_entry()).expect_err("nine character date must fail");
    assert!(err.to_string().contains("source_at"), "err: {err}");
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
