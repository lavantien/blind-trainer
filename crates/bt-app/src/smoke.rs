use crate::logic::FatalReason;
use crate::present::FontBook;
use crate::states::AppState;
use bevy::prelude::*;
use bevy::render::view::screenshot::{Screenshot, save_to_disk};
use std::num::NonZero;
use std::path::PathBuf;

const DWELL_SECS: f32 = 0.5;
const STUCK_LIMIT_SECS: f32 = 10.0;
const SMOKE_FATAL_REASON: &str = "smoke tour complete";

pub struct SmokePlugin {
    pub dir: PathBuf,
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
        app.insert_resource(SmokeTour {
            dir: self.dir.clone(),
            phase: SmokePhase::WaitForBoot { waited: 0.0 },
            entered: Vec::new(),
            shots: Vec::new(),
        })
        .init_resource::<SmokeFonts>()
        .add_systems(Update, (track_fonts, run_smoke).chain());
    }
}

#[derive(Clone, Copy, Debug, Default, Resource)]
struct SmokeFonts(bool);

fn track_fonts(mut ready: ResMut<SmokeFonts>, server: Res<AssetServer>, fonts: Res<FontBook>) {
    ready.0 = server.is_loaded_with_dependencies(&fonts.regular)
        && server.is_loaded_with_dependencies(&fonts.bold);
}

fn take_shot(commands: &mut Commands, tour: &SmokeTour, state: AppState) -> PathBuf {
    let path = tour.shot_path(state);
    commands
        .spawn(Screenshot::primary_window())
        .observe(save_to_disk(path.clone()));
    path
}

fn exit_with(commands: &mut Commands, ok: bool) {
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
                    let path = take_shot(&mut commands, &tour, target);
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

    #[test]
    fn tour_covers_all_views_once() {
        let mut seen = Vec::new();
        for state in AppState::TOUR {
            assert!(!seen.contains(&state), "state {state:?} repeated in tour");
            seen.push(state);
        }
        assert_eq!(seen.len(), 8);
        assert_eq!(seen.first(), Some(&AppState::Menu));
        assert_eq!(seen.last(), Some(&AppState::Fatal));
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
}
