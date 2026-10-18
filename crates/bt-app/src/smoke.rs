use crate::logic::FatalReason;
use crate::present::FontBook;
use crate::states::AppState;
use crate::views::case_drill::{DrillCase, PendingCaseDrill};
use bevy::prelude::*;
use bevy::render::view::screenshot::{Screenshot, save_to_disk};
use bt_data::repo_user::CaseRef;
use std::num::NonZero;
use std::path::PathBuf;

const DWELL_SECS: f32 = 0.5;
const STUCK_LIMIT_SECS: f32 = 10.0;
const SMOKE_FATAL_REASON: &str = "smoke tour complete";

pub struct SmokePlugin {
    pub dir: PathBuf,
    pub seed: u64,
}

fn tour_case() -> DrillCase {
    DrillCase {
        case: CaseRef {
            event: "3x3".to_string(),
            method: "cfop".to_string(),
            stage: "pll".to_string(),
            case_key: "smoke".to_string(),
        },
        size: None,
        seed_alg: "R U R'".to_string(),
        alg: "R U R'".to_string(),
    }
}

pub fn smoke_seeds(seed: u64) -> (u64, u64, u64) {
    (seed, seed.wrapping_add(1), seed.wrapping_add(2))
}

#[derive(Debug)]
enum SmokePhase {
    WaitForBoot {
        waited: f32,
    },
    Dwell {
        state: AppState,
        waited: f32,
        stuck: f32,
    },
    WaitForShot {
        state: AppState,
        path: PathBuf,
    },
    Done,
}

#[derive(Resource)]
struct SmokeTour {
    dir: PathBuf,
    phase: SmokePhase,
    entered: Vec<AppState>,
    shots: Vec<(AppState, PathBuf)>,
}

impl SmokeTour {
    fn shot_path(&self, state: AppState) -> PathBuf {
        self.dir.join(format!("{}.png", state.slug()))
    }
}

impl Plugin for SmokePlugin {
    fn build(&self, app: &mut App) {
        std::fs::create_dir_all(&self.dir)
            .unwrap_or_else(|err| panic!("smoke output dir {}: {err}", self.dir.display()));
        let (memo_seed, solve_seed, drill_seed) = smoke_seeds(self.seed);
        app.insert_resource(SmokeTour {
            dir: self.dir.clone(),
            phase: SmokePhase::WaitForBoot { waited: 0.0 },
            entered: Vec::new(),
            shots: Vec::new(),
        })
        .insert_resource(crate::views::memo::MemoSeed(memo_seed))
        .insert_resource(crate::views::solve::SolveSeed(solve_seed))
        .insert_resource(crate::views::drill::DrillSeed(drill_seed))
        .insert_resource(PendingCaseDrill(vec![tour_case()]))
        .insert_resource(crate::net::PulseFrozen)
        .init_resource::<SmokeFonts>()
        .add_systems(Update, (track_fonts, run_smoke).chain());
    }
}

#[derive(Clone, Copy, Debug, Default, Resource)]
pub(crate) struct SmokeFonts(pub(crate) bool);

pub(crate) fn track_fonts(
    mut ready: ResMut<SmokeFonts>,
    server: Res<AssetServer>,
    fonts: Res<FontBook>,
) {
    ready.0 = server.is_loaded_with_dependencies(&fonts.regular)
        && server.is_loaded_with_dependencies(&fonts.bold);
}

pub(crate) fn shoot(commands: &mut Commands, path: PathBuf) {
    std::fs::remove_file(&path).ok();
    commands
        .spawn(Screenshot::primary_window())
        .observe(save_to_disk(path));
}

pub(crate) fn exit_with(commands: &mut Commands, ok: bool) {
    if ok {
        commands.write_message(AppExit::Success);
    } else {
        commands.write_message(AppExit::Error(NonZero::new(1).expect("nonzero")));
    }
}

