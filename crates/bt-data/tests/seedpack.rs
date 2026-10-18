use bt_data::paths::packs_root_from;
use bt_data::repo_seeds::{seed_all, seed_pack};
use bt_data::seedpack::{
    NSpec, PackEntry, expected_failure, load_pack, load_registry, parse_pack, parse_registry,
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
