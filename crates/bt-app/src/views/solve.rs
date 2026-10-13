use crate::logic::{NetState, SaveStores, VendorCorpus};
use crate::persist::{SolveRecord, Stores};
use crate::states::{AppState, SolvePhase};
use crate::views::drill::sheet_of;
use crate::views::memo::{StreamToken, build_tokens};
use crate::views::{SyllableBook, now_unix_ms};
use bevy::input::ButtonState;
use bevy::input::keyboard::KeyboardInput;
use bevy::prelude::*;
use bt_core::Cube;
use bt_data::corpus::{Corpus, SheetClass, Solver};
use bt_scheme::memo::memo as memo_of;
use bt_scheme::{Memo, Residue, ResidueClass, name_of, random_state, same_piece};

#[derive(Clone, Copy, Debug, Resource, Default)]
pub struct SolveSeed(pub u64);

#[derive(Clone, Copy, Debug, Resource, Default)]
pub struct SolveScrambleOverride(pub Option<Cube>);

#[derive(Clone, PartialEq, Eq, Debug)]
pub struct SolveOutcome {
    pub success: bool,
    pub broken: Vec<String>,
}

#[derive(Clone, Debug)]
pub struct SolveFinished {
    pub record: SolveRecord,
    pub outcome: SolveOutcome,
}

#[derive(Clone, Debug, Resource)]
pub struct SolveSession {
    pub seed: u64,
    pub scramble: Cube,
    pub memo: Option<Memo>,
    pub tokens: Vec<StreamToken>,
    pub shown: usize,
    pub clock: f32,
    pub memo_secs: f64,
    pub exec_secs: f64,
    pub started_ms: i64,
    pub finished: Option<SolveFinished>,
}

impl Default for SolveSession {
    fn default() -> SolveSession {
        SolveSession {
            seed: 0,
            scramble: Cube::solved(),
            memo: None,
            tokens: Vec::new(),
            shown: 0,
            clock: 0.0,
            memo_secs: 0.0,
            exec_secs: 0.0,
            started_ms: 0,
            finished: None,
        }
    }
}

fn pick_row<'a>(
    corpus: &'a Corpus,
    sheet: SheetClass,
    position: &str,
) -> Option<&'a bt_data::corpus::Row> {
    let rows = corpus.rows_for_position(sheet, position);
    rows.iter()
        .find(|row| row.solver == Solver::User)
        .or_else(|| rows.first())
        .copied()
}

pub fn pair_position(buffer: u8, case: &bt_scheme::CaseId) -> String {
    format!(
        "{}-{}-{}",
        name_of(buffer),
        name_of(case.t1),
        name_of(case.t2)
    )
}

pub fn execute_residue(
    corpus: &Corpus,
    cube: &mut Cube,
    residue: &Residue,
    broken: &mut Vec<String>,
) {
    match residue.class() {
        ResidueClass::None => {}
        ResidueClass::FlipPair(first, second) => apply_special(
            corpus,
            cube,
            SheetClass::Flips,
            &format!("{} & {}", name_of(first), name_of(second)),
            broken,
        ),
        ResidueClass::TwistPair(first, second) => {
            let canonical = format!("{} & {}", name_of(first), name_of(second));
            let swapped = format!("{} & {}", name_of(second), name_of(first));
            if pick_row(corpus, SheetClass::Twists, &canonical).is_none()
                && pick_row(corpus, SheetClass::Twists, &swapped).is_some()
            {
                apply_special(corpus, cube, SheetClass::Twists, &swapped, broken);
            } else {
                apply_special(corpus, cube, SheetClass::Twists, &canonical, broken);
            }
        }
        ResidueClass::Parity => {
            let parity = residue.parity.expect("class Parity implies parity");
            apply_special(
                corpus,
                cube,
                SheetClass::Parity,
                &format!(
                    "{}-{}, {}-{}",
                    name_of(parity.edge.0),
                    name_of(parity.edge.1),
                    name_of(parity.corner.0),
                    name_of(parity.corner.1)
                ),
                broken,
            );
        }
        ResidueClass::Ltct => {
            let parity = residue.parity.expect("class Ltct implies parity");
            let bracket = residue
                .twists
                .iter()
                .map(|twist| twist.corner)
                .find(|corner| {
                    !same_piece(*corner, parity.corner.0) && !same_piece(*corner, parity.corner.1)
                });
            match bracket {
                Some(bracket) => apply_special(
                    corpus,
                    cube,
                    SheetClass::Ltct,
                    &format!(
                        "{}-{}, {}-{}[{}]",
                        name_of(parity.edge.0),
                        name_of(parity.edge.1),
                        name_of(parity.corner.0),
                        name_of(parity.corner.1),
                        name_of(bracket)
                    ),
                    broken,
                ),
                None => broken.push("ltct without a bracketed corner".to_string()),
            }
        }
        other => broken.push(format!("residue unsupported: {other:?}")),
    }
}

