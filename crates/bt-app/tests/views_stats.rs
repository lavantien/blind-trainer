#[allow(dead_code)]
mod views_common;

use bevy::prelude::*;
use bt_app::states::AppState;
use bt_app::views::settings::SettingsDraft;
use bt_app::views::stats::StatsSnapshot;
use views_common::*;

fn clone_paths(fixture: &Fixture) -> RuntimePaths {
    RuntimePaths {
        config_path: fixture.paths.config_path.clone(),
        vendor_root: fixture.paths.vendor_root.clone(),
        user_root: fixture.paths.user_root.clone(),
        assets_root: fixture.paths.assets_root.clone(),
    }
}

#[test]
fn stats_aggregate_seeded_solve_and_srs_fixtures() {
    let _guard = guard();
    let fixture = fixture("stats");
    std::fs::create_dir_all(fixture.user_root().join("solves")).expect("solves dir");
    std::fs::create_dir_all(fixture.user_root().join("srs")).expect("srs dir");
    std::fs::write(
        fixture.user_root().join("solves").join("solves.json"),
        serde_json::json!({
            "solves": [
                {"started_ms": 1, "memo_ms": 20000, "exec_ms": 10000, "success": true},
                {"started_ms": 2, "memo_ms": 30000, "exec_ms": 20000, "success": false}
            ]
        })
        .to_string(),
    )
    .expect("solves fixture");
    std::fs::write(
        fixture.user_root().join("srs").join("srs.json"),
        serde_json::json!({
            "cards": {
                "edge/UF-UB-UR": {
                    "ease": 2.5, "interval_days": 1, "due_day": 0, "reps": 2, "lapses": 0,
                    "reviews": [
                        {"ts_days": 100, "quality": 5, "elapsed_days": 0, "scheduled_days": 1},
                        {"ts_days": 101, "quality": 4, "elapsed_days": 1, "scheduled_days": 1}
                    ]
                },
                "corner/UFR-UBL-UBR": {
                    "ease": 2.5, "interval_days": 1, "due_day": 99999, "reps": 1, "lapses": 0,
                    "reviews": [
                        {"ts_days": 100, "quality": 2, "elapsed_days": 0, "scheduled_days": 1}
                    ]
                }
            }
        })
        .to_string(),
    )
    .expect("srs fixture");

    let mut app = headless_app(clone_paths(&fixture));
    boot_to_menu(&mut app);
    open(&mut app, AppState::Stats);
    pump(&mut app, 2);
    let snap = *app.world().resource::<StatsSnapshot>();
    assert_eq!(snap.solves, 2);
    assert_eq!(snap.memo_mean_ms, 25000.0);
    assert_eq!(snap.memo_median_ms, 25000.0);
    assert_eq!(snap.exec_mean_ms, 15000.0);
    assert_eq!(snap.exec_median_ms, 15000.0);
    assert_eq!(snap.due_edge, 1);
    assert_eq!(snap.due_corner, 0);
    assert_eq!(snap.streak, 2);
    let blocks = bt_app::views::stats::blocks(&snap);
    let values: Vec<&str> = blocks.iter().map(|block| block.value.as_str()).collect();
    assert_eq!(
        values,
        vec!["2", "2", "25.0s", "25.0s", "15.0s", "15.0s", "1", "0"]
    );
    let labels: Vec<&str> = blocks.iter().map(|block| block.label).collect();
    assert!(labels.contains(&"verified drill streak"));
    assert!(labels.contains(&"due edge"));
    assert!(labels.contains(&"due corner"));
}

#[test]
fn settings_change_persists_across_store_reload() {
    let _guard = guard();
    let fixture = fixture("settings");
    let mutated = bt_app::persist::Settings {
        memo_cadence_ms: 1234,
        memo_mode: bt_app::persist::MemoMode::Images,
        drill_reps: 9,
    };

    let mut first = headless_app(clone_paths(&fixture));
    boot_to_menu(&mut first);
    open(&mut first, AppState::Settings);
    pump(&mut first, 2);
    assert_eq!(
        *first.world().resource::<SettingsDraft>(),
        SettingsDraft::from_settings(&bt_app::persist::Settings {
            memo_cadence_ms: 800,
            memo_mode: bt_app::persist::MemoMode::Syllables,
            drill_reps: 4,
        })
    );

    *first.world_mut().resource_mut::<SettingsDraft>() = SettingsDraft {
        memo_cadence_ms: mutated.memo_cadence_ms,
        memo_mode: mutated.memo_mode,
        drill_reps: mutated.drill_reps,
    };
    pump(&mut first, 3);
    assert_eq!(
        first.world().resource::<bt_app::persist::Stores>().settings,
        mutated
    );
    let saved = std::fs::read_to_string(fixture.user_root().join("settings").join("settings.json"))
        .expect("settings file");
    assert!(saved.contains("1234"));
    assert!(saved.contains("images"));

    let mut second = headless_app(clone_paths(&fixture));
    boot_to_menu(&mut second);
    assert_eq!(
        second
            .world()
            .resource::<bt_app::persist::Stores>()
            .settings,
        mutated,
        "settings survive a store reload"
    );
}

#[test]
fn settings_mode_keys_toggle_the_draft() {
    let _guard = guard();
    let fixture = fixture("modekeys");
    let mut app = headless_app(clone_paths(&fixture));
    boot_to_menu(&mut app);
    open(&mut app, AppState::Settings);
    pump(&mut app, 2);
    press(&mut app, KeyCode::Digit2);
    pump(&mut app, 2);
    assert_eq!(
        app.world().resource::<SettingsDraft>().memo_mode,
        bt_app::persist::MemoMode::Images
    );
    press(&mut app, KeyCode::Digit1);
    pump(&mut app, 2);
    assert_eq!(
        app.world().resource::<SettingsDraft>().memo_mode,
        bt_app::persist::MemoMode::Syllables
    );
    assert_eq!(
        app.world()
            .resource::<bt_app::persist::Stores>()
            .settings
            .memo_mode,
        bt_app::persist::MemoMode::Syllables
    );
}
