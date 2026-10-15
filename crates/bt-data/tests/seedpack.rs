use bt_data::paths::packs_root_from;
use bt_data::seedpack::{
    NSpec, PackEntry, expected_failure, load_pack, load_registry, normalize_alg, parse_pack,
    parse_registry,
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
    assert!(normalize_alg("3x3", "(R U").is_err());
    assert!(normalize_alg("3x3", "R U)").is_err());
    assert!(normalize_alg("3x3", "(R U)x").is_err());
    assert!(normalize_alg("3x3", "(R U)x").is_err());
    assert!(normalize_alg("sq1", "(1,0) (U D) /").is_err());
    assert!(normalize_alg("sq1", "(x,y)").is_err());
    assert!(normalize_alg("sq1", "(0,1,2)").is_err());
    assert!(normalize_alg("sq1", "(01,0)").is_err());
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
