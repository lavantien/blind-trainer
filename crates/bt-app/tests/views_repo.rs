mod views_common;

use bevy::prelude::*;
use bt_app::logic::BootWarnings;
use bt_app::states::AppState;
use bt_app::views::PendingDrill;
use bt_app::views::algs::AlgsBrowser;
use bt_app::views::algs::browse::{RepoBrowser, RepoKind};
use std::path::Path;
use views_common::*;

const PACK_FRONT_JSON: &str = "{}";
const PACK_SOURCE_URL: &str = "https://example.test/pack";
const PACK_SOURCE_AT: &str = "2026-01-01";

fn temp_paths(fixture: &Fixture) -> RuntimePaths {
    RuntimePaths {
        config_path: fixture.paths.config_path.clone(),
        vendor_root: fixture.paths.vendor_root.clone(),
        packs_root: fixture.paths.packs_root.clone(),
        user_root: fixture.paths.user_root.clone(),
        assets_root: fixture.paths.assets_root.clone(),
    }
}

fn registry_text(entries: &[(&str, &str, &str, &str, &str, usize)]) -> String {
    let body: Vec<String> = entries
        .iter()
        .map(|(file, event, method, stage, n, rows)| {
            format!(
                "{{\"file\":\"{file}\",\"event\":\"{event}\",\"method\":\"{method}\",\"stage\":\"{stage}\",\"n\":{n},\"expected_rows\":{rows}}}"
            )
        })
        .collect();
    format!("[{}]", body.join(","))
}

fn pack_text(rows: &[(&str, &str)]) -> String {
    let mut text = String::from("case_key\talg\tfront_json\tsource_url\tsource_at\n");
    for (key, alg) in rows {
        text.push_str(&format!(
            "{key}\t{alg}\t{PACK_FRONT_JSON}\t{PACK_SOURCE_URL}\t{PACK_SOURCE_AT}\n"
        ));
    }
    text
}

fn write_packs(base: &Path, registry: &str, files: &[(&str, String)]) {
    let packs = base.join("vendor").join("packs");
    std::fs::create_dir_all(&packs).expect("packs dir");
    std::fs::write(packs.join("registry.json"), registry).expect("registry");
    for (file, text) in files {
        std::fs::write(packs.join(file), text).expect("pack file");
    }
}

fn repo_fixture(tag: &str) -> Fixture {
    let fixture = bare_fixture(tag);
    write_packs(
        &fixture.base,
        &registry_text(&[
            ("cll.tsv", "2x2", "cll", "cll", "null", 2),
            ("parity.tsv", "bigcube", "yau", "parity", "[4,5]", 1),
        ]),
        &[
            (
                "cll.tsv",
                pack_text(&[("cll.1", "R U R'"), ("cll.2", "F R F'")]),
            ),
            ("parity.tsv", pack_text(&[("par.1", "R U R'")])),
        ],
    );
    fixture
}

fn repo(app: &App) -> &RepoBrowser {
    app.world().resource::<RepoBrowser>()
}

#[test]
fn repo_cascade_walks_sizes_and_freezes_the_letters_list() {
    let _guard = guard();
    let fixture = repo_fixture("cascade");
    let mut app = headless_app(temp_paths(&fixture));
    boot_to_menu(&mut app);
    open(&mut app, AppState::Algs);
    let labels: Vec<String> = repo(&app)
        .arms
        .iter()
        .map(|arm| arm.label.clone())
        .collect();
    assert_eq!(labels, vec!["letters", "2x2", "bigcube"]);
    assert!(!repo(&app).repo_focused);

    press(&mut app, KeyCode::ArrowDown);
    pump(&mut app, 2);
    assert_eq!(app.world().resource::<AlgsBrowser>().selected, 1);

    press(&mut app, KeyCode::ArrowRight);
    pump(&mut app, 2);
    assert!(repo(&app).repo_focused);
    assert_eq!(repo(&app).depth, 0);

    press(&mut app, KeyCode::ArrowDown);
    pump(&mut app, 2);
    press(&mut app, KeyCode::ArrowDown);
    pump(&mut app, 2);
    assert_eq!(repo(&app).arms[repo(&app).arm].label, "bigcube");

    press(&mut app, KeyCode::ArrowRight);
    pump(&mut app, 2);
    assert_eq!(repo(&app).node().expect("size node").label, "4x4");
    press(&mut app, KeyCode::ArrowDown);
    pump(&mut app, 2);
    assert_eq!(repo(&app).node().expect("size node").label, "5x5");

    press(&mut app, KeyCode::ArrowRight);
    pump(&mut app, 2);
    assert_eq!(repo(&app).node().expect("method node").label, "yau");
    press(&mut app, KeyCode::ArrowRight);
    pump(&mut app, 2);
    assert_eq!(repo(&app).node().expect("stage node").label, "parity");
    press(&mut app, KeyCode::ArrowRight);
    pump(&mut app, 2);
    let case = repo(&app).node().expect("case node");
    assert_eq!(case.kind, RepoKind::Case);
    assert_eq!(case.label, "par.1");

    press(&mut app, KeyCode::ArrowDown);
    pump(&mut app, 2);
    press(&mut app, KeyCode::KeyN);
    pump(&mut app, 2);
    assert_eq!(
        app.world().resource::<AlgsBrowser>().selected,
        1,
        "letters selection must freeze while the repo holds focus"
    );
    assert_eq!(repo(&app).depth, 4);

    for _ in 0..5 {
        press(&mut app, KeyCode::ArrowLeft);
        pump(&mut app, 2);
    }
    assert!(!repo(&app).repo_focused);
    press(&mut app, KeyCode::ArrowDown);
    pump(&mut app, 2);
    assert_eq!(
        app.world().resource::<AlgsBrowser>().selected,
        2,
        "letters navigation resumes after the repo releases focus"
    );
}

