pub mod algs;
pub mod case_drill;
pub mod case_drill_view;
pub mod drill;
pub mod drill_card;
pub mod drill_view;
pub mod images;
pub mod memo;
pub mod menu;
pub mod replay;
pub mod settings;
pub mod solve;
pub mod stats;

use crate::logic::RuntimePaths;
use crate::persist::Stores;
use crate::states::{AppState, CaseDrillPhase, DrillPhase, MemoPhase, SolvePhase};
use bevy::prelude::*;
use bt_scheme::CaseId;
use serde::Deserialize;
use std::collections::BTreeMap;
use std::path::PathBuf;
use std::time::{SystemTime, UNIX_EPOCH};

#[derive(SystemSet, Clone, Copy, PartialEq, Eq, Hash, Debug)]
pub struct ViewsLogic;

#[derive(Clone, Copy, Debug, Resource, Default)]
pub struct PendingDrill(pub Option<CaseId>);

#[derive(Clone, Copy, Debug, Resource)]
pub struct NowDay(pub i64);

#[derive(Clone, Debug, Default, Resource)]
pub struct SyllableBook {
    pub entries: BTreeMap<(String, String), String>,
    pub loaded: bool,
}

impl SyllableBook {
    pub fn syllable_for(&self, orbit: &str, letters: [char; 2]) -> Option<&str> {
        let pair: String = letters.iter().collect();
        self.entries
            .get(&(orbit.to_string(), pair))
            .map(String::as_str)
    }
}

#[derive(Deserialize)]
struct SyllableCase {
    orbit: String,
    letters: String,
    syllable: String,
}

#[derive(Deserialize)]
struct SyllablesFile {
    cases: Vec<SyllableCase>,
}

pub fn syllables_path(paths: &RuntimePaths) -> PathBuf {
    let configured = crate::logic::load_config_value(&paths.config_path)
        .ok()
        .and_then(|value| {
            value
                .pointer("/paths/data_ga")
                .and_then(|section| section.as_str())
                .map(PathBuf::from)
        })
        .unwrap_or_else(|| PathBuf::from("data").join("ga"));
    if configured.is_absolute() {
        configured.join("syllables.json")
    } else {
        bt_data::paths::workspace_root()
            .join(configured)
            .join("syllables.json")
    }
}

pub fn load_syllable_book(paths: &RuntimePaths) -> SyllableBook {
    let Ok(bytes) = std::fs::read(syllables_path(paths)) else {
        return SyllableBook::default();
    };
    let Ok(file) = serde_json::from_slice::<SyllablesFile>(&bytes) else {
        return SyllableBook::default();
    };
    let mut entries = BTreeMap::new();
    for case in file.cases {
        if !case.syllable.is_empty() {
            entries.insert((case.orbit, case.letters), case.syllable);
        }
    }
    SyllableBook {
        entries,
        loaded: true,
    }
}

pub fn now_day() -> i64 {
    now_unix_secs() / 86_400
}

pub fn now_unix_secs() -> i64 {
    SystemTime::now()
        .duration_since(UNIX_EPOCH)
        .map(|duration| duration.as_secs() as i64)
        .unwrap_or(0)
}

pub fn now_unix_ms() -> i64 {
    SystemTime::now()
        .duration_since(UNIX_EPOCH)
        .map(|duration| duration.as_millis() as i64)
        .unwrap_or(0)
}

pub fn time_seed() -> u64 {
    SystemTime::now()
        .duration_since(UNIX_EPOCH)
        .map(|duration| duration.as_nanos() as u64)
        .unwrap_or(0)
}

pub struct ViewsLogicPlugin;