fn finish(commands: &mut Commands, tour: &SmokeTour) {
    let all_entered = tour.entered.len() == AppState::TOUR.len()
        && tour
            .entered
            .iter()
            .zip(AppState::TOUR.iter())
            .all(|(seen, expected)| seen == expected);
    let all_written = tour.shots.len() == AppState::TOUR.len()
        && tour.shots.iter().all(|(state, path)| {
            path.file_name().is_some()
                && std::fs::metadata(path)
                    .map(|meta| meta.len() > 0)
                    .unwrap_or(false)
                && path.ends_with(format!("{}.png", state.slug()))
        });
    exit_with(commands, all_entered && all_written);
}

fn advance_tour(
    commands: &mut Commands,
    tour: &mut SmokeTour,
    fatal: &mut FatalReason,
    next_state: &mut NextState<AppState>,
) {
    match AppState::TOUR.get(tour.entered.len()) {
        Some(&target) => {
            if target == AppState::Fatal {
                fatal.0 = SMOKE_FATAL_REASON.to_string();
            }
            next_state.set(target);
            tour.entered.push(target);
            tour.phase = SmokePhase::Dwell {
                state: target,
                waited: 0.0,
                stuck: 0.0,
            };
        }
        None => {
            tour.phase = SmokePhase::Done;
            finish(commands, tour);
        }
    }
}

fn run_smoke(
    mut commands: Commands,
    state: Res<State<AppState>>,
    mut next_state: ResMut<NextState<AppState>>,
    mut fatal: ResMut<FatalReason>,
    time: Res<Time<Virtual>>,
    fonts: Res<SmokeFonts>,
    mut tour: ResMut<SmokeTour>,
) {
    let dt = time.delta_secs();
    match std::mem::replace(&mut tour.phase, SmokePhase::Done) {
        SmokePhase::WaitForBoot { waited } => {
            if waited >= STUCK_LIMIT_SECS {
                exit_with(&mut commands, false);
                tour.phase = SmokePhase::Done;
                return;
            }
            if *state.get() == AppState::Menu && fonts.0 {
                tour.entered.push(AppState::Menu);
                tour.phase = SmokePhase::Dwell {
                    state: AppState::Menu,
                    waited: 0.0,
                    stuck: 0.0,
                };
            } else {
                tour.phase = SmokePhase::WaitForBoot {
                    waited: waited + dt,
                };
            }
        }
        SmokePhase::Dwell {
            state: target,
            waited,
            stuck,
        } => {
            if *state.get() == target {
                let waited = waited + dt;
                if waited >= DWELL_SECS {
                    let path = tour.shot_path(target);
                    shoot(&mut commands, path.clone());
                    tour.shots.push((target, path.clone()));
                    tour.phase = SmokePhase::WaitForShot {
                        state: target,
                        path,
                    };
                } else {
                    tour.phase = SmokePhase::Dwell {
                        state: target,
                        waited,
                        stuck: 0.0,
                    };
                }
            } else {
                let stuck = stuck + dt;
                if stuck >= STUCK_LIMIT_SECS {
                    commands.write_message(AppExit::Error(NonZero::new(3).expect("nonzero")));
                    tour.phase = SmokePhase::Done;
                } else {
                    tour.phase = SmokePhase::Dwell {
                        state: target,
                        waited,
                        stuck,
                    };
                }
            }
        }
        SmokePhase::WaitForShot { state: shot, path } => {
            let written = std::fs::metadata(&path)
                .map(|meta| meta.len() > 0)
                .unwrap_or(false);
            if !written {
                tour.phase = SmokePhase::WaitForShot { state: shot, path };
                return;
            }
            advance_tour(&mut commands, &mut tour, &mut fatal, &mut next_state);
        }
        SmokePhase::Done => {
            tour.phase = SmokePhase::Done;
        }
    }
}

#[cfg(test)]
mod tests {
    use super::*;
    use bevy::ecs::message::Messages;
    use std::path::Path;
    use std::sync::atomic::{AtomicU64, Ordering};
    use std::time::Duration;

