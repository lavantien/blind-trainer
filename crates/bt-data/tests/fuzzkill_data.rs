use std::collections::BTreeSet;
use std::path::{Path, PathBuf};

use bt_core::{Alg, Cube, Piece, corner_stickers, edge_stickers, piece_of};
use bt_data::corpus::{Corpus, Row, SheetClass, Solver};
use bt_data::paths::{
    APP_DIR_NAME, PathsError, USER_ALGS_FILE, UserAlgs, config_path_from, load_config, read_json,
    user_algs_path, user_algs_path_at, user_dir_path, user_dir_path_at, user_root, user_root_from,
    vendor_root, vendor_root_from, write_json_atomic,
};
use bt_data::srs::{Card, ConfigError, Rating, Sm2Config};
use bt_data::tsv::{TsvError, parse_sheet};
use bt_data::verify::{Problem, Verdict, verify};

fn name_of_sticker(sticker: u8) -> String {
    bt_scheme::name_of(sticker).to_string()
}

fn all_names() -> BTreeSet<String> {
    (0..48u8).map(name_of_sticker).collect()
}

fn edge_piece_names(piece: u8) -> String {
    name_of_sticker(edge_stickers(piece)[0])
}

fn corner_piece_names(piece: u8) -> String {
    name_of_sticker(corner_stickers(piece)[0])
}

fn slot_of_edge(cube: &Cube, piece: u8) -> u8 {
    let anchor = edge_stickers(piece)[0];
    let position = cube.position_of(anchor);
    (0..12u8)
        .find(|&slot| edge_stickers(slot).contains(&position))
        .expect("edge slot always exists")
}

fn slot_of_corner(cube: &Cube, piece: u8) -> u8 {
    let anchor = corner_stickers(piece)[0];
    let position = cube.position_of(anchor);
    (0..8u8)
        .find(|&slot| corner_stickers(slot).contains(&position))
        .expect("corner slot always exists")
}

fn applied(alg: &str) -> Cube {
    let mut cube = Cube::solved();
    cube.apply_alg(&Alg::parse(alg).unwrap());
    cube
}

fn sample_row(position: &str) -> Row {
    Row {
        solver: Solver::Cherry,
        sheet: SheetClass::Edge,
        rowid: 1,
        position: position.to_string(),
        alg: Alg::parse("R U R'").unwrap(),
        alg_text: "R U R'".to_string(),
        commutator: String::new(),
        recon_link: String::new(),
        youtube_link: String::new(),
    }
}

#[test]
fn display_and_debug_impls_are_pinned() {
    assert_eq!(Solver::Cherry.to_string(), "cherry");
    assert_eq!(Solver::Eggins.to_string(), "eggins");
    assert_eq!(Solver::User.to_string(), "user");
    for sheet in SheetClass::ALL {
        assert_eq!(sheet.to_string(), sheet.file());
    }

    let read_err = PathsError::Read {
        path: PathBuf::from("a.tsv"),
        source: std::io::Error::other("boom"),
    };
    assert_eq!(read_err.to_string(), "read a.tsv: boom");
    let missing = PathsError::Missing("srs.sm2");
    assert_eq!(missing.to_string(), "config section missing: srs.sm2");

    let bad_header = TsvError::BadHeader { line: 2 };
    assert_eq!(
        bad_header.to_string(),
        "line 2: header must be Position/Alg/Commutator/Reconstruction Link/Youtube Link"
    );
    let column_count = TsvError::ColumnCount { line: 3, found: 4 };
    assert_eq!(
        column_count.to_string(),
        "line 3: expected 5 columns, found 4"
    );
    let empty_field = TsvError::EmptyField {
        line: 4,
        column: "Alg",
    };
    assert_eq!(empty_field.to_string(), "line 4: Alg column is empty");

    assert_eq!(
        ConfigError::Missing("srs.sm2").to_string(),
        "config section missing: srs.sm2"
    );
    let malformed = serde_json::from_str::<u32>("not json").unwrap_err();
    let malformed_text = ConfigError::Malformed(malformed).to_string();
    assert!(malformed_text.starts_with("config section malformed: "));
    assert!(malformed_text.contains("line 1"), "{malformed_text}");

    let ok_verdict = Verdict {
        problems: vec![],
        notes: vec![],
    };
    assert_eq!(ok_verdict.to_string(), "ok");
    let two_problems = Verdict {
        problems: vec![
            Problem::BadPosition {
                detail: "first".to_string(),
            },
            Problem::CentersMoved,
        ],
        notes: vec![],
    };
    assert_eq!(
        two_problems.to_string(),
        "bad position: first\ncenters moved"
    );

    assert_eq!(
        format!("{:?}", Corpus::from_rows(vec![])),
        "Corpus { rows: 0 }"
    );
    let corpus = Corpus::from_rows(vec![sample_row("UF-FR")]);
    assert_eq!(format!("{:?}", corpus), "Corpus { rows: 1 }");
}