impl Plugin for ViewsLogicPlugin {
    fn build(&self, app: &mut App) {
        let paths = app
            .world()
            .get_resource::<RuntimePaths>()
            .cloned()
            .expect("RuntimePaths installed by AppLogicPlugin");
        app.insert_resource(NowDay(now_day()))
            .insert_resource(PendingDrill::default())
            .insert_resource(load_syllable_book(&paths))
            .insert_resource(memo::MemoSeed(time_seed()))
            .insert_resource(solve::SolveSeed(time_seed()))
            .insert_resource(solve::SolveScrambleOverride::default())
            .insert_resource(memo::MemoSession::default())
            .insert_resource(solve::SolveSession::default())
            .insert_resource(stats::StatsSnapshot::default())
            .insert_resource(drill::DrillSeed(time_seed()))
            .insert_resource(drill::DrillSession::default())
            .insert_resource(algs::AlgsQuery::default())
            .insert_resource(algs::AlgsBrowser::default())
            .insert_resource(algs::browse::RepoBrowser::default())
            .insert_resource(algs::detail::OverrideStore::default())
            .insert_resource(algs::detail::OverrideDraft::default())
            .insert_resource(case_drill::PendingCaseDrill::default())
            .insert_resource(case_drill::CaseCardStore::default())
            .insert_resource(case_drill::CaseDrillSession::default())
            .insert_resource(settings::SettingsDraft::default())
            .insert_resource(replay::ReplayScene::default())
            .configure_sets(Update, ViewsLogic.run_if(resource_exists::<Stores>))
            .add_systems(
                Update,
                (
                    IntoScheduleConfigs::run_if(drill::card_keys, in_state(DrillPhase::Card)),
                    IntoScheduleConfigs::run_if(drill::ensure_card, in_state(DrillPhase::Card)),
                    (
                        IntoScheduleConfigs::run_if(drill::reps_tick, in_state(DrillPhase::Reps)),
                        IntoScheduleConfigs::run_if(drill::reps_keys, in_state(DrillPhase::Reps)),
                        IntoScheduleConfigs::run_if(drill::reps_bind, in_state(DrillPhase::Reps)),
                    )
                        .chain(),
                    (
                        IntoScheduleConfigs::run_if(drill::rate_keys, in_state(DrillPhase::Rate)),
                        IntoScheduleConfigs::run_if(drill::rate_bind, in_state(DrillPhase::Rate)),
                    )
                        .chain(),
                    IntoScheduleConfigs::run_if(memo::intro_keys, in_state(MemoPhase::Intro)),
                    IntoScheduleConfigs::run_if(memo::stream, in_state(MemoPhase::Stream)),
                    IntoScheduleConfigs::run_if(memo::recall_keys, in_state(MemoPhase::Recall)),
                    IntoScheduleConfigs::run_if(memo::review_keys, in_state(MemoPhase::Review)),
                    IntoScheduleConfigs::run_if(solve::ready_keys, in_state(SolvePhase::Ready)),
                    IntoScheduleConfigs::run_if(solve::memo_tick, in_state(SolvePhase::Memo)),
                    IntoScheduleConfigs::run_if(solve::memo_keys, in_state(SolvePhase::Memo)),
                    IntoScheduleConfigs::run_if(solve::exec_tick, in_state(SolvePhase::Exec)),
                    IntoScheduleConfigs::run_if(solve::exec_keys, in_state(SolvePhase::Exec)),
                    IntoScheduleConfigs::run_if(solve::review_keys, in_state(SolvePhase::Review)),
                    (
                        IntoScheduleConfigs::run_if(
                            replay::replay_open_key,
                            in_state(SolvePhase::Review),
                        ),
                        IntoScheduleConfigs::run_if(replay::keys, in_state(AppState::Replay)),
                        IntoScheduleConfigs::run_if(replay::drag, in_state(AppState::Replay)),
                        IntoScheduleConfigs::run_if(replay::zoom, in_state(AppState::Replay)),
                        IntoScheduleConfigs::run_if(replay::tick, in_state(AppState::Replay)),
                    ),
                    IntoScheduleConfigs::run_if(algs::refresh_list, in_state(AppState::Algs)),
                    IntoScheduleConfigs::run_if(algs::focus_query_keys, in_state(AppState::Algs)),
                    IntoScheduleConfigs::run_if(algs::list_keys, in_state(AppState::Algs)),
                    (
                        IntoScheduleConfigs::run_if(
                            algs::browse::repo_keys,
                            in_state(AppState::Algs),
                        ),
                        IntoScheduleConfigs::run_if(
                            algs::detail::override_keys,
                            in_state(AppState::Algs),
                        ),
                        (
                            IntoScheduleConfigs::run_if(
                                case_drill::show_keys,
                                in_state(CaseDrillPhase::Show),
                            ),
                            IntoScheduleConfigs::run_if(
                                case_drill::grade_keys,
                                in_state(CaseDrillPhase::Grade),
                            ),
                        ),
                    )
                        .chain(),
                    (
                        IntoScheduleConfigs::run_if(
                            settings::mode_keys,
                            in_state(AppState::Settings),
                        ),
                        IntoScheduleConfigs::run_if(
                            settings::apply_draft,
                            in_state(AppState::Settings),
                        ),
                    )
                        .chain(),
                )
                    .in_set(ViewsLogic),
            )
            .add_systems(
                OnEnter(AppState::Drill),
                (drill::enter, drill::sync_net).chain(),
            )
            .add_systems(OnEnter(DrillPhase::Card), drill::enter_card)
            .add_systems(OnEnter(DrillPhase::Reps), drill::enter_reps)
            .add_systems(OnEnter(DrillPhase::Rate), drill::enter_rate)
            .add_systems(OnEnter(AppState::Memo), memo::enter)
            .add_systems(OnEnter(MemoPhase::Intro), memo::enter_intro)
            .add_systems(OnEnter(MemoPhase::Stream), memo::enter_stream)
            .add_systems(OnEnter(MemoPhase::Recall), memo::enter_recall)
            .add_systems(OnEnter(MemoPhase::Review), memo::enter_review)
            .add_systems(OnEnter(AppState::Solve), solve::enter)
            .add_systems(OnEnter(SolvePhase::Ready), solve::enter_ready)
            .add_systems(OnEnter(SolvePhase::Memo), solve::enter_memo)
            .add_systems(OnEnter(SolvePhase::Exec), solve::enter_exec)
            .add_systems(OnEnter(SolvePhase::Review), solve::enter_review)
            .add_systems(OnEnter(AppState::Replay), replay::enter)
            .add_systems(
                OnEnter(AppState::Algs),
                (
                    algs::enter,
                    algs::browse::repo_enter,
                    algs::detail::override_enter,
                    case_drill::cards_enter,
                )
                    .chain(),
            )
            .add_systems(
                OnEnter(AppState::CaseDrill),
                (case_drill::enter, case_drill_view::spawn_ui).chain(),
            )
            .add_systems(OnEnter(CaseDrillPhase::Show), case_drill_view::enter_show)
            .add_systems(OnEnter(CaseDrillPhase::Grade), case_drill_view::enter_grade)
            .add_systems(OnEnter(AppState::Stats), stats::enter)
            .add_systems(OnEnter(AppState::Settings), settings::enter);
    }
}