    static DIR_COUNTER: AtomicU64 = AtomicU64::new(0);

    fn tour_dir(tag: &str) -> PathBuf {
        let dir = std::env::temp_dir()
            .join("bt-app-smoke-tests")
            .join(format!(
                "{tag}-{}-{}",
                std::process::id(),
                DIR_COUNTER.fetch_add(1, Ordering::Relaxed)
            ));
        std::fs::create_dir_all(&dir).expect("tour dir");
        dir
    }

    fn smoke_world(
        dir: PathBuf,
        phase: SmokePhase,
        entered: Vec<AppState>,
        state: AppState,
        dt: f32,
    ) -> World {
        let mut world = World::new();
        world.insert_resource(State::<AppState>::new(state));
        world.insert_resource(NextState::<AppState>::default());
        world.insert_resource(FatalReason::default());
        let mut time = Time::<Virtual>::default();
        time.advance_by(Duration::from_secs_f32(dt));
        world.insert_resource(time);
        world.insert_resource(SmokeFonts(true));
        world.insert_resource(SmokeTour {
            dir,
            phase,
            entered,
            shots: Vec::new(),
        });
        world.init_resource::<Messages<AppExit>>();
        world
    }

    fn step(world: &mut World) {
        let mut schedule = Schedule::default();
        schedule.add_systems(run_smoke);
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
    fn tour_covers_all_views_once() {
        let mut seen = Vec::new();
        for state in AppState::TOUR {
            assert!(!seen.contains(&state), "state {state:?} repeated in tour");
            seen.push(state);
        }
        assert_eq!(seen.len(), 10);
        assert_eq!(seen.first(), Some(&AppState::Menu));
        assert_eq!(seen.last(), Some(&AppState::Fatal));
        assert_eq!(seen[5], AppState::Algs);
        assert_eq!(seen[6], AppState::CaseDrill);
    }

    #[test]
    fn shot_paths_are_unique_per_state() {
        let tour = SmokeTour {
            dir: PathBuf::from("shots"),
            phase: SmokePhase::WaitForBoot { waited: 0.0 },
            entered: Vec::new(),
            shots: Vec::new(),
        };
        let mut names: Vec<String> = AppState::TOUR
            .iter()
            .map(|state| tour.shot_path(*state).to_string_lossy().to_string())
            .collect();
        let total = names.len();
        names.sort();
        names.dedup();
        assert_eq!(names.len(), total);
    }

    #[test]
    fn smoke_seeds_pin_the_tour_determinism() {
        assert_eq!(smoke_seeds(20261012), (20261012, 20261013, 20261014));
        assert_eq!(smoke_seeds(u64::MAX), (u64::MAX, 0, 1));
    }

    #[test]
    fn wait_for_boot_exits_with_error_one_after_the_stuck_limit() {
        let mut world = smoke_world(
            tour_dir("boot-stuck"),
            SmokePhase::WaitForBoot { waited: 9.95 },
            Vec::new(),
            AppState::Bootstrap,
            0.1,
        );
        step(&mut world);
        assert!(matches!(
            world.resource::<SmokeTour>().phase,
            SmokePhase::WaitForBoot { .. }
        ));
        step(&mut world);
        assert_eq!(exit_taken(&mut world), Some(1));
        assert!(matches!(
            world.resource::<SmokeTour>().phase,
            SmokePhase::Done
        ));
    }

    #[test]
    fn wait_for_boot_needs_both_menu_and_ready_fonts() {
        let mut world = smoke_world(
            tour_dir("boot-wait"),
            SmokePhase::WaitForBoot { waited: 0.0 },
            Vec::new(),
            AppState::Menu,
            0.1,
        );
        world.insert_resource(SmokeFonts(false));
        step(&mut world);
        let waited = match &world.resource::<SmokeTour>().phase {
            SmokePhase::WaitForBoot { waited } => *waited,
            other => panic!("must keep waiting for boot, got {other:?}"),
        };
        assert!((waited - 0.1).abs() < 1e-6);
        world.insert_resource(State::<AppState>::new(AppState::Bootstrap));
        world.insert_resource(SmokeFonts(true));
        step(&mut world);
        let waited = match &world.resource::<SmokeTour>().phase {
            SmokePhase::WaitForBoot { waited } => *waited,
            other => panic!("must keep waiting for boot, got {other:?}"),
        };
        assert!((waited - 0.2).abs() < 1e-6);
        world.insert_resource(State::<AppState>::new(AppState::Menu));
        step(&mut world);
        let tour = world.resource::<SmokeTour>();
        assert!(matches!(
            tour.phase,
            SmokePhase::Dwell {
                state: AppState::Menu,
                ..
            }
        ));
        assert_eq!(tour.entered, vec![AppState::Menu]);
    }

    #[test]
    fn dwell_shoots_once_then_waits_for_the_file() {
        let dir = tour_dir("dwell-shoot");
        let mut world = smoke_world(
            dir.clone(),
            SmokePhase::Dwell {
                state: AppState::Menu,
                waited: 0.45,
                stuck: 0.0,
            },
            vec![AppState::Menu],
            AppState::Menu,
            0.1,
        );
        step(&mut world);
        let tour = world.resource::<SmokeTour>();
        assert_eq!(tour.shots.len(), 1);
        let (shot_state, shot_path) = &tour.shots[0];
        assert_eq!(*shot_state, AppState::Menu);
        assert_eq!(*shot_path, dir.join("menu.png"));
        assert!(matches!(
            tour.phase,
            SmokePhase::WaitForShot {
                state: AppState::Menu,
                ..
            }
        ));
        let mut shots = world.query::<&Screenshot>();
        assert_eq!(shots.iter(&world).count(), 1);
    }

    #[test]
    fn dwell_times_out_with_error_three_when_the_state_never_arrives() {
        let mut world = smoke_world(
            tour_dir("dwell-stuck"),
            SmokePhase::Dwell {
                state: AppState::Memo,
                waited: 0.0,
                stuck: 9.95,
            },
            vec![AppState::Memo],
            AppState::Menu,
            0.1,
        );
        step(&mut world);
        assert_eq!(exit_taken(&mut world), Some(3));
        assert!(matches!(
            world.resource::<SmokeTour>().phase,
            SmokePhase::Done
        ));
    }

    #[test]
    fn wait_for_shot_blocks_until_the_file_lands_then_advances() {
        let dir = tour_dir("shot-wait");
        let mut world = smoke_world(
            dir.clone(),
            SmokePhase::WaitForShot {
                state: AppState::Menu,
                path: dir.join("menu.png"),
            },
            vec![AppState::Menu],
            AppState::Menu,
            0.1,
        );
        step(&mut world);
        assert!(matches!(
            world.resource::<SmokeTour>().phase,
            SmokePhase::WaitForShot { .. }
        ));
        std::fs::write(dir.join("menu.png"), b"png").expect("shot lands");
        step(&mut world);
        let tour = world.resource::<SmokeTour>();
        assert_eq!(tour.entered, vec![AppState::Menu, AppState::Drill]);
        assert!(matches!(
            tour.phase,
            SmokePhase::Dwell {
                state: AppState::Drill,
                ..
            }
        ));
        assert!(matches!(
            world.resource::<NextState<AppState>>(),
            NextState::Pending(AppState::Drill)
        ));
        assert_eq!(world.resource::<FatalReason>().0, "");
    }

    #[test]
    fn the_fatal_stop_sets_the_fatal_reason() {
        let dir = tour_dir("fatal-stop");
        std::fs::write(dir.join("settings.png"), b"png").expect("shot lands");
        let mut world = smoke_world(
            dir.clone(),
            SmokePhase::WaitForShot {
                state: AppState::Settings,
                path: dir.join("settings.png"),
            },
            AppState::TOUR[..9].to_vec(),
            AppState::Settings,
            0.1,
        );
        step(&mut world);
        let tour = world.resource::<SmokeTour>();
        assert_eq!(tour.entered.len(), AppState::TOUR.len());
        assert!(matches!(
            tour.phase,
            SmokePhase::Dwell {
                state: AppState::Fatal,
                ..
            }
        ));
        assert_eq!(world.resource::<FatalReason>().0, SMOKE_FATAL_REASON);
        assert!(matches!(
            world.resource::<NextState<AppState>>(),
            NextState::Pending(AppState::Fatal)
        ));
    }

    #[test]
    fn a_full_tour_with_landed_shots_exits_successfully() {
        let mut world = smoke_world(
            tour_dir("full-tour"),
            SmokePhase::WaitForBoot { waited: 0.0 },
            Vec::new(),
            AppState::Menu,
            0.1,
        );
        for _ in 0..400 {
            step(&mut world);
            let pending = {
                let tour = world.resource::<SmokeTour>();
                match &tour.phase {
                    SmokePhase::Done => None,
                    SmokePhase::WaitForShot { path, .. } => Some((path.clone(), None)),
                    SmokePhase::Dwell { state, .. } => Some((PathBuf::new(), Some(*state))),
                    SmokePhase::WaitForBoot { .. } => Some((PathBuf::new(), None)),
                }
            };
            let Some((shot_path, dwell_state)) = pending else {
                break;
            };
            if let Some(state) = dwell_state {
                world.insert_resource(State::<AppState>::new(state));
            } else if !shot_path.as_os_str().is_empty()
                && !matches!(std::fs::metadata(&shot_path), Ok(meta) if meta.len() > 0)
            {
                std::fs::write(&shot_path, b"png").expect("shot lands");
            }
        }
        let tour = world.resource::<SmokeTour>();
        assert!(
            matches!(tour.phase, SmokePhase::Done),
            "tour must end Done, phase is {:?}",
            tour.phase
        );
        assert_eq!(tour.entered, AppState::TOUR.to_vec());
        assert_eq!(tour.shots.len(), AppState::TOUR.len());
        assert_eq!(world.resource::<FatalReason>().0, SMOKE_FATAL_REASON);
        assert_eq!(exit_taken(&mut world), Some(0));
    }

    fn landed_tour(dir: &Path, entered: Vec<AppState>, bytes: &[u8]) -> SmokeTour {
        let shots: Vec<(AppState, PathBuf)> = AppState::TOUR
            .iter()
            .map(|state| (*state, dir.join(format!("{}.png", state.slug()))))
            .collect();
        for (_, path) in &shots {
            std::fs::write(path, bytes).expect("shot file");
        }
        SmokeTour {
            dir: dir.to_path_buf(),
            phase: SmokePhase::Done,
            entered,
            shots,
        }
    }

    #[test]
    fn finish_accepts_only_complete_and_ordered_tours() {
        let ok_dir = tour_dir("finish-ok");
        let mut world = World::new();
        world.init_resource::<Messages<AppExit>>();
        let tour = landed_tour(&ok_dir, AppState::TOUR.to_vec(), b"png");
        {
            let mut commands = world.commands();
            finish(&mut commands, &tour);
        }
        world.flush();
        assert_eq!(exit_taken(&mut world), Some(0));
    }

    #[test]
    fn finish_rejects_empty_shot_files() {
        let dir = tour_dir("finish-empty");
        let mut world = World::new();
        world.init_resource::<Messages<AppExit>>();
        let tour = landed_tour(&dir, AppState::TOUR.to_vec(), b"");
        {
            let mut commands = world.commands();
            finish(&mut commands, &tour);
        }
        world.flush();
        assert_eq!(exit_taken(&mut world), Some(1));
    }

    #[test]
    fn finish_rejects_out_of_order_tours() {
        let dir = tour_dir("finish-order");
        let mut world = World::new();
        world.init_resource::<Messages<AppExit>>();
        let mut reversed = AppState::TOUR.to_vec();
        reversed.reverse();
        let tour = landed_tour(&dir, reversed, b"png");
        {
            let mut commands = world.commands();
            finish(&mut commands, &tour);
        }
        world.flush();
        assert_eq!(exit_taken(&mut world), Some(1));
    }

    #[test]
    fn finish_rejects_shot_paths_named_for_another_state() {
        let dir = tour_dir("finish-renamed");
        let mut world = World::new();
        world.init_resource::<Messages<AppExit>>();
        let mut tour = landed_tour(&dir, AppState::TOUR.to_vec(), b"png");
        let stray = dir.parent().expect("parent").join("renamed.png");
        std::fs::write(&stray, b"png").expect("stray file");
        tour.shots.last_mut().expect("last shot").1 = stray;
        {
            let mut commands = world.commands();
            finish(&mut commands, &tour);
        }
        world.flush();
        assert_eq!(exit_taken(&mut world), Some(1));
    }

    #[test]
    fn shoot_removes_a_stale_file_before_spawning() {
        let dir = tour_dir("shoot-stale");
        let path = dir.join("menu.png");
        std::fs::write(&path, b"stale").expect("stale file");
        let mut world = World::new();
        {
            let mut commands = world.commands();
            shoot(&mut commands, path.clone());
        }
        world.flush();
        assert!(std::fs::metadata(&path).is_err(), "stale shot must go");
        let mut shots = world.query::<&Screenshot>();
        assert_eq!(shots.iter(&world).count(), 1);
    }

    #[test]
    fn exit_with_maps_the_verdict_to_the_exit_code() {
        let mut world = World::new();
        world.init_resource::<Messages<AppExit>>();
        {
            let mut commands = world.commands();
            exit_with(&mut commands, true);
        }
        world.flush();
        assert_eq!(exit_taken(&mut world), Some(0));
        {
            let mut commands = world.commands();
            exit_with(&mut commands, false);
        }
        world.flush();
        assert_eq!(exit_taken(&mut world), Some(1));
    }

    #[test]
    fn plugin_build_creates_the_dir_and_pins_the_seeds() {
        let dir = std::env::temp_dir()
            .join("bt-app-smoke-tests")
            .join(format!(
                "plugin-{}-{}",
                std::process::id(),
                DIR_COUNTER.fetch_add(1, Ordering::Relaxed)
            ));
        let mut app = App::new();
        app.add_plugins(MinimalPlugins);
        app.add_plugins(SmokePlugin {
            dir: dir.clone(),
            seed: 7,
        });
        assert!(dir.is_dir(), "plugin must create the output dir");
        assert_eq!(app.world().resource::<crate::views::memo::MemoSeed>().0, 7);
        assert_eq!(
            app.world().resource::<crate::views::solve::SolveSeed>().0,
            8
        );
        assert_eq!(
            app.world().resource::<crate::views::drill::DrillSeed>().0,
            9
        );
        assert!(!app.world().resource::<SmokeFonts>().0);
        let tour = app.world().resource::<SmokeTour>();
        assert!(matches!(
            tour.phase,
            SmokePhase::WaitForBoot { waited: 0.0 }
        ));
        assert!(tour.entered.is_empty());
        assert!(tour.shots.is_empty());
        let pending = app.world().resource::<PendingCaseDrill>();
        assert_eq!(pending.0.len(), 1, "the tour arms exactly one case drill");
        let case = &pending.0[0];
        assert_eq!(case.case.event, "3x3");
        assert!(
            crate::views::case_drill::case_facelets(&case.case.event, case.size, &case.alg).is_ok(),
            "the smoke case must render a net"
        );
    }
}
