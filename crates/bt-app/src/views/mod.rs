pub mod drill;
pub mod menu;

use crate::logic::RuntimePaths;
use crate::persist::Stores;
use crate::states::{AppState, DrillPhase};
use bevy::prelude::*;
use bt_scheme::CaseId;
use std::time::{SystemTime, UNIX_EPOCH};

#[derive(SystemSet, Clone, Copy, PartialEq, Eq, Hash, Debug)]
pub struct ViewsLogic;

#[derive(Clone, Copy, Debug, Resource, Default)]
pub struct PendingDrill(pub Option<CaseId>);

#[derive(Clone, Copy, Debug, Resource)]
pub struct NowDay(pub i64);

pub fn now_day() -> i64 {
    now_unix_secs() / 86_400
}

pub fn now_unix_secs() -> i64 {
    SystemTime::now()
        .duration_since(UNIX_EPOCH)
        .map(|duration| duration.as_secs() as i64)
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
            .insert_resource(drill::DrillSeed(time_seed()))
            .insert_resource(drill::DrillDraft::default())
            .insert_resource(drill::DrillSession::default())
            .configure_sets(Update, ViewsLogic.run_if(resource_exists::<Stores>))
            .add_systems(
                Update,
                (
                    IntoScheduleConfigs::run_if(drill::swap_target, in_state(AppState::Drill)),
                    IntoScheduleConfigs::run_if(drill::ready_keys, in_state(DrillPhase::Ready)),
                    IntoScheduleConfigs::run_if(drill::input_keys, in_state(DrillPhase::Memo)),
                    IntoScheduleConfigs::run_if(drill::verdict_keys, in_state(DrillPhase::Exec)),
                    IntoScheduleConfigs::run_if(drill::review_keys, in_state(DrillPhase::Review)),
                )
                    .in_set(ViewsLogic),
            )
            .add_systems(
                OnEnter(AppState::Drill),
                (drill::enter, drill::sync_net).chain(),
            )
            .add_systems(OnEnter(DrillPhase::Ready), drill::enter_ready)
            .add_systems(OnEnter(DrillPhase::Memo), drill::enter_memo)
            .add_systems(OnEnter(DrillPhase::Exec), drill::enter_exec)
            .add_systems(OnEnter(DrillPhase::Review), drill::enter_review);
    }
}