fn apply_special(
    corpus: &Corpus,
    cube: &mut Cube,
    sheet: SheetClass,
    position: &str,
    broken: &mut Vec<String>,
) {
    match pick_row(corpus, sheet, position) {
        Some(row) => cube.apply_alg(&row.alg),
        None => broken.push(format!("{position} uncovered")),
    }
}

pub fn simulate(corpus: &Corpus, scramble: &Cube, memo: &Memo) -> SolveOutcome {
    let mut cube = *scramble;
    let mut broken = Vec::new();
    for (case, buffer) in memo
        .pairs
        .0
        .iter()
        .zip(memo.buffers.0.iter())
        .chain(memo.pairs.1.iter().zip(memo.buffers.1.iter()))
    {
        let position = pair_position(*buffer, case);
        match pick_row(corpus, sheet_of(case.orbit), &position) {
            Some(row) => cube.apply_alg(&row.alg),
            None => broken.push(format!("{position} uncovered")),
        }
    }
    execute_residue(corpus, &mut cube, &memo.residue, &mut broken);
    SolveOutcome {
        success: broken.is_empty() && cube.is_solved(),
        broken,
    }
}

pub fn rebuild(
    session: &mut SolveSession,
    seed: u64,
    book: &SyllableBook,
    override_scramble: Option<Cube>,
) {
    let scramble = override_scramble.unwrap_or_else(|| random_state(seed));
    let memo = memo_of(&scramble);
    session.tokens = build_tokens(&memo, book);
    session.memo = Some(memo);
    session.scramble = scramble;
    session.seed = seed;
    session.shown = 0;
    session.clock = 0.0;
    session.memo_secs = 0.0;
    session.exec_secs = 0.0;
    session.started_ms = 0;
    session.finished = None;
}

pub fn enter(
    mut session: ResMut<SolveSession>,
    seed: Res<SolveSeed>,
    book: Res<SyllableBook>,
    mut scramble_override: ResMut<SolveScrambleOverride>,
    mut net: ResMut<NetState>,
    mut next: ResMut<NextState<SolvePhase>>,
) {
    rebuild(&mut session, seed.0, &book, scramble_override.0.take());
    net.cube = session.scramble;
    net.buffer_sticker = None;
    net.targets = [None, None];
    next.set(SolvePhase::Ready);
}

pub fn enter_ready(mut messages: ResMut<Messages<KeyboardInput>>) {
    messages.clear();
}

pub fn enter_memo(mut messages: ResMut<Messages<KeyboardInput>>) {
    messages.clear();
}

pub fn enter_exec(mut messages: ResMut<Messages<KeyboardInput>>) {
    messages.clear();
}

pub fn enter_review(mut messages: ResMut<Messages<KeyboardInput>>) {
    messages.clear();
}

pub fn ready_keys(
    mut inputs: MessageReader<KeyboardInput>,
    mut session: ResMut<SolveSession>,
    mut next: ResMut<NextState<SolvePhase>>,
) {
    for input in inputs.read() {
        if input.state == ButtonState::Pressed && input.key_code == KeyCode::Space {
            session.started_ms = now_unix_ms();
            session.memo_secs = 0.0;
            session.shown = 0;
            session.clock = 0.0;
            next.set(SolvePhase::Memo);
            return;
        }
    }
}

pub fn memo_tick(time: Res<Time<Virtual>>, stores: Res<Stores>, mut session: ResMut<SolveSession>) {
    let dt = time.delta_secs();
    session.memo_secs += dt as f64;
    let cadence = stores.settings.memo_cadence_ms as f32 / 1000.0;
    if cadence <= 0.0 {
        session.shown = session.tokens.len();
        return;
    }
    session.clock += dt;
    while session.clock >= cadence && session.shown < session.tokens.len() {
        session.clock -= cadence;
        session.shown += 1;
    }
}

pub fn memo_keys(
    mut inputs: MessageReader<KeyboardInput>,
    mut session: ResMut<SolveSession>,
    mut next: ResMut<NextState<SolvePhase>>,
) {
    for input in inputs.read() {
        if input.state == ButtonState::Pressed && input.key_code == KeyCode::Space {
            session.exec_secs = 0.0;
            next.set(SolvePhase::Exec);
            return;
        }
    }
}

