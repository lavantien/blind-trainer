use bevy::prelude::*;

#[derive(States, Clone, Copy, PartialEq, Eq, Hash, Debug, Default)]
pub enum AppState {
    #[default]
    Bootstrap,
    Fatal,
    Menu,
    Drill,
    Memo,
    Solve,
    Algs,
    Stats,
    Settings,
}

impl AppState {
    pub const TOUR: [AppState; 8] = [
        AppState::Menu,
        AppState::Drill,
        AppState::Memo,
        AppState::Solve,
        AppState::Algs,
        AppState::Stats,
        AppState::Settings,
        AppState::Fatal,
    ];

    pub const MENU_KEYS: [(KeyCode, AppState); 6] = [
        (KeyCode::Digit1, AppState::Drill),
        (KeyCode::Digit2, AppState::Memo),
        (KeyCode::Digit3, AppState::Solve),
        (KeyCode::Digit4, AppState::Algs),
        (KeyCode::Digit5, AppState::Stats),
        (KeyCode::Digit6, AppState::Settings),
    ];

    pub fn slug(self) -> &'static str {
        match self {
            AppState::Bootstrap => "bootstrap",
            AppState::Fatal => "fatal",
            AppState::Menu => "menu",
            AppState::Drill => "drill",
            AppState::Memo => "memo",
            AppState::Solve => "solve",
            AppState::Algs => "algs",
            AppState::Stats => "stats",
            AppState::Settings => "settings",
        }
    }

    pub fn label(self) -> &'static str {
        match self {
            AppState::Bootstrap => "booting",
            AppState::Fatal => "fatal",
            AppState::Menu => "menu",
            AppState::Drill => "drill",
            AppState::Memo => "memo training",
            AppState::Solve => "solve",
            AppState::Algs => "algs",
            AppState::Stats => "stats",
            AppState::Settings => "settings",
        }
    }
}

#[derive(SubStates, Clone, Copy, PartialEq, Eq, Hash, Debug, Default)]
#[source(AppState = AppState::Drill)]
pub enum DrillPhase {
    #[default]
    Card,
    Reps,
    Rate,
}

#[derive(SubStates, Clone, Copy, PartialEq, Eq, Hash, Debug, Default)]
#[source(AppState = AppState::Solve)]
pub enum SolvePhase {
    #[default]
    Ready,
    Memo,
    Exec,
    Review,
}

#[derive(SubStates, Clone, Copy, PartialEq, Eq, Hash, Debug, Default)]
#[source(AppState = AppState::Memo)]
pub enum MemoPhase {
    #[default]
    Intro,
    Stream,
    Recall,
    Review,
}