#[test]
fn corpus_is_empty_tracks_rows() {
    assert!(Corpus::from_rows(vec![]).is_empty());
    assert!(!Corpus::from_rows(vec![sample_row("UF-FR")]).is_empty());
}

#[test]
fn srs_accessors_are_pinned() {
    let config = serde_json::json!({
        "srs": {"sm2": {"again": 2, "hard": 3, "good": 4, "easy": 5, "interval_clamp_days": [7, 200]}}
    });
    let cfg = Sm2Config::from_value(&config).unwrap();
    assert_eq!(cfg.interval_min_days(), 7);
    assert_eq!(cfg.interval_max_days(), 200);
    assert_eq!(cfg.quality(Rating::Again), 2);
    assert_eq!(cfg.quality(Rating::Hard), 3);
    assert_eq!(cfg.quality(Rating::Good), 4);
    assert_eq!(cfg.quality(Rating::Easy), 5);

    let card = Card::new(10);
    assert_eq!(card.last_review_day(), None);
    let mut reviewed = Card::new(10);
    reviewed.review(Rating::Good, 42, &cfg);
    assert_eq!(reviewed.last_review_day(), Some(42));
}

#[test]
fn tsv_header_and_row_widths_are_pinned() {
    let header = "Position\tAlg\tCommutator\tReconstruction Link\tYoutube Link";
    let row = "UF-FR\tR U R'\t\t\t";
    assert!(parse_sheet(&format!("{header}\n{row}")).is_ok());
    assert!(parse_sheet(&format!("{header}\t\t\n{row}")).is_ok());
    assert!(matches!(
        parse_sheet(&format!("{header}\n{row}\textra")),
        Err(TsvError::ColumnCount { line: 2, found: 6 })
    ));

    let empty_last = "Position\tAlg\tCommutator\tReconstruction Link\t";
    assert!(matches!(
        parse_sheet(&format!("{empty_last}\n{row}")),
        Err(TsvError::BadHeader { line: 1 })
    ));

    let short_empty = "Position\tAlg\tCommutator\t";
    assert!(matches!(
        parse_sheet(&format!("{short_empty}\n{row}")),
        Err(TsvError::ColumnCount { line: 1, found: 4 })
    ));

    let short_full = "Position\tAlg\tCommutator\tx";
    assert!(matches!(
        parse_sheet(&format!("{short_full}\n{row}")),
        Err(TsvError::ColumnCount { line: 1, found: 4 })
    ));

    let wide_row = "UF-FR\tR U R'\t\t\tkept";
    assert!(matches!(
        parse_sheet(&format!("{header}\n{wide_row}\tkept2")),
        Err(TsvError::ColumnCount { line: 2, found: 6 })
    ));

    assert!(matches!(
        parse_sheet("Nope\tAlg\tCommutator\tReconstruction Link\tYoutube Link"),
        Err(TsvError::BadHeader { line: 1 })
    ));
}