pub fn exec_tick(time: Res<Time<Virtual>>, mut session: ResMut<SolveSession>) {
    session.exec_secs += time.delta_secs() as f64;
}

pub fn exec_keys(
    mut commands: Commands,
    mut inputs: MessageReader<KeyboardInput>,
    mut session: ResMut<SolveSession>,
    corpus: Res<VendorCorpus>,
    mut stores: ResMut<Stores>,
    mut next: ResMut<NextState<SolvePhase>>,
) {
    for input in inputs.read() {
        if input.state == ButtonState::Pressed && input.key_code == KeyCode::Space {
            let memo_secs = session.memo_secs;
            let exec_secs = session.exec_secs;
            let record = SolveRecord {
                started_ms: session.started_ms,
                memo_ms: (memo_secs * 1000.0).round() as i64,
                exec_ms: (exec_secs * 1000.0).round() as i64,
                success: false,
            };
            let outcome = session
                .memo
                .as_ref()
                .map(|memo| simulate(&corpus.0, &session.scramble, memo))
                .unwrap_or(SolveOutcome {
                    success: false,
                    broken: vec!["no memo".to_string()],
                });
            let record = SolveRecord {
                success: outcome.success,
                ..record
            };
            stores.solves.solves.push(record);
            commands.write_message(SaveStores);
            session.finished = Some(SolveFinished { record, outcome });
            next.set(SolvePhase::Review);
            return;
        }
    }
}

pub fn review_keys(
    mut inputs: MessageReader<KeyboardInput>,
    mut session: ResMut<SolveSession>,
    mut seed: ResMut<SolveSeed>,
    book: Res<SyllableBook>,
    mut net: ResMut<NetState>,
    mut next: ResMut<NextState<SolvePhase>>,
) {
    for input in inputs.read() {
        if input.state == ButtonState::Pressed && input.key_code == KeyCode::Space {
            seed.0 = seed.0.wrapping_add(1);
            rebuild(&mut session, seed.0, &book, None);
            net.cube = session.scramble;
            next.set(SolvePhase::Ready);
            return;
        }
    }
}

pub fn review_lines(session: &SolveSession) -> Vec<String> {
    let mut lines = Vec::new();
    if let Some(finished) = &session.finished {
        lines.push(if finished.outcome.success {
            "solved".to_string()
        } else {
            "failed".to_string()
        });
        lines.push(format!(
            "memo {:.1}s",
            finished.record.memo_ms as f64 / 1000.0
        ));
        lines.push(format!(
            "exec {:.1}s",
            finished.record.exec_ms as f64 / 1000.0
        ));
        lines.push(format!(
            "total {:.1}s",
            (finished.record.memo_ms + finished.record.exec_ms) as f64 / 1000.0
        ));
        lines.extend(finished.outcome.broken.iter().cloned());
    }
    lines
}

pub fn phase_word(phase: SolvePhase) -> &'static str {
    match phase {
        SolvePhase::Ready => "ready",
        SolvePhase::Memo => "memo",
        SolvePhase::Exec => "exec",
        SolvePhase::Review => "review",
    }
}

pub fn phase_label(phase: SolvePhase, session: &SolveSession) -> String {
    format!("{}, seed {}", phase_word(phase), session.seed)
}

pub fn hint_text(phase: SolvePhase) -> &'static str {
    match phase {
        SolvePhase::Ready => "space to start memo",
        SolvePhase::Memo => "space to finish memo",
        SolvePhase::Exec => "space to stop",
        SolvePhase::Review => "space next solve",
    }
}

pub fn focal_text(phase: SolvePhase, session: &SolveSession) -> String {
    match phase {
        SolvePhase::Memo => format!("{}/{}", session.shown, session.tokens.len()),
        _ => String::new(),
    }
}

pub fn timer_text(phase: SolvePhase, session: &SolveSession) -> String {
    match phase {
        SolvePhase::Ready => format!("{} pairs", session.tokens.len()),
        SolvePhase::Memo => format!("memo {:.1}s", session.memo_secs),
        SolvePhase::Exec => format!("exec {:.1}s", session.exec_secs),
        SolvePhase::Review => String::new(),
    }
}

pub fn review_parts(session: &SolveSession) -> (String, String, String) {
    let lines = review_lines(session);
    let result = lines.first().cloned().unwrap_or_default();
    let split_end = lines.len().min(4);
    let splits = lines.get(1..split_end).unwrap_or_default().join("\n");
    let broken = lines.get(split_end..).unwrap_or_default().join("\n");
    (result, splits, broken)
}