#[test]
fn d_never_drills_from_the_repo_side() {
    let _guard = guard();
    let fixture = repo_fixture("nodrill");
    let mut app = headless_app(temp_paths(&fixture));
    boot_to_menu(&mut app);
    open(&mut app, AppState::Algs);
    press(&mut app, KeyCode::ArrowRight);
    pump(&mut app, 2);
    assert!(repo(&app).repo_focused);
    press(&mut app, KeyCode::KeyD);
    pump(&mut app, 3);
    assert_eq!(current_state(&app), AppState::Algs);
    assert_eq!(app.world().resource::<PendingDrill>().0, None);
}

#[test]
fn broken_pack_hides_its_event_and_warns_at_boot() {
    let _guard = guard();
    let fixture = bare_fixture("partialhide");
    write_packs(
        &fixture.base,
        &registry_text(&[
            ("cll.tsv", "2x2", "cll", "cll", "null", 1),
            ("bad.tsv", "skewb", "sarah", "sarah", "null", 1),
        ]),
        &[
            ("cll.tsv", pack_text(&[("cll.1", "R U R'")])),
            ("bad.tsv", "wrong\theader\n".to_string()),
        ],
    );
    let mut app = headless_app(temp_paths(&fixture));
    boot_to_menu(&mut app);
    let warnings = &app.world().resource::<BootWarnings>().0;
    assert_eq!(warnings.len(), 1);
    assert!(
        warnings[0].contains("bad.tsv"),
        "warning was {}",
        warnings[0]
    );
    open(&mut app, AppState::Algs);
    let labels: Vec<String> = repo(&app)
        .arms
        .iter()
        .map(|arm| arm.label.clone())
        .collect();
    assert_eq!(labels, vec!["letters", "2x2"]);
}

#[test]
fn all_packs_broken_keeps_the_letters_browser_alone() {
    let _guard = guard();
    let fixture = bare_fixture("fullhide");
    write_packs(
        &fixture.base,
        &registry_text(&[("bad.tsv", "skewb", "sarah", "sarah", "null", 1)]),
        &[("bad.tsv", "wrong\theader\n".to_string())],
    );
    let mut app = headless_app(temp_paths(&fixture));
    boot_to_menu(&mut app);
    open(&mut app, AppState::Algs);
    assert_eq!(repo(&app).arms.len(), 1);
    press(&mut app, KeyCode::ArrowRight);
    pump(&mut app, 2);
    assert!(
        !repo(&app).repo_focused,
        "right must stay inert with no seed arms"
    );
    press(&mut app, KeyCode::ArrowDown);
    pump(&mut app, 2);
    assert_eq!(app.world().resource::<AlgsBrowser>().selected, 1);
}

#[test]
fn reentering_the_algs_view_resets_the_repo_browser() {
    let _guard = guard();
    let fixture = repo_fixture("reenter");
    let mut app = headless_app(temp_paths(&fixture));
    boot_to_menu(&mut app);
    open(&mut app, AppState::Algs);
    press(&mut app, KeyCode::ArrowRight);
    pump(&mut app, 2);
    press(&mut app, KeyCode::ArrowDown);
    pump(&mut app, 2);
    press(&mut app, KeyCode::ArrowRight);
    pump(&mut app, 2);
    assert!(repo(&app).repo_focused);
    assert_eq!(repo(&app).depth, 1);

    press(&mut app, KeyCode::Escape);
    pump(&mut app, 3);
    assert_eq!(current_state(&app), AppState::Menu);
    open(&mut app, AppState::Algs);
    assert!(!repo(&app).repo_focused);
    assert_eq!(repo(&app).depth, 0);
    assert_eq!(repo(&app).arm, 0);
    assert_eq!(repo(&app).arms.len(), 3);
}