#[test]
fn path_helpers_are_pinned() {
    let _root = bt_data::paths::workspace_root();
    let absolute = if cfg!(windows) {
        PathBuf::from("C:/somewhere/config.json")
    } else {
        PathBuf::from("/somewhere/config.json")
    };
    assert_eq!(config_path_from(Some(absolute.to_str().unwrap())), absolute);
    assert!(config_path_from(Some("rel/config.json")).ends_with("rel/config.json"));
    assert!(config_path_from(None).ends_with(Path::new("config").join("config.json")));

    let config = serde_json::json!({"paths": {"ref_ground": "rg"}});
    assert_eq!(
        vendor_root_from(Some(absolute.to_str().unwrap()), &config).unwrap(),
        absolute
    );
    assert!(vendor_root_from(Some(""), &config).unwrap().ends_with("rg"));
    assert!(vendor_root().unwrap().ends_with("ref_ground"));

    let appdata = if cfg!(windows) {
        PathBuf::from("C:/users/data")
    } else {
        PathBuf::from("/users/data")
    };
    assert_eq!(
        user_root_from(Some(appdata.to_str().unwrap()), None, None),
        appdata.join(APP_DIR_NAME)
    );
    let expected_user_root = user_root_from(
        std::env::var("APPDATA").ok().as_deref(),
        std::env::var("XDG_DATA_HOME").ok().as_deref(),
        std::env::var("HOME").ok().as_deref(),
    );
    assert_eq!(user_root(), expected_user_root);
    assert_eq!(
        user_dir_path(bt_data::paths::UserDir::Srs),
        user_dir_path_at(&user_root(), bt_data::paths::UserDir::Srs)
    );
    assert!(user_dir_path(bt_data::paths::UserDir::Algs).ends_with("algs"));
    assert_eq!(user_algs_path(), user_algs_path_at(&user_root()));
    assert!(user_algs_path().ends_with(Path::new("algs").join(USER_ALGS_FILE)));

    let loaded = load_config().unwrap();
    assert!(loaded.pointer("/fuzz").is_some());
    assert!(loaded.pointer("/paths").is_some());
}

#[test]
fn user_algs_round_trip_and_read_json_are_pinned() {
    let root = std::env::temp_dir().join(format!("bt-t14-{}", std::process::id()));
    let _ = std::fs::remove_dir_all(&root);
    std::fs::create_dir_all(&root).unwrap();

    assert_eq!(UserAlgs::load_at(&root).unwrap(), UserAlgs::new());
    assert!(read_json::<UserAlgs>(&root).is_err());

    let mut algs = UserAlgs::new();
    assert!(algs.is_empty());
    algs.set(SheetClass::Edge, "UF-FR", "R U R'");
    assert!(!algs.is_empty());
    assert_eq!(algs.alg_for(SheetClass::Edge, "UF-FR"), Some("R U R'"));
    algs.save_at(&root).unwrap();
    assert_eq!(UserAlgs::load_at(&root).unwrap(), algs);
    assert!(algs.remove(SheetClass::Edge, "UF-FR"));
    assert!(!algs.remove(SheetClass::Edge, "UF-FR"));
    assert!(algs.is_empty());

    let value = serde_json::json!({"k": 7});
    let file = root.join("round.json");
    write_json_atomic(&file, &value).unwrap();
    assert_eq!(read_json::<serde_json::Value>(&file).unwrap(), Some(value));
    assert_eq!(
        read_json::<serde_json::Value>(&root.join("missing.json")).unwrap(),
        None
    );

    let invalid = root.join("bad-algs\0");
    assert!(UserAlgs::new().save_at(&invalid).is_err());

    let _ = std::fs::remove_dir_all(&root);
}