#[cfg(test)]
mod tests {
    use super::*;
    use crate::views::SyllableBook;
    use bt_core::Alg;
    use bt_data::corpus::Row;
    use bt_scheme::{Orbit, apply_sticker_cycles, state_array};

    fn row(sheet: SheetClass, position: &str, alg: &str) -> Row {
        Row {
            solver: Solver::Cherry,
            sheet,
            rowid: 1,
            position: position.to_string(),
            alg: Alg::parse(alg).expect("fixture alg parses"),
            alg_text: alg.to_string(),
            commutator: String::new(),
            recon_link: String::new(),
            youtube_link: String::new(),
        }
    }

    fn single_pair_solve(first: char, second: char) -> (Memo, Cube) {
        let t1 = bt_scheme::sticker_of(Orbit::Edge, first);
        let t2 = bt_scheme::sticker_of(Orbit::Edge, second);
        let mut occ = state_array(&Cube::solved());
        let inverse =
            bt_scheme::ideal_cycle_at(Orbit::Edge, bt_scheme::EDGE_BUFFER_STICKER, t2, t1);
        apply_sticker_cycles(&mut occ, &inverse);
        let scramble = Cube::from_occ(occ);
        let memo = memo_of(&scramble);
        (memo, scramble)
    }

    #[test]
    fn simulate_passes_when_the_covered_alg_solves_the_scramble() {
        let corpus = Corpus::from_rows(vec![row(
            SheetClass::Edge,
            "UF-UB-UR",
            "R2 U' S R2 S' R2 U R2",
        )]);
        let (memo, scramble) = single_pair_solve('A', 'B');
        assert_eq!(memo.pairs.0.len(), 1);
        assert_eq!(memo.pairs.0[0].letters(), ('A', 'B'));
        let outcome = simulate(&corpus, &scramble, &memo);
        assert!(outcome.success, "broken: {:?}", outcome.broken);
    }

    #[test]
    fn simulate_reports_the_pair_that_broke_it() {
        let corpus = Corpus::from_rows(Vec::new());
        let (memo, scramble) = single_pair_solve('A', 'B');
        let outcome = simulate(&corpus, &scramble, &memo);
        assert!(!outcome.success);
        assert_eq!(outcome.broken, vec!["UF-UB-UR uncovered".to_string()]);
    }

    #[test]
    fn tokens_are_built_from_the_solve_memo() {
        let (memo, _) = single_pair_solve('A', 'B');
        let tokens = build_tokens(&memo, &SyllableBook::default());
        assert_eq!(tokens.len(), 1);
        assert_eq!(tokens[0].letters, ['A', 'B']);
    }

    #[test]
    fn zone_texts_track_the_phase() {
        let mut session = SolveSession::default();
        rebuild(&mut session, 7, &SyllableBook::default(), None);
        assert_eq!(phase_label(SolvePhase::Ready, &session), "ready, seed 7");
        assert_eq!(
            timer_text(SolvePhase::Ready, &session),
            format!("{} pairs", session.tokens.len())
        );
        assert_eq!(hint_text(SolvePhase::Ready), "space to start memo");
        session.shown = 1;
        session.memo_secs = 2.5;
        assert_eq!(
            focal_text(SolvePhase::Memo, &session),
            format!("1/{}", session.tokens.len())
        );
        assert_eq!(timer_text(SolvePhase::Memo, &session), "memo 2.5s");
        assert_eq!(focal_text(SolvePhase::Exec, &session), "");
        session.exec_secs = 1.2;
        assert_eq!(timer_text(SolvePhase::Exec, &session), "exec 1.2s");
    }

    #[test]
    fn review_parts_split_verdict_splits_and_broken() {
        let mut session = SolveSession::default();
        assert_eq!(
            review_parts(&session),
            (String::new(), String::new(), String::new())
        );
        session.finished = Some(SolveFinished {
            record: SolveRecord {
                started_ms: 1,
                memo_ms: 900,
                exec_ms: 500,
                success: true,
            },
            outcome: SolveOutcome {
                success: true,
                broken: vec!["UF-UB-UR uncovered".to_string()],
            },
        });
        let (result, splits, broken) = review_parts(&session);
        assert_eq!(result, "solved");
        assert_eq!(splits, "memo 0.9s\nexec 0.5s\ntotal 1.4s");
        assert_eq!(broken, "UF-UB-UR uncovered");
    }
}

