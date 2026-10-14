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

#[derive(Clone, Copy, Debug)]
enum Action {
    Press(KeyCode),
    PressChar { key: KeyCode, ch: char },
    NavOpen(AppState),
    NavMenu,
    PinDrill,
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
        Action::WaitDrill(DrillPhase::Card),
        Action::Shot("drill-card"),
        Action::Press(KeyCode::Space),
        Action::WaitDrill(DrillPhase::Reps),
        Action::Shot("drill-reps"),
        Action::Press(KeyCode::Space),
        Action::Elapse(0.1),
        Action::Press(KeyCode::Space),
        Action::Elapse(0.1),
        Action::Press(KeyCode::Space),
        Action::Elapse(0.1),
        Action::Press(KeyCode::Space),
        Action::Elapse(0.1),
        Action::Press(KeyCode::Space),
        Action::Elapse(0.1),
        Action::Press(KeyCode::Space),
        Action::Elapse(0.1),
        Action::WaitDrill(DrillPhase::Rate),
        Action::Shot("drill-rate-a"),
        Action::Press(KeyCode::Digit3),
        Action::Elapse(0.1),
        Action::Shot("drill-rate-b"),
        Action::Press(KeyCode::Digit3),
        Action::WaitDrill(DrillPhase::Card),
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
        Action::Press(KeyCode::Slash),
        Action::Elapse(0.1),
        Action::PressChar {
            key: KeyCode::KeyB,
            ch: 'b',
        },
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

fn press_char(commands: &mut Commands, key: KeyCode, ch: char) {
    let text = ch.to_string();
    commands.write_message(KeyboardInput {
        key_code: key,
        logical_key: Key::Character(text.clone().into()),
        state: ButtonState::Pressed,
        text: Some(text.into()),
        repeat: false,
        window: Entity::PLACEHOLDER,
    });
}

fn logical_of(key: KeyCode) -> Key {
    match key {
        KeyCode::Space => Key::Space,
        KeyCode::Enter => Key::Enter,
        KeyCode::ArrowDown => Key::ArrowDown,
        KeyCode::Slash => Key::Character("/".into()),
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
                Action::PressChar { key, ch } => {
                    press_char(&mut commands, key, ch);
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
    use bevy::ecs::message::Messages;
    use bt_core::Alg;
    use bt_data::corpus::{Row, SheetClass, Solver};
    use bt_scheme::{EDGE_BUFFER_STICKER, apply_sticker_cycles, ideal_cycle_at, state_array};
    use std::sync::atomic::{AtomicU64, Ordering};
    use std::time::Duration;

    static DIR_COUNTER: AtomicU64 = AtomicU64::new(0);

    fn usage_dir(tag: &str) -> PathBuf {
        std::env::temp_dir()
            .join("bt-app-usage-tests")
            .join(format!(
                "{tag}-{}-{}",
                std::process::id(),
                DIR_COUNTER.fetch_add(1, Ordering::Relaxed)
            ))
    }

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
            "drill-card",
            "drill-reps",
            "drill-rate-a",
            "drill-rate-b",
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
        let is_press =
            |action: &Action| matches!(action, Action::Press(_) | Action::PressChar { .. });
        for pair in actions.windows(2) {
            assert!(
                !(is_press(&pair[0]) && is_press(&pair[1])),
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
                Action::WaitDrill(DrillPhase::Rate)
            )) < position(&actions, &|a| matches!(a, Action::Shot("drill-rate-a")))
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

    fn usage_world(dir: PathBuf, actions: Vec<Action>, expected_shots: usize) -> World {
        let mut world = World::new();
        world.insert_resource(State::<AppState>::new(AppState::Menu));
        let mut time = Time::<Virtual>::default();
        time.advance_by(Duration::from_secs_f32(0.1));
        world.insert_resource(time);
        world.insert_resource(SmokeFonts(true));
        world.insert_resource(UsageTour {
            dir,
            actions,
            index: 0,
            mode: Mode::Act,
            shots: Vec::new(),
            expected_shots,
        });
        world.insert_resource(PendingDrill::default());
        world.insert_resource(SolveSeed(0));
        world.init_resource::<Messages<KeyboardInput>>();
        world.init_resource::<Messages<NavIntent>>();
        world.init_resource::<Messages<AppExit>>();
        world
    }

    fn step(world: &mut World) {
        let mut schedule = Schedule::default();
        schedule.add_systems(run_usage);
        schedule.run(world);
    }

    fn exit_taken(world: &mut World) -> Option<u8> {
        world
            .resource_mut::<Messages<AppExit>>()
            .drain()
            .next()
            .map(|exit| match exit {
                AppExit::Success => 0,
                AppExit::Error(code) => code.get(),
            })
    }

    #[test]
    fn the_verdict_follows_the_expected_shot_count() {
        let mut ok = usage_world(usage_dir("finish-ok"), Vec::new(), 0);
        step(&mut ok);
        assert_eq!(exit_taken(&mut ok), Some(0));
        assert!(matches!(ok.resource::<UsageTour>().mode, Mode::Done));
        let mut gap = usage_world(usage_dir("finish-gap"), Vec::new(), 2);
        step(&mut gap);
        assert_eq!(exit_taken(&mut gap), Some(1));
        assert!(matches!(gap.resource::<UsageTour>().mode, Mode::Done));
    }

    #[test]
    fn press_and_press_char_write_keyboard_input() {
        let dir = usage_dir("press");
        let mut world = usage_world(
            dir,
            vec![
                Action::Press(KeyCode::Space),
                Action::PressChar {
                    key: KeyCode::KeyB,
                    ch: 'b',
                },
            ],
            0,
        );
        step(&mut world);
        assert_eq!(world.resource::<UsageTour>().index, 1);
        let inputs: Vec<KeyboardInput> = world
            .resource_mut::<Messages<KeyboardInput>>()
            .drain()
            .collect();
        assert_eq!(inputs.len(), 1);
        assert_eq!(inputs[0].key_code, KeyCode::Space);
        assert!(matches!(inputs[0].logical_key, Key::Space));
        assert_eq!(inputs[0].state, ButtonState::Pressed);
        assert!(inputs[0].text.is_none());
        step(&mut world);
        assert_eq!(world.resource::<UsageTour>().index, 2);
        let typed: Vec<KeyboardInput> = world
            .resource_mut::<Messages<KeyboardInput>>()
            .drain()
            .collect();
        assert_eq!(typed.len(), 1);
        assert_eq!(typed[0].key_code, KeyCode::KeyB);
        assert!(matches!(
            &typed[0].logical_key,
            Key::Character(text) if text.as_str() == "b"
        ));
        assert_eq!(typed[0].text.as_ref().map(|text| text.as_str()), Some("b"));
    }

    #[test]
    fn nav_and_pin_actions_forward_their_effects() {
        let dir = usage_dir("nav-pin");
        let mut world = usage_world(
            dir,
            vec![
                Action::NavOpen(AppState::Algs),
                Action::NavMenu,
                Action::PinDrill,
            ],
            0,
        );
        step(&mut world);
        let intents: Vec<NavIntent> = world
            .resource_mut::<Messages<NavIntent>>()
            .drain()
            .collect();
        assert!(matches!(
            intents.as_slice(),
            [NavIntent::Open(AppState::Algs)]
        ));
        assert_eq!(world.resource::<UsageTour>().index, 1);
        step(&mut world);
        let intents: Vec<NavIntent> = world
            .resource_mut::<Messages<NavIntent>>()
            .drain()
            .collect();
        assert!(matches!(intents.as_slice(), [NavIntent::ToMenu]));
        assert_eq!(world.resource::<UsageTour>().index, 2);
        assert_eq!(world.resource::<PendingDrill>().0, None);
        step(&mut world);
        assert_eq!(world.resource::<UsageTour>().index, 3);
        assert_eq!(world.resource::<PendingDrill>().0, Some(pinned_case()));
    }

    #[test]
    fn pick_solve_seed_without_a_corpus_stops_the_tour() {
        let dir = usage_dir("pick-seed");
        let mut world = usage_world(dir, vec![Action::PickSolveSeed], 0);
        step(&mut world);
        assert_eq!(exit_taken(&mut world), Some(1));
        assert!(matches!(world.resource::<UsageTour>().mode, Mode::Done));
        assert_eq!(world.resource::<UsageTour>().index, 0);
        assert_eq!(world.resource::<SolveSeed>().0, 0);
    }

    #[test]
    fn waits_pass_immediately_when_the_target_matches() {
        let dir = usage_dir("wait-hit");
        let mut world = usage_world(
            dir,
            vec![
                Action::WaitApp(AppState::Menu),
                Action::WaitFonts,
                Action::WaitDrill(DrillPhase::Card),
                Action::WaitMemo(MemoPhase::Intro),
                Action::WaitSolve(SolvePhase::Ready),
            ],
            0,
        );
        world.insert_resource(State::<DrillPhase>::new(DrillPhase::Card));
        world.insert_resource(State::<MemoPhase>::new(MemoPhase::Intro));
        world.insert_resource(State::<SolvePhase>::new(SolvePhase::Ready));
        for expected in 1..=5 {
            step(&mut world);
            assert!(
                matches!(world.resource::<UsageTour>().mode, Mode::WaitUntil { .. }),
                "action {expected} must arm the wait first"
            );
            step(&mut world);
            assert_eq!(
                world.resource::<UsageTour>().index,
                expected,
                "wait {expected} must resolve on the next frame"
            );
            assert!(matches!(world.resource::<UsageTour>().mode, Mode::Act));
        }
    }

    #[test]
    fn waits_time_out_when_the_target_never_arrives() {
        let mut world = usage_world(usage_dir("wait-timeout"), Vec::new(), 0);
        world.resource_mut::<UsageTour>().mode = Mode::WaitUntil {
            until: Wait::Drill(DrillPhase::Rate),
            waited: 0.0,
        };
        step(&mut world);
        assert_eq!(world.resource::<UsageTour>().index, 0);
        assert_eq!(exit_taken(&mut world), None);
        let mut stuck = usage_world(usage_dir("wait-stuck"), Vec::new(), 0);
        stuck.resource_mut::<UsageTour>().mode = Mode::WaitUntil {
            until: Wait::App(AppState::Stats),
            waited: 29.95,
        };
        step(&mut stuck);
        assert_eq!(exit_taken(&mut stuck), Some(1));
        assert!(matches!(stuck.resource::<UsageTour>().mode, Mode::Done));
    }

    fn elapse_left(world: &World) -> f32 {
        match &world.resource::<UsageTour>().mode {
            Mode::Elapse { left } => *left,
            other => panic!("expected elapse mode, got {other:?}"),
        }
    }

    #[test]
    fn elapse_counts_down_by_the_frame_delta() {
        let dir = usage_dir("elapse");
        let mut world = usage_world(dir, vec![Action::Elapse(0.25)], 0);
        step(&mut world);
        assert!((elapse_left(&world) - 0.25).abs() < 1e-6);
        step(&mut world);
        assert!((elapse_left(&world) - 0.15).abs() < 1e-6);
        step(&mut world);
        assert!((elapse_left(&world) - 0.05).abs() < 1e-6);
        assert_eq!(world.resource::<UsageTour>().index, 0);
        step(&mut world);
        assert_eq!(world.resource::<UsageTour>().index, 1);
        assert!(matches!(world.resource::<UsageTour>().mode, Mode::Act));
    }

    fn shot_clock(world: &World) -> f32 {
        match &world.resource::<UsageTour>().mode {
            Mode::Shot { clock, .. } => *clock,
            other => panic!("expected shot mode, got {other:?}"),
        }
    }

    fn shot_taken(world: &World) -> bool {
        match &world.resource::<UsageTour>().mode {
            Mode::Shot { taken, .. } => *taken,
            other => panic!("expected shot mode, got {other:?}"),
        }
    }

    #[test]
    fn shot_dwells_then_captures_then_waits_for_the_file() {
        let dir = usage_dir("shot");
        std::fs::create_dir_all(&dir).expect("usage dir");
        let mut world = usage_world(dir.clone(), vec![Action::Shot("probe")], 1);
        step(&mut world);
        assert!((shot_clock(&world) + 0.4).abs() < 1e-6);
        let mut frames = 0;
        while !shot_taken(&world) {
            step(&mut world);
            frames += 1;
            assert!(frames < 10, "shot must fire once the dwell elapses");
        }
        assert!(shot_clock(&world) > -0.4);
        assert_eq!(world.resource::<UsageTour>().index, 0);
        let mut shots = world.query::<&bevy::render::view::screenshot::Screenshot>();
        assert_eq!(shots.iter(&world).count(), 1);
        step(&mut world);
        assert!(shot_taken(&world));
        assert_eq!(world.resource::<UsageTour>().shots.len(), 0);
        std::fs::write(dir.join("probe.png"), b"png").expect("shot lands");
        step(&mut world);
        let tour = world.resource::<UsageTour>();
        assert_eq!(tour.shots, vec![dir.join("probe.png")]);
        assert_eq!(tour.index, 1);
        assert!(matches!(tour.mode, Mode::Act));
    }

    #[test]
    fn done_mode_stays_done_without_side_effects() {
        let dir = usage_dir("done");
        let mut world = usage_world(dir, vec![Action::Press(KeyCode::Space)], 0);
        world.resource_mut::<UsageTour>().mode = Mode::Done;
        step(&mut world);
        assert!(matches!(world.resource::<UsageTour>().mode, Mode::Done));
        assert_eq!(world.resource::<UsageTour>().index, 0);
        assert!(world.resource::<Messages<KeyboardInput>>().is_empty());
    }

    #[test]
    fn fatal_script_shoots_only_the_fatal_screen() {
        let actions = fatal_script();
        assert_eq!(shot_names(&actions), vec!["fatal"]);
        assert!(matches!(
            actions.first(),
            Some(Action::WaitApp(AppState::Fatal))
        ));
    }

    #[test]
    fn script_elapsed_times_are_frame_dwells_or_cadence_scaled() {
        let actions = script(0.8);
        let partial = 0.8f32 * 3.25;
        let mut stream_waits = 0;
        let mut frame_dwells = 0;
        for action in &actions {
            if let Action::Elapse(secs) = action {
                let near = |a: f32, b: f32| (a - b).abs() < 1e-6;
                assert!(
                    near(*secs, 0.1)
                        || near(*secs, 0.3)
                        || near(*secs, 1.6)
                        || near(*secs, partial),
                    "unexpected elapse duration {secs}"
                );
                if near(*secs, partial) {
                    stream_waits += 1;
                }
                if near(*secs, 0.1) {
                    frame_dwells += 1;
                }
            }
        }
        assert_eq!(stream_waits, 2, "memo stream and solve memo waits");
        assert!(frame_dwells > 0);
    }

    #[test]
    fn plugin_build_wires_the_tour_and_the_seeds() {
        let dir = usage_dir("plugin-tour");
        let mut app = App::new();
        app.add_plugins(MinimalPlugins);
        app.add_plugins(UsagePlugin::tour(dir.clone(), 5, 800));
        assert!(dir.is_dir());
        assert_eq!(app.world().resource::<crate::views::memo::MemoSeed>().0, 5);
        assert_eq!(
            app.world().resource::<crate::views::solve::SolveSeed>().0,
            6
        );
        assert_eq!(
            app.world().resource::<crate::views::drill::DrillSeed>().0,
            7
        );
        let tour = app.world().resource::<UsageTour>();
        assert_eq!(tour.expected_shots, 17);
        assert_eq!(tour.index, 0);
        assert!(matches!(tour.mode, Mode::Act));
        assert!(tour.shots.is_empty());
        let fatal_dir = usage_dir("plugin-fatal");
        let mut fatal = App::new();
        fatal.add_plugins(MinimalPlugins);
        fatal.add_plugins(UsagePlugin::fatal_shot(fatal_dir.clone()));
        assert!(fatal_dir.is_dir());
        assert_eq!(fatal.world().resource::<UsageTour>().expected_shots, 1);
    }
}
