use crate::states::{AppState, CaseDrillPhase, DrillPhase, MemoPhase, SolvePhase};
use bevy::prelude::*;

#[derive(Clone, Copy, Debug)]
pub(crate) enum Action {
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
    WaitCaseDrill(CaseDrillPhase),
    Elapse(f32),
    Shot(&'static str),
}

pub(crate) fn script(cadence_secs: f32) -> Vec<Action> {
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
        Action::NavOpen(AppState::Algs),
        Action::WaitApp(AppState::Algs),
        Action::Press(KeyCode::ArrowRight),
        Action::Elapse(0.1),
        Action::Press(KeyCode::ArrowDown),
        Action::Elapse(0.1),
        Action::Press(KeyCode::ArrowRight),
        Action::Elapse(0.1),
        Action::Press(KeyCode::ArrowRight),
        Action::Elapse(0.3),
        Action::Press(KeyCode::KeyD),
        Action::WaitApp(AppState::CaseDrill),
        Action::WaitCaseDrill(CaseDrillPhase::Show),
        Action::Shot("case-drill-show"),
        Action::Press(KeyCode::Space),
        Action::WaitCaseDrill(CaseDrillPhase::Grade),
        Action::Shot("case-drill-grade"),
        Action::Press(KeyCode::Digit1),
        Action::Elapse(0.1),
        Action::WaitCaseDrill(CaseDrillPhase::Show),
        Action::Press(KeyCode::Space),
        Action::Elapse(0.1),
        Action::WaitCaseDrill(CaseDrillPhase::Grade),
        Action::Press(KeyCode::Digit2),
        Action::Elapse(0.1),
        Action::WaitCaseDrill(CaseDrillPhase::Show),
        Action::Press(KeyCode::Space),
        Action::Elapse(0.1),
        Action::WaitCaseDrill(CaseDrillPhase::Grade),
        Action::Press(KeyCode::Digit3),
        Action::Elapse(0.1),
        Action::WaitCaseDrill(CaseDrillPhase::Show),
        Action::Press(KeyCode::Space),
        Action::Elapse(0.1),
        Action::WaitCaseDrill(CaseDrillPhase::Grade),
        Action::Press(KeyCode::Digit4),
        Action::Elapse(0.1),
        Action::WaitCaseDrill(CaseDrillPhase::Show),
        Action::Press(KeyCode::Escape),
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

pub(crate) fn fatal_script() -> Vec<Action> {
    vec![
        Action::WaitApp(AppState::Fatal),
        Action::WaitFonts,
        Action::Shot("fatal"),
    ]
}

#[cfg(test)]
mod tests {
    use super::*;

    fn shot_names(actions: &[Action]) -> Vec<&'static str> {
        actions
            .iter()
            .filter_map(|action| match action {
                Action::Shot(name) => Some(*name),
                _ => None,
            })
            .collect()
    }

    fn position(actions: &[Action], pred: &dyn Fn(&Action) -> bool) -> usize {
        actions
            .iter()
            .position(pred)
            .unwrap_or_else(|| panic!("action missing"))
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
            "case-drill-show",
            "case-drill-grade",
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
    fn the_case_drill_leg_drives_the_repo_and_all_four_grade_keys() {
        let actions = script(0.8);
        let entry = position(&actions, &|a| {
            matches!(a, Action::WaitApp(AppState::CaseDrill))
        });
        let mut nav: Vec<KeyCode> = actions[..entry]
            .iter()
            .rev()
            .filter_map(|action| match action {
                Action::Press(key) => Some(*key),
                _ => None,
            })
            .take(5)
            .collect();
        nav.reverse();
        assert_eq!(
            nav,
            [
                KeyCode::ArrowRight,
                KeyCode::ArrowDown,
                KeyCode::ArrowRight,
                KeyCode::ArrowRight,
                KeyCode::KeyD,
            ],
            "the leg focuses the repo, picks the first event arm, deepens to its stage, then d"
        );
        let exit = actions[entry..]
            .iter()
            .position(|a| matches!(a, Action::WaitApp(AppState::Menu)))
            .expect("case drill exit")
            + entry;
        assert!(matches!(actions[exit - 1], Action::Press(KeyCode::Escape)));
        let drills: Vec<KeyCode> = actions[entry..exit]
            .iter()
            .filter_map(|action| match action {
                Action::Press(key) => Some(*key),
                _ => None,
            })
            .collect();
        assert_eq!(
            drills,
            [
                KeyCode::Space,
                KeyCode::Digit1,
                KeyCode::Space,
                KeyCode::Digit2,
                KeyCode::Space,
                KeyCode::Digit3,
                KeyCode::Space,
                KeyCode::Digit4,
                KeyCode::Escape,
            ],
            "each case reveals then grades with a distinct key, escape exits the session"
        );
        assert!(matches!(
            actions[exit + 1],
            Action::NavOpen(AppState::Stats)
        ));
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
}