#[derive(Clone, Copy, Component, Debug, PartialEq, Eq)]
pub struct SolveZone(pub SolveZoneKind);

#[derive(Clone, Copy, Debug, PartialEq, Eq)]
pub enum SolveZoneKind {
    Phase,
    Focal,
    Timer,
    Result,
    Splits,
    Broken,
    Hint,
}

pub fn spawn(
    commands: &mut Commands,
    fonts: &crate::present::FontBook,
    config: &crate::logic::AppConfig,
) -> Entity {
    let content = crate::present::spawn_view_frame(
        commands,
        fonts,
        AppState::Solve,
        "solve",
        "space through ready, memo, exec, review, esc menu",
    );
    commands.entity(content).insert(Node {
        width: Val::Percent(100.0),
        flex_grow: 1.0,
        flex_direction: FlexDirection::Row,
        align_items: AlignItems::Center,
        column_gap: crate::theme::rem(crate::theme::SPACING_REM * 2.0),
        ..default()
    });
    commands.entity(content).with_children(|parent| {
        parent
            .spawn(Node {
                width: Val::Percent(crate::theme::DRILL_NET_WIDTH_PCT * 100.0),
                height: Val::Percent(100.0),
                flex_direction: FlexDirection::Column,
                align_items: AlignItems::Center,
                justify_content: JustifyContent::Center,
                ..default()
            })
            .with_children(|region| {
                crate::net::spawn_net(
                    region,
                    crate::present::net_geometry(config),
                    fonts.bold.clone(),
                );
            });
        parent
            .spawn(Node {
                flex_grow: 1.0,
                flex_direction: FlexDirection::Column,
                justify_content: JustifyContent::Center,
                row_gap: crate::theme::rem(crate::theme::SPACING_REM),
                ..default()
            })
            .with_children(|rail| {
                crate::present::spawn_line(
                    rail,
                    fonts,
                    "",
                    crate::theme::FONT_SMALL,
                    crate::theme::text_muted(),
                    SolveZone(SolveZoneKind::Phase),
                );
                crate::present::spawn_line(
                    rail,
                    fonts,
                    "",
                    crate::theme::FONT_FOCAL,
                    crate::theme::text_primary(),
                    SolveZone(SolveZoneKind::Focal),
                );
                crate::present::spawn_line(
                    rail,
                    fonts,
                    "",
                    crate::theme::FONT_HEADING,
                    crate::theme::text_primary(),
                    SolveZone(SolveZoneKind::Timer),
                );
                crate::present::spawn_line(
                    rail,
                    fonts,
                    "",
                    crate::theme::FONT_TITLE,
                    crate::theme::ok(),
                    SolveZone(SolveZoneKind::Result),
                );
                crate::present::spawn_line(
                    rail,
                    fonts,
                    "",
                    crate::theme::FONT_HEADING,
                    crate::theme::text_primary(),
                    SolveZone(SolveZoneKind::Splits),
                );
                crate::present::spawn_line(
                    rail,
                    fonts,
                    "",
                    crate::theme::FONT_SMALL,
                    crate::theme::danger(),
                    SolveZone(SolveZoneKind::Broken),
                );
                crate::present::spawn_line(
                    rail,
                    fonts,
                    "",
                    crate::theme::FONT_SMALL,
                    crate::theme::text_muted(),
                    SolveZone(SolveZoneKind::Hint),
                );
            });
    });
    content
}

pub fn bind_text(
    phase: Res<State<SolvePhase>>,
    session: Res<SolveSession>,
    mut zones: Query<(&SolveZone, &mut Text, &mut TextColor)>,
) {
    if !session.is_changed() && !phase.is_changed() {
        return;
    }
    let current = *phase.get();
    let (result, splits, broken) = review_parts(&session);
    for (zone, mut text, mut color) in &mut zones {
        match zone.0 {
            SolveZoneKind::Phase => *text = Text::new(phase_label(current, &session)),
            SolveZoneKind::Focal => *text = Text::new(focal_text(current, &session)),
            SolveZoneKind::Timer => *text = Text::new(timer_text(current, &session)),
            SolveZoneKind::Splits => *text = Text::new(splits.clone()),
            SolveZoneKind::Broken => *text = Text::new(broken.clone()),
            SolveZoneKind::Hint => *text = Text::new(hint_text(current).to_string()),
            SolveZoneKind::Result => {
                *text = Text::new(result.clone());
                color.0 = if result == "solved" {
                    crate::theme::ok()
                } else {
                    crate::theme::danger()
                };
            }
        }
    }
}