#[test]
fn user_algs_load_and_save_use_the_user_root() {
    let stash = std::env::var("APPDATA").ok();
    let temp = std::env::temp_dir().join(format!("bt-t14-appdata-{}", std::process::id()));
    let _ = std::fs::remove_dir_all(&temp);
    std::fs::create_dir_all(&temp).unwrap();
    unsafe {
        std::env::set_var("APPDATA", &temp);
    }

    let mut algs = UserAlgs::new();
    algs.set(SheetClass::Corner, "UF-FR", "R U R'");
    algs.save().unwrap();
    assert!(user_algs_path().is_file(), "save must write the user file");
    assert_eq!(UserAlgs::load().unwrap(), algs);
    assert_eq!(
        UserAlgs::load()
            .unwrap()
            .alg_for(SheetClass::Corner, "UF-FR"),
        Some("R U R'")
    );

    unsafe {
        match stash {
            Some(value) => std::env::set_var("APPDATA", value),
            None => std::env::remove_var("APPDATA"),
        }
    }
    let _ = std::fs::remove_dir_all(&temp);
}

#[test]
fn verify_reports_displaced_and_center_problems() {
    let m2 = applied("M2");
    let mut swapped_edges = Vec::new();
    for piece in 0..12u8 {
        if slot_of_edge(&m2, piece) != piece {
            swapped_edges.push(piece);
        }
    }
    assert_eq!(swapped_edges.len(), 4, "M2 must swap two edge pairs");
    let mut mutual = None;
    for i in 0..swapped_edges.len() {
        for j in (i + 1)..swapped_edges.len() {
            let (a, b) = (swapped_edges[i], swapped_edges[j]);
            if slot_of_edge(&m2, a) == b && slot_of_edge(&m2, b) == a {
                mutual = Some((a, b));
            }
        }
    }
    let (ea, eb) = mutual.expect("M2 contains a swapped pair");

    let flips_position = format!("{} & {}", edge_piece_names(ea), edge_piece_names(eb));
    let flips = verify(
        &Alg::parse("M2").unwrap(),
        &flips_position,
        SheetClass::Flips,
    );
    assert!(!flips.ok());
    assert!(
        flips
            .problems
            .iter()
            .any(|p| matches!(p, Problem::Displaced { .. }))
    );
    for problem in &flips.problems {
        if let Problem::Displaced {
            expected_slot,
            actual_slot,
            ..
        } = problem
        {
            assert!(all_names().contains(expected_slot), "{expected_slot}");
            assert!(all_names().contains(actual_slot), "{actual_slot}");
        }
    }

    let mut unmoved_edges = Vec::new();
    for piece in 0..12u8 {
        if slot_of_edge(&m2, piece) == piece {
            unmoved_edges.push(piece);
        }
    }
    let tokens: Vec<String> = unmoved_edges[..3]
        .iter()
        .map(|&piece| edge_piece_names(piece))
        .collect();
    let three_style_position = tokens.join("-");
    let style = verify(
        &Alg::parse("M2").unwrap(),
        &three_style_position,
        SheetClass::Edge,
    );
    assert!(!style.ok());
    assert!(
        style
            .problems
            .iter()
            .any(|p| matches!(p, Problem::NonFixedResidue { .. }))
    );

    let rotated = verify(
        &Alg::parse("x").unwrap(),
        &three_style_position,
        SheetClass::Edge,
    );
    assert!(
        rotated
            .problems
            .iter()
            .any(|p| matches!(p, Problem::CentersMoved))
    );

    let combo = applied("M2 R U R' D' R U' R' D");
    let mut half_swapped = None;
    for a in 0..8u8 {
        let b = slot_of_corner(&combo, a);
        if b != a && slot_of_corner(&combo, b) != a {
            half_swapped = Some((a, b));
            break;
        }
    }
    let (ca, cb) = half_swapped.expect("corner three cycle has a one way pair");
    let parity_position = format!(
        "{}-{}, {}-{}",
        edge_piece_names(ea),
        edge_piece_names(eb),
        corner_piece_names(ca),
        corner_piece_names(cb)
    );
    let parity = verify(
        &Alg::parse("M2 R U R' D' R U' R' D").unwrap(),
        &parity_position,
        SheetClass::Parity,
    );
    assert!(!parity.ok());
    assert!(
        parity
            .problems
            .iter()
            .any(|p| matches!(p, Problem::Displaced { .. }))
    );
}

