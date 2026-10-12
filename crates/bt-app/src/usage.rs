use crate::logic::{NavIntent, VendorCorpus};
use crate::smoke::{self, SmokeFonts, exit_with, shoot, track_fonts};
use crate::states::{AppState, DrillPhase, MemoPhase, SolvePhase};
use crate::views::PendingDrill;
use crate::views::drill::{case_position, sheet_of};
use crate::views::solve::{self, SolveSeed};
use bevy::input::ButtonState;
use bevy::input::keyboard::{Key, KeyboardInput};
use bevy::prelude::*;
use bt_core::Cube;
use bt_data::corpus::Corpus;
use bt_scheme::memo::memo as memo_of;
use bt_scheme::{CaseId, Orbit, random_state, sticker_of};
use std::path::PathBuf;

const DWELL_SECS: f32 = 0.4;
const STUCK_LIMIT_SECS: f32 = 30.0;
const SOLVE_SEED_ATTEMPTS: u64 = 400;
const WRONG_ALG: &str = "R U R' U'";

#[derive(Clone, Copy, Debug)]
enum Action {
    Press(KeyCode),
    NavOpen(AppState),
    NavMenu,
    PinDrill,
    CoveredDraft,
    WrongDraft,
    Query(&'static str),
    PickSolveSeed,
    WaitApp(AppState),
    WaitFonts,
    WaitDrill(DrillPhase),
    WaitMemo(MemoPhase),
    WaitSolve(SolvePhase),
    Elapse(f32),
    Shot(&'static str),
}

fn pinned_case() -> CaseId {
    CaseId {
        orbit: Orbit::Edge,
        t1: sticker_of(Orbit::Edge, 'A'),
        t2: sticker_of(Orbit::Edge, 'B'),
    }
}

pub fn covered_alg_text(corpus: &Corpus, case: &CaseId) -> Option<String> {
    corpus
        .rows_for_position(sheet_of(case.orbit), &case_position(case))
        .first()
        .map(|row| row.alg_text.clone())
}

pub fn fully_covered(corpus: &Corpus, scramble: &Cube) -> bool {
    let memo = memo_of(scramble);
    solve::simulate(corpus, scramble, &memo).success
}

pub fn covered_seed(corpus: &Corpus, base: u64, attempts: u64) -> Option<u64> {
    (base..base.saturating_add(attempts)).find(|&seed| fully_covered(corpus, &random_state(seed)))
}

fn script(cadence_secs: f32) -> Vec<Action> {
    let partial = cadence_secs * 3.25;
    vec![
        Action::WaitApp(AppState::Menu),
        Action::WaitFonts,
        Action::Shot("menu"),
        Action::PinDrill,
        Action::NavOpen(AppState::Drill),
        Action::WaitDrill(DrillPhase::Ready),
        Action::Shot("drill-ready"),
        Action::Press(KeyCode::Space),
        Action::WaitDrill(DrillPhase::Memo),
        Action::CoveredDraft,
        Action::Elapse(0.3),
        Action::Shot("drill-memo"),
        Action::Press(KeyCode::Enter),
        Action::WaitDrill(DrillPhase::Exec),
        Action::Shot("drill-exec-correct"),
        Action::Press(KeyCode::Digit3),
        Action::WaitDrill(DrillPhase::Review),
        Action::Shot("drill-review"),
        Action::Press(KeyCode::Space),
        Action::WaitDrill(DrillPhase::Ready),
        Action::Press(KeyCode::Space),
        Action::WaitDrill(DrillPhase::Memo),
        Action::WrongDraft,
        Action::Elapse(0.3),
        Action::Press(KeyCode::Enter),
        Action::WaitDrill(DrillPhase::Exec),
        Action::Shot("drill-exec-wrong"),
        Action::NavMenu,
        Action::WaitApp(AppState::Menu),
        Action::NavOpen(AppState::Memo),
        Action::WaitMemo(MemoPhase::Intro),
        Action::Shot("memo-intro"),
        Action::Press(KeyCode::Space),
        Action::WaitMemo(MemoPhase::Stream),
        Action::Elapse(partial),
        Action::Shot("memo-stream"),
        Action::WaitMemo(MemoPhase::Recall),
        Action::Shot("memo-recall"),
        Action::Press(KeyCode::Space),
        Action::WaitMemo(MemoPhase::Review),
        Action::Shot("memo-review"),
        Action::NavMenu,
        Action::WaitApp(AppState::Menu),
        Action::PickSolveSeed,
        Action::NavOpen(AppState::Solve),
        Action::WaitSolve(SolvePhase::Ready),
        Action::Shot("solve-ready"),
        Action::Press(KeyCode::Space),
        Action::WaitSolve(SolvePhase::Memo),
        Action::Elapse(partial),
        Action::Shot("solve-memo"),
        Action::Press(KeyCode::Space),
        Action::WaitSolve(SolvePhase::Exec),
        Action::Elapse(1.6),
        Action::Shot("solve-exec"),
        Action::Press(KeyCode::Space),
        Action::WaitSolve(SolvePhase::Review),
        Action::Shot("solve-review"),
        Action::NavMenu,
        Action::WaitApp(AppState::Menu),
        Action::NavOpen(AppState::Algs),
        Action::WaitApp(AppState::Algs),
        Action::Shot("algs"),
        Action::Query("B"),
        Action::Elapse(0.3),
        Action::Press(KeyCode::ArrowDown),
        Action::Elapse(0.1),
        Action::Press(KeyCode::ArrowDown),
        Action::Elapse(0.3),
        Action::Shot("algs-search"),
        Action::NavMenu,
        Action::WaitApp(AppState::Menu),
        Action::NavOpen(AppState::Stats),
        Action::WaitApp(AppState::Stats),
        Action::Shot("stats"),
        Action::NavMenu,
        Action::WaitApp(AppState::Menu),
        Action::NavOpen(AppState::Settings),
        Action::WaitApp(AppState::Settings),
        Action::Shot("settings"),
    ]
}

fn fatal_script() -> Vec<Action> {
    vec![
        Action::WaitApp(AppState::Fatal),
        Action::WaitFonts,
        Action::Shot("fatal"),
    ]
}

#[derive(Debug)]
enum Wait {
    App(AppState),
    Fonts,
    Drill(DrillPhase),
    Memo(MemoPhase),
    Solve(SolvePhase),
}

#[derive(Debug)]
enum Mode {
    Act,
    WaitUntil {
        until: Wait,
        waited: f32,
    },
    Elapse {
        left: f32,
    },
    Shot {
        path: PathBuf,
        clock: f32,
        taken: bool,
    },
    Done,
}

#[derive(Resource)]
struct UsageTour {
    dir: PathBuf,
    actions: Vec<Action>,
    index: usize,
    mode: Mode,
    shots: Vec<PathBuf>,
    expected_shots: usize,
}

pub enum UsageKind {
    Tour { seed: u64, cadence_ms: u32 },
    FatalShot,
}

pub struct UsagePlugin {
    pub dir: PathBuf,
    pub kind: UsageKind,
}

impl UsagePlugin {
    pub fn tour(dir: PathBuf, seed: u64, cadence_ms: u32) -> UsagePlugin {
        UsagePlugin {
            dir,
            kind: UsageKind::Tour { seed, cadence_ms },
        }
    }

    pub fn fatal_shot(dir: PathBuf) -> UsagePlugin {
        UsagePlugin {
            dir,
            kind: UsageKind::FatalShot,
        }
    }
}

impl Plugin for UsagePlugin {
    fn build(&self, app: &mut App) {
        std::fs::create_dir_all(&self.dir)
            .unwrap_or_else(|err| panic!("usage output dir {}: {err}", self.dir.display()));
        let actions = match &self.kind {
            UsageKind::Tour { seed, cadence_ms } => {
                let (memo_seed, solve_seed, drill_seed) = smoke::smoke_seeds(*seed);
                app.insert_resource(crate::views::memo::MemoSeed(memo_seed));
                app.insert_resource(crate::views::solve::SolveSeed(solve_seed));
                app.insert_resource(crate::views::drill::DrillSeed(drill_seed));
                script(*cadence_ms as f32 / 1000.0)
            }
            UsageKind::FatalShot => fatal_script(),
        };
        let expected_shots = actions
            .iter()
            .filter(|action| matches!(action, Action::Shot(_)))
            .count();
        app.insert_resource(UsageTour {
            dir: self.dir.clone(),
            actions,
            index: 0,
            mode: Mode::Act,
            shots: Vec::new(),
            expected_shots,
        })
        .init_resource::<SmokeFonts>()
        .add_systems(Update, (track_fonts, run_usage).chain());
    }
}

fn press(commands: &mut Commands, key: KeyCode) {
    commands.write_message(KeyboardInput {
        key_code: key,
        logical_key: logical_of(key),
        state: ButtonState::Pressed,
        text: None,
        repeat: false,
        window: Entity::PLACEHOLDER,
    });
}

fn logical_of(key: KeyCode) -> Key {
    match key {
        KeyCode::Space => Key::Space,
        KeyCode::Enter => Key::Enter,
        KeyCode::ArrowDown => Key::ArrowDown,
        KeyCode::Digit3 => Key::Character("3".into()),
        _ => Key::Character(" ".into()),
    }
}

fn finish(commands: &mut Commands, tour: &UsageTour) {
    let all_written = tour.shots.len() == tour.expected_shots
        && tour.shots.iter().all(|path| {
            std::fs::metadata(path)
                .map(|meta| meta.len() > 0)
                .unwrap_or(false)
        });
    exit_with(commands, all_written);
}

fn stuck_out(commands: &mut Commands, tour: &mut UsageTour, detail: String) {
    eprintln!("usage tour stuck at action {}: {detail}", tour.index);
    exit_with(commands, false);
    tour.mode = Mode::Done;
}

fn phase_matches<T: States>(phase: Option<&State<T>>, expected: &T) -> bool {
    phase.is_some_and(|state| state.get() == expected)
}

#[allow(clippy::too_many_arguments)]
fn run_usage(
    mut commands: Commands,
    app_state: Res<State<AppState>>,
    drill_phase: Option<Res<State<DrillPhase>>>,
    memo_phase: Option<Res<State<MemoPhase>>>,
    solve_phase: Option<Res<State<SolvePhase>>>,
    time: Res<Time<Virtual>>,
    fonts: Res<SmokeFonts>,
    corpus: Option<Res<VendorCorpus>>,
    mut tour: ResMut<UsageTour>,
    mut pending: ResMut<PendingDrill>,
    mut solve_seed: ResMut<SolveSeed>,
) {
    let dt = time.delta_secs();
    let mode = std::mem::replace(&mut tour.mode, Mode::Done);
    match mode {
        Mode::Act => {
            let Some(action) = tour.actions.get(tour.index).copied() else {
                finish(&mut commands, &tour);
                tour.mode = Mode::Done;
                return;
            };
            match action {
                Action::Press(key) => {
                    press(&mut commands, key);
                    tour.index += 1;
                    tour.mode = Mode::Act;
                }
                Action::NavOpen(state) => {
                    commands.write_message(NavIntent::Open(state));
                    tour.index += 1;
                    tour.mode = Mode::Act;
                }
                Action::NavMenu => {
                    commands.write_message(NavIntent::ToMenu);
                    tour.index += 1;
                    tour.mode = Mode::Act;
                }
                Action::PinDrill => {
                    pending.0 = Some(pinned_case());
                    tour.index += 1;
                    tour.mode = Mode::Act;
                }
                Action::CoveredDraft => {
                    let text = corpus
                        .as_ref()
                        .and_then(|loaded| covered_alg_text(&loaded.0, &pinned_case()));
                    match text {
                        Some(text) => {
                            commands.queue(move |world: &mut World| {
                                crate::views::drill::set_draft_and_box(world, &text);
                            });
                            tour.index += 1;
                            tour.mode = Mode::Act;
                        }
                        None => stuck_out(
                            &mut commands,
                            &mut tour,
                            "vendor corpus has no alg for UF-UB-UR, run make gen-data".to_string(),
                        ),
                    }
                }
                Action::WrongDraft => {
                    let text = WRONG_ALG.to_string();
                    commands.queue(move |world: &mut World| {
                        crate::views::drill::set_draft_and_box(world, &text);
                    });
                    tour.index += 1;
                    tour.mode = Mode::Act;
                }
                Action::Query(text) => {
                    commands.queue(move |world: &mut World| {
                        crate::views::algs::set_query_and_box(world, text);
                    });
                    tour.index += 1;
                    tour.mode = Mode::Act;
                }
                Action::PickSolveSeed => {
                    let found = corpus.as_ref().and_then(|loaded| {
                        covered_seed(&loaded.0, solve_seed.0, SOLVE_SEED_ATTEMPTS)
                    });
                    match found {
                        Some(seed) => {
                            solve_seed.0 = seed;
                            tour.index += 1;
                            tour.mode = Mode::Act;
                        }
                        None => stuck_out(
                            &mut commands,
                            &mut tour,
                            "no fully covered solve scramble found".to_string(),
                        ),
                    }
                }
                Action::WaitApp(expected) => {
                    tour.mode = Mode::WaitUntil {
                        until: Wait::App(expected),
                        waited: 0.0,
                    };
                }
                Action::WaitFonts => {
                    tour.mode = Mode::WaitUntil {
                        until: Wait::Fonts,
                        waited: 0.0,
                    };
                }
                Action::WaitDrill(expected) => {
                    tour.mode = Mode::WaitUntil {
                        until: Wait::Drill(expected),
                        waited: 0.0,
                    };
                }
                Action::WaitMemo(expected) => {
                    tour.mode = Mode::WaitUntil {
                        until: Wait::Memo(expected),
                        waited: 0.0,
                    };
                }
                Action::WaitSolve(expected) => {
                    tour.mode = Mode::WaitUntil {
                        until: Wait::Solve(expected),
                        waited: 0.0,
                    };
                }
                Action::Elapse(secs) => {
                    tour.mode = Mode::Elapse { left: secs };
                }
                Action::Shot(name) => {
                    let path = tour.dir.join(format!("{name}.png"));
                    tour.mode = Mode::Shot {
                        path,
                        clock: -DWELL_SECS,
                        taken: false,
                    };
                }
            }
        }
        Mode::WaitUntil { until, waited } => {
            let matched = match until {
                Wait::App(expected) => *app_state.get() == expected,
                Wait::Fonts => fonts.0,
                Wait::Drill(expected) => phase_matches(drill_phase.as_deref(), &expected),
                Wait::Memo(expected) => phase_matches(memo_phase.as_deref(), &expected),
                Wait::Solve(expected) => phase_matches(solve_phase.as_deref(), &expected),
            };
            if matched {
                tour.index += 1;
                tour.mode = Mode::Act;
            } else if waited + dt >= STUCK_LIMIT_SECS {
                stuck_out(
                    &mut commands,
                    &mut tour,
                    format!("wait {until:?} timed out"),
                );
            } else {
                tour.mode = Mode::WaitUntil {
                    until,
                    waited: waited + dt,
                };
            }
        }
        Mode::Elapse { left } => {
            if left - dt <= 0.0 {
                tour.index += 1;
                tour.mode = Mode::Act;
            } else {
                tour.mode = Mode::Elapse { left: left - dt };
            }
        }
        Mode::Shot { path, clock, taken } => {
            if clock + dt < 0.0 {
                tour.mode = Mode::Shot {
                    path,
                    clock: clock + dt,
                    taken,
                };
                return;
            }
            if !taken {
                shoot(&mut commands, path.clone());
                tour.mode = Mode::Shot {
                    path,
                    clock: clock + dt,
                    taken: true,
                };
                return;
            }
            let written = std::fs::metadata(&path)
                .map(|meta| meta.len() > 0)
                .unwrap_or(false);
            if written {
                tour.shots.push(path);
                tour.index += 1;
                tour.mode = Mode::Act;
            } else if clock + dt >= STUCK_LIMIT_SECS {
                stuck_out(
                    &mut commands,
                    &mut tour,
                    format!("shot {} never landed", path.display()),
                );
            } else {
                tour.mode = Mode::Shot {
                    path,
                    clock: clock + dt,
                    taken,
                };
            }
        }
        Mode::Done => {
            tour.mode = Mode::Done;
        }
    }
}

#[cfg(test)]
mod tests {
    use super::*;
    use bt_core::Alg;
    use bt_data::corpus::{Row, SheetClass, Solver};
    use bt_scheme::{EDGE_BUFFER_STICKER, apply_sticker_cycles, ideal_cycle_at, state_array};

    fn shot_names(actions: &[Action]) -> Vec<&'static str> {
        actions
            .iter()
            .filter_map(|action| match action {
                Action::Shot(name) => Some(*name),
                _ => None,
            })
            .collect()
    }

    #[test]
    fn script_shots_are_unique_and_complete() {
        let actions = script(0.8);
        let names = shot_names(&actions);
        let mut sorted = names.clone();
        sorted.sort_unstable();
        sorted.dedup();
        assert_eq!(sorted.len(), names.len(), "shot names must be unique");
        assert_eq!(names.first(), Some(&"menu"));
        assert_eq!(names.last(), Some(&"settings"));
        let mut expected = [
            "menu",
            "drill-ready",
            "drill-memo",
            "drill-exec-correct",
            "drill-review",
            "drill-exec-wrong",
            "memo-intro",
            "memo-stream",
            "memo-recall",
            "memo-review",
            "solve-ready",
            "solve-memo",
            "solve-exec",
            "solve-review",
            "algs",
            "algs-search",
            "stats",
            "settings",
        ];
        expected.sort_unstable();
        assert_eq!(sorted, expected);
    }

    #[test]
    fn script_never_issues_two_presses_back_to_back() {
        let actions = script(0.8);
        for pair in actions.windows(2) {
            assert!(
                !(matches!(pair[0], Action::Press(_)) && matches!(pair[1], Action::Press(_))),
                "adjacent presses can be eaten by phase entry message drains: {pair:?}"
            );
        }
    }

    #[test]
    fn script_sequences_each_phase_before_its_shot() {
        let actions = script(0.8);
        fn position(actions: &[Action], pred: &dyn Fn(&Action) -> bool) -> usize {
            actions
                .iter()
                .position(pred)
                .unwrap_or_else(|| panic!("action missing"))
        }
        assert!(
            position(&actions, &|a| matches!(
                a,
                Action::WaitDrill(DrillPhase::Exec)
            )) < position(&actions, &|a| matches!(
                a,
                Action::Shot("drill-exec-correct")
            ))
        );
        assert!(
            position(&actions, &|a| matches!(
                a,
                Action::WaitMemo(MemoPhase::Review)
            )) < position(&actions, &|a| matches!(a, Action::Shot("memo-review")))
        );
        assert!(
            position(&actions, &|a| matches!(a, Action::PickSolveSeed))
                < position(&actions, &|a| matches!(a, Action::NavOpen(AppState::Solve)))
        );
        assert!(
            position(&actions, &|a| matches!(a, Action::NavOpen(AppState::Stats)))
                > position(&actions, &|a| matches!(a, Action::Shot("solve-review")))
        );
    }

    #[test]
    fn logical_keys_match_their_key_codes() {
        assert_eq!(logical_of(KeyCode::Space), Key::Space);
        assert_eq!(logical_of(KeyCode::Enter), Key::Enter);
        assert_eq!(logical_of(KeyCode::ArrowDown), Key::ArrowDown);
        assert_eq!(
            logical_of(KeyCode::Digit3),
            Key::Character("3".into()),
            "digit keys carry their character"
        );
    }

    fn row(position: &str, alg: &str) -> Row {
        Row {
            solver: Solver::Cherry,
            sheet: SheetClass::Edge,
            rowid: 1,
            position: position.to_string(),
            alg: Alg::parse(alg).expect("fixture alg parses"),
            alg_text: alg.to_string(),
            commutator: String::new(),
            recon_link: String::new(),
            youtube_link: String::new(),
        }
    }

    fn single_pair_scramble(first: char, second: char) -> Cube {
        let t1 = sticker_of(Orbit::Edge, first);
        let t2 = sticker_of(Orbit::Edge, second);
        let mut occ = state_array(&Cube::solved());
        let inverse = ideal_cycle_at(Orbit::Edge, EDGE_BUFFER_STICKER, t2, t1);
        apply_sticker_cycles(&mut occ, &inverse);
        Cube::from_occ(occ)
    }

    #[test]
    fn covered_alg_text_reads_the_first_vendor_row() {
        let corpus = Corpus::from_rows(vec![row("UF-UB-UR", "R2 U' S R2 S' R2 U R2")]);
        assert_eq!(
            covered_alg_text(&corpus, &pinned_case()).as_deref(),
            Some("R2 U' S R2 S' R2 U R2")
        );
        assert_eq!(
            covered_alg_text(&Corpus::from_rows(Vec::new()), &pinned_case()),
            None
        );
    }

    #[test]
    fn fully_covered_judges_the_whole_memo() {
        let corpus = Corpus::from_rows(vec![row("UF-UB-UR", "R2 U' S R2 S' R2 U R2")]);
        let scramble = single_pair_scramble('A', 'B');
        assert!(fully_covered(&corpus, &scramble));
        assert!(!fully_covered(&Corpus::from_rows(Vec::new()), &scramble));
    }

    #[test]
    fn covered_seed_gives_up_within_its_attempt_budget() {
        assert_eq!(covered_seed(&Corpus::from_rows(Vec::new()), 7, 8), None);
    }
}