#[test]
fn verify_agrees_with_classifier_on_corner_cycle() {
    let alg = Alg::parse("R U R' D' R U' R' D").unwrap();
    let report = bt_scheme::classify_alg(&alg);
    let Some(position) = report.position() else {
        panic!(
            "corner commutator must classify as three style: {:?}",
            report.class
        )
    };
    let verdict = verify(&alg, &position, SheetClass::Corner);
    assert!(verdict.ok(), "{verdict}");

    let wrong = Alg::parse("R U R' D' R U' R' D R").unwrap();
    let wrong_verdict = verify(&wrong, &position, SheetClass::Corner);
    assert!(!wrong_verdict.ok());
}

#[test]
fn empty_alg_and_piece_index_helpers_stay_sane() {
    let solved = Cube::solved();
    for piece in 0..8u8 {
        assert_eq!(slot_of_corner(&solved, piece), piece);
    }
    for piece in 0..12u8 {
        assert_eq!(slot_of_edge(&solved, piece), piece);
    }
    assert_eq!(piece_of(corner_stickers(0)[1]), Piece::Corner(0));
    assert_eq!(piece_of(edge_stickers(0)[1]), Piece::Edge(0));
}

#[test]
fn verify_accepts_exactly_the_true_corner_cycle() {
    for text in ["R U R' D' R U' R' D", "R U' L' U R' U' L U"] {
        let alg = Alg::parse(text).unwrap();
        let cube = applied(text);
        assert!(
            verify(&alg, &report_position(&alg), SheetClass::Corner).ok(),
            "{text}"
        );

        let start = (0..24u8).find(|&s| cube.position_of(s) != s).unwrap();
        let second = cube.position_of(start);
        let third = cube.position_of(second);
        assert_eq!(cube.position_of(third), start, "{text} cycle shape");
        let cycle_pieces: Vec<u8> = [start, second, third]
            .iter()
            .map(|&s| match piece_of(s) {
                Piece::Corner(index) => index,
                _ => panic!("{text} sticker {s} is not a corner"),
            })
            .collect();

        let mut pool = Vec::new();
        for index in cycle_pieces {
            pool.extend(corner_stickers(index));
        }

        let mut rotations = Vec::new();
        let mut seen = Vec::new();
        for &sticker in &pool {
            if seen.contains(&sticker) {
                continue;
            }
            let b = cube.position_of(sticker);
            let c = cube.position_of(b);
            assert_eq!(cube.position_of(c), sticker, "{text} sticker cycle shape");
            seen.extend([sticker, b, c]);
            rotations.push([
                format!(
                    "{}-{}-{}",
                    name_of_sticker(sticker),
                    name_of_sticker(b),
                    name_of_sticker(c)
                ),
                format!(
                    "{}-{}-{}",
                    name_of_sticker(b),
                    name_of_sticker(c),
                    name_of_sticker(sticker)
                ),
                format!(
                    "{}-{}-{}",
                    name_of_sticker(c),
                    name_of_sticker(sticker),
                    name_of_sticker(b)
                ),
            ]);
        }
        let ok_set: Vec<String> = rotations.iter().flatten().cloned().collect();
        assert_eq!(ok_set.len(), 9, "{text} expected three sticker cycles");

        for a in &pool {
            for b in &pool {
                for c in &pool {
                    if a == b || b == c || a == c {
                        continue;
                    }
                    let candidate = format!(
                        "{}-{}-{}",
                        name_of_sticker(*a),
                        name_of_sticker(*b),
                        name_of_sticker(*c)
                    );
                    let verdict = verify(&alg, &candidate, SheetClass::Corner);
                    let expected_ok = ok_set.contains(&candidate);
                    assert_eq!(verdict.ok(), expected_ok, "{text} {candidate}");
                }
            }
        }
    }
}

fn report_position(alg: &Alg) -> String {
    let report = bt_scheme::classify_alg(alg);
    report
        .position()
        .unwrap_or_else(|| panic!("alg must classify as three style: {:?}", report.class))
}
